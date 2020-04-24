#include "Parser.h"

#include <fstream>
#include <iostream>
#include <cstring>

struct AddOrder { 
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t orderRef;
  char side;
  char padding[3];
  uint32_t size;
  double price;
}; 


struct ExecutedOrder { 
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
  double price;
}; 


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
  timeinfo->tm_year = ( date / 1E4) - 1900; // Years since 1900.
  timeinfo->tm_mon = month - 1; // Months are 0-indexed.
  timeinfo->tm_mday = day;
  timeinfo->tm_hour = 0;
  timeinfo->tm_min = 0;
  timeinfo->tm_sec = 0;
  uint32_t epochToMidnightLocalSeconds = mktime (timeinfo);
  epochToMidnightLocalNanos = 1000000000 * (uint64_t) epochToMidnightLocalSeconds;
  // Empty the file.
  std::ofstream outfile;
  outfile.open(outputFilename);
  outfile.close();
}

void Parser::catchupSequencePayloads() {
  // Lookup if there is a packet succeeding the sequence that
  // arrived early.
  auto entry = earlyPackets.find(sequencePosition);
  while(entry != earlyPackets.end()) {
    const char* bytes = entry->second;
    uint16_t packetSize = readBigEndianUint16(bytes, 0);
    // Enqueue the payload.
    for( int i = 6; i < packetSize; i++) {
      q.push(bytes[i]);
    }

    sequencePosition++;
    entry = earlyPackets.find(sequencePosition);
  }
}

void Parser::processQueue() {
  std::ofstream outfile;
  outfile.open(filename, std::ios_base::app); // append instead of overwrite
  
  // Reuse buffer to store current input message.
  char* in = new char[34];
  char* out = new char[48];

  // There's atleast 1 complete message in the queue.
  while(q.size() >= 34 ||
      (q.size() >= 33 && q.front() != 'A') ||
      (q.size() >= 21 && (q.front() == 'X' || q.front() == 'E'))) {
    switch(q.front()) {
      case 'A':
        popNBytes(34, &in);
        mapAdd(in, &out);
        outfile.write(out, 44);
        break;
      case 'E':
        popNBytes(21, &in);
        mapExecuted(in, &out);
        outfile.write(out, 40);
        break;
      case 'X':
        popNBytes(21, &in);
        mapReduced(in, &out);
        outfile.write(out, 32);
        break;
      case 'R':
        popNBytes(33, &in);
        mapReplaced(in, &out);
        outfile.write(out, 48);
        break;
      default:
        throw std::runtime_error("Unexpected message type");
    }
  }

  delete[] in, delete[] out, outfile.close();
}

void Parser::onUDPPacket(const char *buffer, size_t len) {
  printf("Received packet of size %zu\n", len);
  if(static_cast<int>(len) < 6) {
      throw std::invalid_argument("Packet must be atleast 6 bytes");
  }

  // Copy the buffer since may be mutated.
  char *buf = new char[len];
  for(int i = 0; i < static_cast<int>(len); i++) {
    buf[i] = buffer[i];
  }

  uint16_t packetSize = readBigEndianUint16(buf, 0);
  if(static_cast<int>(packetSize) != static_cast<int>(len)) {
    throw std::invalid_argument("Packet size does match buffer length.");
  }
  
  uint32_t sequenceNumber = readBigEndianUint32(buf, 2);

  // Packet arrived "early", stash for later.
  if (sequenceNumber > sequencePosition) {
    earlyPackets[sequenceNumber] = buf; 
    return;
  } else if (sequenceNumber < sequencePosition) {
    // Packet already arrived and processed.
    return;
  }

  // Enqueue payload of current packet.
  for( int i = 6; i < static_cast<int>(len); i++) {
    q.push(buf[i]);
  }
  sequencePosition++;

  // Catchup with packets continue sequence, but arrived early.
  catchupSequencePayloads();

  // Map bytes / messages that are queued.
  processQueue();
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
uint32_t Parser::readBigEndianUint32(const char *in, int offset) {
  return (
    (uint32_t)((uint8_t)in[offset]) << 24 |
    (uint32_t)((uint8_t)in[offset+1]) << 16 |
    (uint32_t)((uint8_t)in[offset+2]) << 8 |
    (uint32_t)((uint8_t)in[offset+3]));
}

uint16_t Parser::readBigEndianUint16(const char *buf, int offset) {
  return (
    (uint16_t)((uint8_t)buf[offset]) << 8 |
    (uint16_t)((uint8_t)buf[offset+1]));
}

void Parser::mapAdd(const char *in, char ** outPtr) {
  AddOrder a;
  char * out = *outPtr;

  char msgType[] = { 0x00, 0x01 };
  memcpy(a.msgType, msgType, 2);

  a.msgSize = 44;

  char* ticker = new char[8];
  for(int i = 0; i < 8; i++) {
    char c = in[22 + i];
    ticker[i] = c == ' ' ? '\0': c;
  }
  memcpy(a.ticker, ticker, 8);
  memcpy(out, &a, 12);

  // Timestamp field. 
  uint64_t timestamp = readBigEndianUint64(in, 1);
  uint64_t nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
  a.timestamp = nanosSinceEpoch;

  uint64_t orderRef = readBigEndianUint64(in, 9);
  a.orderRef = orderRef;

  a.side = in[17];

  char padding[] = { 0x00, 0x00, 0x00} ;
  memcpy(a.padding, padding, 3);

  uint32_t size = readBigEndianUint32(in, 18);
  a.size = size;

  double price = double(readBigEndianUint32(in, 30));
  a.price = price;

  // Must individually copy fields since compiler may add
  // padding between struct fields.
  memcpy(out, a.msgType, sizeof(a.msgType));
  memcpy(&out[2], &a.msgSize, sizeof(a.msgSize));
  memcpy(&out[4], &a.ticker, sizeof(a.ticker));
  memcpy(&out[12], &a.timestamp, sizeof(a.timestamp));
  memcpy(&out[20], &a.orderRef, sizeof(a.orderRef));
  memcpy(&out[28], &a.side, sizeof(a.side));
  memcpy(&out[31], &a.padding, sizeof(a.padding));
  memcpy(&out[32], &a.size, sizeof(a.size));
  memcpy(&out[36], &a.price, sizeof(a.price));

  orders[orderRef] = {
    ticker,
    price, 
    size
  };
}

Order_t* Parser::lookupOrder(uint64_t orderRef) {
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    throw std::runtime_error("Order ref was not found: " +  std::to_string(orderRef));
  }
  return &(order->second);
}

void Parser::mapExecuted(const char *in, char** outPtr) {
  char* out = *outPtr;
  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x02;
  // Msg size. Offset 2, length 2.
  out[2] = 0x28, out[3] = 0x00;

  // Lookup add order using order ref.
  uint64_t orderRef = readBigEndianUint64(in, 9);
  Order_t* o = lookupOrder(orderRef);
  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4 + i] = o->ticker[i];
  }
  
  uint64_t timestamp = readBigEndianUint64(in, 1);
  uint64_t nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
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
  uint32_t sizeInt = readBigEndianUint32(in, 17);
  // Update remaining order size.
  if(sizeInt > o->size) {
    // Can execute at most the remaining size.
    sizeInt = o->size;
  }
  uint32_t remainingSize = o->size - sizeInt;
  o->size =  remainingSize;
  char* sizeBytes = reinterpret_cast<char*>(&sizeInt);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  } 

  char* priceBytes = reinterpret_cast<char*>(&o->price);
  for(int i = 0 ; i < 8; i++) {
    // On x86, the bytes of the double are in little-endian order.
    out[32 + i] = priceBytes[i];
  }
}

void Parser::mapReduced(const char* in, char** outPtr) {
  char* out = *outPtr;

  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x03;
    // Msg size. Offset 2, length 2.
  out[2] = 0x20, out[3] = 0x00;

  // Lookup add order using order ref.
  uint64_t orderRef = readBigEndianUint64(in, 9);
  Order_t* o = lookupOrder(orderRef);
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o->ticker[i];
  }

  // Timestamp, offset 12, length 8.
  uint64_t timestamp = readBigEndianUint64(in, 1);
  uint64_t nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
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
  uint32_t sizeInt = readBigEndianUint32(in, 17);
  uint32_t remainingSize = sizeInt > o->size ? 0 : o->size - sizeInt;
  o->size = remainingSize;

  char* sizeBytes = reinterpret_cast<char*>(&remainingSize);
  for(int i = 0 ; i < 4; i++) {
    out[28 + i] = sizeBytes[i];
  }
}

void Parser::mapReplaced(const char *in, char ** outPtr) {
  char* out = *outPtr;

  // Msg type. Offset 0, length 2.
  out[0] = 0x00, out[1] = 0x04;
  // Msg size. Offset 2, length 2.
  out[2] = 0x30, out[3] = 0x00;
  
  // Lookup add order using order ref.
  uint64_t orderRef = readBigEndianUint64(in, 9);
  Order_t* o = lookupOrder(orderRef);
  // Order's size should go to 0.
  o->size = 0;

  // Stock ticker. Offset 4, length 8.
  for(int i = 0 ; i < 8; i++) {
    out[4+i] = o->ticker[i];
  }

  uint64_t timestamp = readBigEndianUint64(in, 1);
  uint64_t nanosSinceEpoch = epochToMidnightLocalNanos + timestamp; 
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
  uint64_t newOrderRef = readBigEndianUint64(in, 17);
  char* newOrderRefBytes = reinterpret_cast<char*>(&newOrderRef);
  for(int i = 0 ; i < 8; i++) {
    out[28 + i] = newOrderRefBytes[i];
  }

  // New size. Offset 36, length 4.
  uint32_t size = readBigEndianUint32(in, 25);
  char* sizeBytes = reinterpret_cast<char*>(&size);
  for(int i = 0 ; i < 4; i++) {
    out[36 + i] = sizeBytes[i];
  }

  double price = double(readBigEndianUint32(in, 29));
  char* priceBytes = reinterpret_cast<char*>(&price);
  // On x86, the bytes of the double are in little-endian order.
  for(int i = 0 ; i < 8 ; i++) {
    out[40 + i] = priceBytes[i];
  }

  orders[newOrderRef] = {
    o->ticker,
    price,
    size
  };
}

char* Parser::popNBytes(int n, char** buf) {
  for(int i = 0 ; i < n; i++) {
    (*buf)[i] = q.front();
    q.pop();
  }
}