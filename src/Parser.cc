#include "Parser.h"

#include <fstream>
#include <iostream>

Parser::Parser(int date, const std::string &outputFilename) {
  filename = outputFilename;
  
  // "The first packet processed by your parser should be
  // the packet with sequence number 1."
  sequencePosition = 1;

  int year = date / 10000;
  if(year < 1970) {
    throw std::invalid_argument("YYYY must be between 1970 and 2105.");
  }
  int month = date % 10000 / 100;
  if(month > 12 || month == 0) {
    throw std::invalid_argument("MM must be between 1 and 12 inclusive.");
  }
  int day = date % 100;
  if(day > 31 || day == 0) {
    throw std::invalid_argument("DD must be between 1 and 31 inclusive.");
  }
  // Copied from http://www.cplusplus.com/reference/ctime/mktime/.
  time_t rawtime;
  struct tm * timeinfo = localtime ( &rawtime );
  std::cout << timeinfo << "\n";
  timeinfo->tm_year = ( date / 1E4) - 1900; // Years since 1900.
  timeinfo->tm_mon = month - 1; // Months are 0-indexed.
  timeinfo->tm_mday = day;
  timeinfo->tm_hour = 0;
  timeinfo->tm_min = 0;
  timeinfo->tm_sec = 0;
  unsigned int epochToMidnightLocalSeconds = mktime (timeinfo);
  epochToMidnightLocalNanos = 1000000000 * (unsigned long long) epochToMidnightLocalSeconds;
  // Empty the file.
  std::ofstream outfile;
  outfile.open(outputFilename);
  outfile.close();
}

void Parser::enqueuePayloads(const char *buf, size_t len) {
  // Payload of current packet can be queued for processing.
  printf("Payload length %zu\n", static_cast<int>(len) - 6);
  for( int i = 6; i < static_cast<int>(len); i++) {
    q.push(buf[i]);
  }
  sequencePosition++;

  // Payload bytes of previously skipped packets 
  // succeed the sequence and can be queued.
  auto entry = packets.find(sequencePosition);
  while(entry != packets.end()) {
    const char* skippedPacketBytes = entry->second;
    int skippedPacketSize = readBigEndianUint16(skippedPacketBytes, 0);
    for( int i = 6; i < skippedPacketSize; i++) {
      q.push(skippedPacketBytes[i]);
    }
    sequencePosition++;
    entry = packets.find(sequencePosition);
  }
}

void Parser::processQueue() {
  std::ofstream outfile;
  outfile.open(filename, std::ios_base::app); // append instead of overwrite
  
  printf("processQueue\n");
  printf("%d", q.size());
  printf("%d", q.front());

  // There's atleast 1 complete message in the queue.
  while(q.size() >= 34 ||
      (q.size() >= 33 && q.front() != 'A') ||
      (q.size() >= 21 && (q.front() == 'X' || q.front() == 'E'))) {
    char msgType = q.front();
    printf("msgType %X\n", msgType);
    if(msgType == 'A') {
      char* in = popNBytes(34);
      char* out = mapAdd(in);
      outfile.write(out, 44);
      delete[] in, delete[] out;
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
}

void Parser::onUDPPacket(const char *buf, size_t len) {
  printf("Received packet of size %zu\n", len);
  if(static_cast<int>(len) < 6) {
      throw std::invalid_argument("Packet must be atleast 6 bytes");
  }

  uint16_t packetSize = readBigEndianUint16(buf, 0);
  if(static_cast<int>(packetSize) != static_cast<int>(len)) {
      throw std::invalid_argument("Packet size does match buffer length.");
  }
  
  unsigned int sequenceNumber = readBigEndianUint32(buf, 2);
  printf("Packet sequence number %zu.\n", sequenceNumber);
  // Packet arrived "early".
  if (sequenceNumber > sequencePosition) {
    // Store for when the gap in packet sequence is closed.
    packets[sequenceNumber] = buf;
  } else if (sequenceNumber < sequencePosition) {
    // Packet already arrived and processed.
    return;
  } else {
    enqueuePayloads(buf, len);
    processQueue();
  }
}

uint64_t Parser::readBigEndianUint64(const char *in, int offset) {
 return (
    ((uint64_t)(uint8_t)in[offset] << 54) +
    ((uint64_t)(uint8_t)in[offset+1] << 48) +
    ((uint64_t)(uint8_t)in[offset+2] << 40) +
    ((uint64_t)(uint8_t)in[offset+3] << 32) + 
    ((uint64_t)(uint8_t)in[offset+4] << 24) +
    ((uint64_t)(uint8_t)in[offset+5] << 16) +
    ((uint64_t)(uint8_t)in[offset+6] << 8) +
    ((uint64_t)(uint8_t)in[offset+7]));
}
unsigned int Parser::readBigEndianUint32(const char *in, int offset) {
  return (
    (unsigned int)(in[offset]) << 24 |
    (unsigned int)(in[offset+1]) << 16 |
    (unsigned int)(in[offset+2]) << 8 |
    (unsigned int)(in[offset+3]));
}

uint16_t Parser::readBigEndianUint16(const char *buf, int offset) {
  return (
    (uint16_t)(buf[offset]) << 8 |
    (uint16_t)(buf[offset+1]));
}

char* Parser::mapAdd(const char *in) {
  printf("mapAdd\n");
  // Bytes associated with output message.
  char* out = new char[44];

  // Msg Type field.
  out[0] = 0x00, out[1] = 0x01;
  // Msg Size field.
  out[2] = 0x2C, out[3] = 0x00;

  // Copy Stock Ticker Ascii bytes with space replaced by null.
  // Create buffer of mapped Ascii bytes for store in Order struct.
  char* ticker = new char[8];
  for(int i = 0; i < 8; i++) {
    char c = in[22 + i];
     // Replace space with null.
    c = c == ' ' ? '\0': c;

    ticker[i] = c;
    out[ 4 + i] = c;
  }

  // Timestamp field. 
  unsigned long long timestamp = readBigEndianUint64(in, 1);
  unsigned long long nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 

  char* timeLittleEndianBytes = reinterpret_cast<char*>(&nanosSinceEpoch);
  for(int i = 0 ; i < 8; i++) {
    out[12+i] = timeLittleEndianBytes[i];
  }

  // Copy Order Reference Number, write in little endian.
  unsigned long long orderRef = readBigEndianUint64(in, 9);
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i] = orderRefLittleEndianBytes[i];
  }

  // Map ASCII value for Side (either 'B' or 'S').
  out[28] = in[17];

  // Padding. 
  for(int i = 29; i <= 31; i++) {
    out[i] = 0x00;
  }

  // Size. Offset 32, length 4.
  unsigned int sizeInt = readBigEndianUint32(in, 18);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[32 + i] = sizeBytes[i];
  }

  // Price. Offset 36, length 8.
  int32_t priceInt = readBigEndianUint32(in, 30);
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

char* Parser::mapExecuted(const char *in) {
  printf("mapExecuted\n");

  char* out = new char[40];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x02;
  // Msg size. Offset 2, length 2.
  out[2] = 0x28, out[3] = 0x00;

  // Lookup add order using order ref.
  unsigned long long orderRef = readBigEndianUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4 + i] = o.ticker[i];
  }

  unsigned long long timestamp = readBigEndianUint64(in, 1);
  unsigned long long nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
  char* timeBytes = reinterpret_cast<char*>(&nanosSinceEpoch);
  for(int i = 0 ; i < 8; i++) {
    out[12+i] = timeBytes[i];
  }

  // Order reference number. Offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i] = orderRefLittleEndianBytes[i];
  }

  // Size. Offset 28, length 4.
  unsigned int sizeInt = readBigEndianUint32(in, 17);
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

char* Parser::mapReduced(const char* in) {
  char* out = new char[32];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x03;
    // Msg size. Offset 2, length 2.
  out[2] = 0x20, out[3] = 0x00;

  // Lookup add order using order ref.
  unsigned long long orderRef = readBigEndianUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o.ticker[i];
  }

  // Timestamp, offset 12, length 8.
  unsigned long long timestamp = readBigEndianUint64(in, 1);
  unsigned long long nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
  char* timeBytes = reinterpret_cast<char*>(&nanosSinceEpoch);
  for(int i = 0 ; i < 8; i++) {
    out[12+i] = timeBytes[i];
  }

  // Order ref number. Offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i ] = orderRefLittleEndianBytes[i];
  }
  
  // Size remaining. Offset 28, length 4.
  unsigned int sizeInt = readBigEndianUint32(in, 18);
  unsigned int remainingSize = o.size - sizeInt;
  o.size = remainingSize;
  char* sizeBytes = reinterpret_cast<char*>(&remainingSize);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  }
  return out;
}

char* Parser::mapReplaced(const char *in) {
  char* out = new char[48];
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x04;
  // Msg size. Offset 2, length 2.
  out[2] = 0x30, out[3] = 0x00;
  
  // Lookup add order using order ref.
  unsigned long long orderRef = readBigEndianUint64(in, 9);
  Order_t o = lookupOrder(orderRef);
  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o.ticker[i];
  }

  unsigned long long timestamp = readBigEndianUint64(in, 1);
  unsigned long long nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
  char* timeBytes = reinterpret_cast<char*>(&nanosSinceEpoch);
  for(int i = 0 ; i < 8; i++) {
    out[12+i] = timeBytes[i];
  }

  // Older order reference number. offset 20, length 8.
  char* orderRefLittleEndianBytes = reinterpret_cast<char*>(&orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[20 + i] = orderRefLittleEndianBytes[i];
  }

  // New order reference number. offset 28, length 8.
  unsigned long long newOrderRef = readBigEndianUint64(in, 17);
  char* newOrderRefBytes = reinterpret_cast<char*>(&newOrderRef);
  for(int i = 0 ; i < 8; i++) {
    out[28] = newOrderRefBytes[i];
  }

  // New size. Offset 36, length 4.
  unsigned int sizeInt = readBigEndianUint32(in, 25);
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[36 + i] = sizeBytes[i];
  }

  int32_t priceInt = readBigEndianUint32(in, 29);
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