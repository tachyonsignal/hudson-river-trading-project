#include "Parser.h"

#include <string>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdexcept>
#include <fstream>

Parser::Parser(int date, const std::string &outputFilename)
{
    pos = 1;
    filename = outputFilename;

    // Empty the file.
    std::ofstream outfile;
    outfile.open(outputFilename);
    outfile.close();
}

void Parser::onUDPPacket(const char *buf, size_t len)
{
    printf("Received packet of size %zu\n", len);
    if(static_cast<int>(len) < 4) {
       throw std::runtime_error("Packet must be atleast 4 bytes");
    }

    uint16_t packetSize = 0;
    packetSize = (uint16_t)((buf[0] << 8) | buf[1]);
    printf("Packet size %zu\n", packetSize);
    if(static_cast<int>(packetSize) != static_cast<int>(len)) {
       throw std::runtime_error("Packet size does match buffer length");
    }
    uint16_t sequenceNumber = (uint16_t)((
        buf[2] << 24) | buf[3] << 16 | buf[4] << 8 | buf[5]);
    printf("Sequence number %zu\n", sequenceNumber);

    int payloadLength = static_cast<int>(len) - 6;
    printf("Payload length %zu\n", payloadLength);

    if(sequenceNumber == pos) {
       // Queue payload bytes of current payload.
       for( int i = 6; i < static_cast<int>(len); i++) {
           q.push(buf[i]);
       }
       pos++;

       // Queue payload bytes of previously skipped payloads.
       auto payload = m.find(pos);
       while(payload != m.end()) {
          const char* forwardBuf = payload->second;
          uint16_t prevPacketSize  = (uint16_t)((forwardBuf[0] << 8) | forwardBuf[1]);
          for( int i = 6; i < static_cast<int>(prevPacketSize); i++) {
            q.push(forwardBuf[i]);
          }
          pos++;
          payload = m.find(pos);
       }

       std::ofstream outfile;
       outfile.open(filename, std::ios_base::app); // append instead of overwrite
           
       // There's atleast 1 message that is processable.
       while(q.size() >= 34 ||
            (q.size() >= 33 && q.front() != 'A') ||
            (q.size() >= 21 && (q.front() == 'X' || q.front() == 'R'))) {
         char msgType = q.front();
         if(msgType == 'A') {
           char* in = popNBytes(34);
           char* out = mapAdd(in);
           delete[] in;
           outfile.write(out, 44);
           delete[] out;
         } else if(msgType == 'E') {
           char* in = popNBytes(21);
           char* out = mapExecuted(in);
           outfile.write(out, 40);
           delete[] in, delete[] out;
         } else if(msgType == 'X') {
           char* in = popNBytes(21);
           char* out = mapReduced(in);
           outfile.write(out, 32);
           delete[] in, delete[] out;
         } else if(msgType == 'R') {
           char* in = popNBytes(33);
           char* out = mapReplaced(in);
           outfile.write(out, 48);
           delete[] in, delete[] out;
         } else {
           throw std::runtime_error("Unexpected message type");
         }
       }
       outfile.close();
    } else if (sequenceNumber < pos) {
      // Duplicate packet that has already proccessed, ignore.
    } else {
      // Store for when the gap in packet sequence is closed.
      m[sequenceNumber] = buf;
    }
}

unsigned long long Parser::getUint64(char *in, int offset) {
 unsigned long long orderRef = (
    (unsigned long long)(in[offset]) << 54 |
    (unsigned long long)(in[offset+1]) << 48 |
    (unsigned long long)(in[offset+2]) << 40 |
    (unsigned long long)(in[offset+3]) << 32 |
    (unsigned long long)(in[offset+4]) << 24 |
    (unsigned long long)(in[offset+5]) << 16 |
    (unsigned long long)(in[offset+6]) << 8 |
    (unsigned long long)(in[offset+7]));
  return orderRef;
}
unsigned int Parser::getUint32(char *in, int offset) {
  return (
    (unsigned int)(in[offset]) << 24 |
    (unsigned int)(in[offset+1]) << 16 |
    (unsigned int)(in[offset+2]) << 8 |
    (unsigned int)(in[offset+3]));
}

char* Parser::mapAdd(char *in) {
  char* out = new char[44];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x01;
  // Msg size. Offset 2, length 2.
  out[2] = 0x00, out[3] = 0x2C;

  // Stock Ticker. Offset 4, length 8.
  char* ticker = new char[8];
  for(int i = 0; i < 8; i++) {
    char c = in[22 + i];
    c == ' ' ? '\0': c; // Replace space with null.
    out[4+i] = c;
    ticker[i] = c;
  }

  // Timestamp. Offset 12, length 8.

  // Order reference number. Offset 20, length 8.
  unsigned long long orderRef = getUint64(in, 9);
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20+i] = orderRefLittleEndianBytes[i];
  }

  // Side. Offset 28, length 1.
  out[28] = in[17];

  // Padding. Offset 29, length 3.
  for(int i = 29; i <= 31; i++) {
    out[i] = 0x00;
  }

  // Size. Offset 32, length 4.
  unsigned int sizeInt = getUint32(in, 18);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[32 + i] = sizeBytes[i];
  }

  // Price. Offset 36, length 8.
  int32_t priceInt = getUint32(in, 30);
  double priceDouble = double(priceInt);
  char* priceBytes = reinterpret_cast<char*>(&priceDouble);
  for(int i = 0 ; i < 8; i++) {
    out[36 + i] = priceBytes[i];
  }

  orders[orderRef] = {
    ticker,
    priceDouble, 
    sizeInt
  };

  return out;
}

Order_t Parser::lookupOrder(unsigned long long orderRef) {
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    // TODO: Throw with order ref #.
    throw std::runtime_error("Order ref was not found.");
  }
  return order->second;
}

char* Parser::mapExecuted(char *in) {
  char* out = new char[40];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x02;
  // Msg size. Offset 2, length 2.
  out[2] = 0x00, out[3] = 0x28;

  // Lookup add order using order ref.
  unsigned long long orderRef = getUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4 + i] = o.ticker[i];
  }

  // TODO: timestamp. offset 12, length 8. map.

  // Order reference number. Offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i] = orderRefLittleEndianBytes[0];
  }

  // Size. Offset 28, length 4.
  unsigned int sizeInt = getUint32(in, 17);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  }

  char* priceBytes = reinterpret_cast<char*>(&o.price);
  for(int i = 0 ; i < 8; i++) {
    // On x86, the bytes of the double are in little-endian order.
    out[32 + i] = priceBytes[i];
  }

  return out;
}

char* Parser::mapReduced(char* in) {
  char* out = new char[32];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x03;
    // Msg size. Offset 2, length 2.
  out[2] = 0x00, out[3] = 0x20;

  // Lookup add order using order ref.
  unsigned long long orderRef = getUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o.ticker[i];
  }

  // Timestamp, offset 12, length 8.
  // TODO: map from *in.

  // Order ref number. Offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i ] = orderRefLittleEndianBytes[i];
  }
  
  // Size remaining. Offset 28, length 4.
  unsigned int sizeInt = getUint32(in, 18);
  unsigned int remainingSize = o.size - sizeInt;
  o.size = remainingSize;
  char* sizeBytes = reinterpret_cast<char*>(&remainingSize);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  }
  return out;
}

char* Parser::mapReplaced(char *in) {
  char* out = new char[48];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x04;
  // Msg size. Offset 2, length 2.
  out[2] = 0x00, out[3] = 0x30;
  
  // Lookup add order using order ref.
  unsigned long long orderRef = getUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o.ticker[i];
  }

  // TODO: timestamp. offset 12, length 8, map timestamp from *in.
  // Older order reference number. offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i] = orderRefLittleEndianBytes[i];
  }

  // New order reference number. offset 28, length 8.
  unsigned long long newOrderRef = getUint64(in, 17);
  char* newOrderRefBytes = reinterpret_cast<char*>(&newOrderRef);
  for(int i = 0 ; i < 8; i++) {
    out[28] = newOrderRefBytes[i];
  }

  // New size. Offset 36, length 4.
  unsigned int sizeInt = getUint32(in, 25);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[36 + i] = sizeBytes[i];
  }

  int32_t priceInt = getUint32(in, 29);
  double priceDouble = double(priceInt);
  char* priceBytes = reinterpret_cast<char*>(&priceDouble);
  // On x86, the bytes of the double are in little-endian order.
  for(int i = 0 ; i < 8 ; i++) {
    out[40 + i] = priceBytes[i];
  }

  return out;
}

char* Parser::popNBytes(int n) {
  char* out = new char[n];
  for(int i = 0 ; i < n; i++) {
    out[i] = q.front();
    q.pop();
  }
  return out;
}