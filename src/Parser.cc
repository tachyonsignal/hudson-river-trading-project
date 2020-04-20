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
       // Queue current payload.
       for( int i = 6; i < static_cast<int>(len); i++) {
           q.push(buf[i]);
       }
       pos++;

       // Queue previously skipped payloads.
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

       // There's atleast 1 message that is processable.
       while(q.size() >= 34 ||
            (q.size() >= 33 && q.front() != 0x41) ||
            (q.size() >= 21 && q.front() == 0x58 && q.front() == 0x52))
        {
         char msgType = q.front();
         q.pop();

         bool isAdd = msgType == 0x41;
         bool isExecuted = msgType == 0x45;
         bool isCanceled = msgType == 0x58;
         bool isReplaced = msgType == 0x52;

         if(isAdd) {
           char* in = popNBytes(34 - 1);
           char* out = mapAdd(in);
           // TODO: delete[] vs delete.
           delete in;

           std::ofstream outfile;
           outfile.open(filename, std::ios_base::app); // append instead of overwrite
           outfile.write(out, 44);
           outfile.close();

           delete out;
         } else if(isExecuted) {
           char* in = popNBytes(21 - 1);
           char* out = mapExecuted(in);
           delete in;
           std::ofstream outfile;
           outfile.open(filename, std::ios_base::app); // append instead of overwrite
           outfile.write(out, 40);
           outfile.close();
           delete out;
         } else if(isCanceled) {
           char* in = popNBytes(21 - 1);
           char* out = mapReduced(in);
           delete in;
           std::ofstream outfile;
           outfile.open(filename, std::ios_base::app); // append instead of overwrite
           outfile.write(out, 32);
           outfile.close();
           delete out;
         } else if(isReplaced) {
           char* in = popNBytes(33 - 1);
           char* out = mapReplaced(in);
           delete in;
           std::ofstream outfile;
           outfile.open(filename, std::ios_base::app); // append instead of overwrite
           outfile.write(out, 48);
           outfile.close();
           delete out;
         } else {
           throw std::runtime_error("Unexpected message type");
         }
       }
    } else if (sequenceNumber < pos) {
      // Skip, duplicate / already proccessed.
    } else {
      // Store for revisit when the gap in sequence is closed.
      m[sequenceNumber] = buf;
    }
}

char Parser::replaceAsciiSpace(char c) {
  return c == ' ' ? '\0': c;
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
    char c = in[22 - 1 + i];
    // Replace space with null.
    c == ' ' ? '\0': c;
    out[4+i] = c;
    ticker[i] = c;
  }

  // Timestamp. Offset 12, length 8.

  // Order reference number. Offset 20, length 8.
  unsigned long long orderRef = getUint64(in, 9-1);
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20+i] = orderRefLittleEndianBytes[i];
  }

  // Side. Offset 28, length 1.
  out[28] = in[17 - 1];

  // Padding. Offset 29, length 3.
  for(int i = 29; i <= 31; i++) {
    out[i] = 0x00;
  }

  // Size. Offset 32, length 4.
  unsigned int sizeInt = getUint32(in, 18-1);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[32 + i] = sizeBytes[i];
  }

  // Price. Offset 36, length 8.
  int32_t priceInt = getUint32(in, 30 - 1);
  double priceDouble = double(priceInt);
  char* priceBytes = reinterpret_cast<char*>(&priceDouble);
  // On x86, the bytes of the double are already in little-endian order.
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

char* Parser::mapExecuted(char *in) {
  char* out = new char[40];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x02;
  // Msg size. Offset 2, length 2.
  out[2] = 0x00, out[3] = 0x28;

  // Lookup add order using order ref.
  unsigned long long orderRef = getUint64(in, 9-1);
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    // TODO: Throw with order ref #.
    throw std::runtime_error("Order ref was not found.");
  }
  Order_t o = order->second;
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
  unsigned int sizeInt = getUint32(in, 17-1);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  }

  char* priceBytes = reinterpret_cast<char*>(&o.price);
  // On x86, the bytes of the double are in little-endian order.
  for(int i = 0 ; i < 8; i++) {
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
  unsigned long long orderRef = getUint64(in, 9-1);
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    // TODO: Throw with order ref #.
    throw std::runtime_error("Order ref was not found.");
  }
  Order_t o = order->second;
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
  unsigned int sizeInt = getUint32(in, 18-1);
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
  out[0] = 0x00;
  out[1] = 0x04;
  // Msg size. Offset 2, length 2.
  out[2] = 0x00;
  out[3] = 0x30;
  
  // Lookup add order using order ref.
  unsigned long long orderRef = getUint64(in, 9-1);
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    // TODO: Throw with order ref #.
    throw std::runtime_error("Order ref was not found.");
  }
  Order_t o = order->second;
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
  out[28] = in[24-1];
  out[29] = in[23-1];
  out[30] = in[22-1];
  out[31] = in[21-1];
  out[32] = in[20-1];
  out[33] = in[19-1];
  out[34] = in[18-1];
  out[35] = in[17-1];
  // New size. Offset 36, length 4.
  unsigned int sizeInt = getUint32(in, 25-1);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[36 + i] = sizeBytes[i];
  }

  int32_t priceInt = getUint32(in, 29-1);
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