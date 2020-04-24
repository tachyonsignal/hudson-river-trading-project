#include "Parser.h"

#include <fstream>
#include <iostream>
#include <cstring>
#include <assert.h>     /* assert */


struct OutputAddOrder { 
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

struct OutputOrderExecuted { 
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
  double price;
}; 

struct OutputOrderReduced {
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t sizeRemaining;
};

struct OutputOrderReplaced {
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t oldOrderRef;
  uint64_t newOrderRef;
  uint32_t newSize;
  double newPrice;
};


const char SPACE_CHAR = ' ';
const char NUL_CHAR = '\0';

const char PADDING[] = {0x00,0x00,0x00};
const char MSG_TYPE_ADD = 'A';
const char MSG_TYPE_EXECUTE = 'E';
const char MSG_TYPE_CANCEL = 'X';
const char MSG_TYPE_REPLACE = 'R';
const char MSG_TYPE_1[] = { 0x00, 0x01 };
const char MSG_TYPE_2[] = { 0x00, 0x02 };
const char MSG_TYPE_3[] = { 0x00, 0x03 };
const char MSG_TYPE_4[] = { 0x00, 0x04 };

const char INPUT_ADD_PAYLOAD_SIZE = 34;
const char INPUT_EXECUTE_PAYLOAD_SIZE = 21;
const char INPUT_CANCEL_PAYLOAD_SIZE = 21; 
const char INPUT_REPLACE_PAYLOAD_SIZE = 33; 

const char OUTPUT_ADD_PAYLOAD_SIZE = 44;
const char OUTPUT_EXECUTE_PAYLOAD_SIZE = 40;
const char OUTPUT_CANCEL_PAYLOAD_SIZE = 32;
const char OUTPUT_REPLACE_PAYLOAD_SIZE = 48;

const char MAX_INPUT_PAYLOAD_SIZE = 34;
const char MIN_INPUT_PAYLOAD_SIZE = 21;

const char MAX_OUTPUT_PAYLOAD_SIZE = 48;


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
  
  // Reuse buffer to store current serialized input / output messages.
  char* in = new char[MAX_INPUT_PAYLOAD_SIZE];
  char* out = new char[MAX_OUTPUT_PAYLOAD_SIZE];

  // There's atleast 1 complete message in the queue.
  while((q.size() >= INPUT_ADD_PAYLOAD_SIZE && q.front() == MSG_TYPE_ADD) ||
      (q.size() >= INPUT_REPLACE_PAYLOAD_SIZE && q.front() == MSG_TYPE_REPLACE) ||
      (q.size() >= INPUT_EXECUTE_PAYLOAD_SIZE && q.front() == MSG_TYPE_CANCEL) ||
      (q.size() >= INPUT_EXECUTE_PAYLOAD_SIZE && q.front() == MSG_TYPE_EXECUTE)) {
    switch(q.front()) {
      case MSG_TYPE_ADD:
        popNBytes(INPUT_ADD_PAYLOAD_SIZE, &in);
        InputAddOrder inputAddOrder;
        deserializeAddOrder(in, &inputAddOrder);
        serializeAddOrder(&out, inputAddOrder);
        outfile.write(out, OUTPUT_ADD_PAYLOAD_SIZE);
        break;
      case MSG_TYPE_EXECUTE:
        popNBytes(INPUT_EXECUTE_PAYLOAD_SIZE, &in);
        InputOrderExecuted inputOrderExecuted;
        deserializeOrderExecuted(in, &inputOrderExecuted);
        serializeOrderExecuted(&out, inputOrderExecuted);
        outfile.write(out, OUTPUT_EXECUTE_PAYLOAD_SIZE);
        break;
      case MSG_TYPE_CANCEL:
        popNBytes(INPUT_CANCEL_PAYLOAD_SIZE, &in);
        InputOrderCanceled inputOrderCanceled;
        deserializeOrderCanceled(in, &inputOrderCanceled);
        serializeOrderReduced(&out, inputOrderCanceled);
        outfile.write(out, OUTPUT_CANCEL_PAYLOAD_SIZE);
        break;
      case MSG_TYPE_REPLACE:
        popNBytes(INPUT_REPLACE_PAYLOAD_SIZE, &in);
        InputOrderReplaced inputOrderReplaced;
        deserializeOrderReplaced(in, &inputOrderReplaced);
        serializeOrderReplaced(&out, inputOrderReplaced);
        outfile.write(out, OUTPUT_REPLACE_PAYLOAD_SIZE);
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

void Parser::deserializeAddOrder(char* in, InputAddOrder* msg) {
  msg->msgType = MSG_TYPE_ADD;
  msg->timestamp = readBigEndianUint64(in, 1);
  msg->orderRef = readBigEndianUint64(in, 9);        
  msg->side = in[17];
  msg->size = readBigEndianUint32(in, 18);
  memcpy(msg->ticker, &in[22], 8);
  msg->price = readBigEndianUint32(in, 30);
}

void Parser::deserializeOrderExecuted(char* in, InputOrderExecuted* msg) {
  msg->msgType = MSG_TYPE_EXECUTE; 
  msg->timestamp = readBigEndianUint64(in, 1);
  msg->orderRef = readBigEndianUint64(in, 9);
  msg->size = readBigEndianUint32(in, 17);
}
void Parser::deserializeOrderCanceled(char* in, InputOrderCanceled* msg) {
  msg->msgType = MSG_TYPE_CANCEL;
  msg->timestamp = readBigEndianUint64(in, 1);
  msg->orderRef = readBigEndianUint64(in, 9);
  msg->size = readBigEndianUint32(in, 17);
}
void Parser::deserializeOrderReplaced(char* in, InputOrderReplaced* msg) {
  msg->msgType = MSG_TYPE_REPLACE;
  msg->timestamp = readBigEndianUint64(in, 1);
  msg->originalOrderRef = readBigEndianUint64(in, 9);
  msg->newOrderRef = readBigEndianUint64(in, 17);
  msg->size = readBigEndianUint32(in, 25);
  msg->price = readBigEndianUint32(in, 29);
}

void Parser::serializeAddOrder(char ** outPtr, InputAddOrder inputMsg) {
  OutputAddOrder order;
  char * out = *outPtr;

  memcpy(order.msgType, MSG_TYPE_1, 2);

  order.msgSize = OUTPUT_ADD_PAYLOAD_SIZE;

  memcpy(order.ticker, inputMsg.ticker, sizeof(inputMsg.ticker));
  for(int i = 0 ; i < 8; i++) {
    if(order.ticker[i] == SPACE_CHAR) {
      order.ticker[i] = NUL_CHAR;
    }
  }

  order.timestamp = epochToMidnightLocalNanos + inputMsg.timestamp;

  order.orderRef = inputMsg.orderRef;

  order.side = inputMsg.side;

  memcpy(order.padding, PADDING, 3);

  order.size = inputMsg.size;
  order.price = double(inputMsg.price);

  // Must individually copy fields since compiler may add
  // padding between struct fields.
  memcpy(out, order.msgType, sizeof(order.msgType));
  memcpy(&out[2], &order.msgSize, sizeof(order.msgSize));
  memcpy(&out[4], &order.ticker, sizeof(order.ticker));
  memcpy(&out[12], &order.timestamp, sizeof(order.timestamp));
  memcpy(&out[20], &order.orderRef, sizeof(order.orderRef));
  memcpy(&out[28], &order.side, sizeof(order.side));
  memcpy(&out[31], &order.padding, sizeof(order.padding));
  memcpy(&out[32], &order.size, sizeof(order.size));
  memcpy(&out[36], &order.price, sizeof(order.price));

  char * ticker = new char[8];
  memcpy(ticker, order.ticker, 8);
  orders[order.orderRef] = {
    ticker,
    order.price, 
    order.size
  };
}

PendingOrder_t* Parser::lookupOrder(uint64_t orderRef) {
  auto order  = orders.find(orderRef);
  if(order == orders.end()) {
    throw std::runtime_error("Order ref was not found: " +  std::to_string(orderRef));
  }
  return &(order->second);
}

void Parser::serializeOrderExecuted(char** outPtr, InputOrderExecuted inputMsg) {
  OutputOrderExecuted order;
  char* out = *outPtr;
  
  memcpy(order.msgType, MSG_TYPE_2, 2);

  order.msgSize = OUTPUT_EXECUTE_PAYLOAD_SIZE;

  order.orderRef = inputMsg.orderRef;
  PendingOrder_t* pendingOrder = lookupOrder(inputMsg.orderRef);
  memcpy(order.ticker, pendingOrder->ticker, 8);

  order.timestamp = epochToMidnightLocalNanos + inputMsg.timestamp;

  uint32_t executionSize = inputMsg.size;
  // Can execute at most the remaining size.
  if(executionSize > pendingOrder->sizeRemaining) {
    executionSize = pendingOrder->sizeRemaining;
  }
  pendingOrder->sizeRemaining -= executionSize;
  order.size = executionSize;

  order.price = pendingOrder->price;

  memcpy(out, order.msgType, sizeof(order.msgType));
  memcpy(&out[2], &order.msgSize, sizeof(order.msgSize));
  memcpy(&out[4], &order.ticker, sizeof(order.ticker));
  memcpy(&out[12], &order.timestamp, sizeof(order.timestamp));
  memcpy(&out[20], &order.orderRef, sizeof(order.orderRef));
  memcpy(&out[28], &order.size, sizeof(order.size));
  memcpy(&out[32], &order.price, sizeof(order.price));
}

void Parser::serializeOrderReduced(char** outPtr, InputOrderCanceled inputMsg) {
  OutputOrderReduced order;
  char* out = *outPtr;

  memcpy(order.msgType, MSG_TYPE_3, 2);

  order.msgSize = OUTPUT_CANCEL_PAYLOAD_SIZE;

  order.orderRef = inputMsg.orderRef;
  PendingOrder_t* pendingOrder = lookupOrder(inputMsg.orderRef);
  memcpy(order.ticker, pendingOrder->ticker, 8);

  order.timestamp = epochToMidnightLocalNanos + inputMsg.timestamp;
  
  uint32_t sizeRemaining = (inputMsg.size > pendingOrder->sizeRemaining ? 
      0 : pendingOrder->sizeRemaining - inputMsg.size);
  pendingOrder->sizeRemaining = sizeRemaining;
  order.sizeRemaining = sizeRemaining;

  memcpy(out, order.msgType, sizeof(order.msgType));
  memcpy(&out[2], &order.msgSize, sizeof(order.msgSize));
  memcpy(&out[4], &order.ticker, sizeof(order.ticker));
  memcpy(&out[12], &order.timestamp, sizeof(order.timestamp));
  memcpy(&out[20], &order.orderRef, sizeof(order.orderRef));
  memcpy(&out[28], &order.sizeRemaining, sizeof(order.sizeRemaining));
}

void Parser::serializeOrderReplaced(char ** outPtr, InputOrderReplaced inputMsg) {
  OutputOrderReplaced order;
  char* out = *outPtr;

  assert(sizeof(order.msgType) == sizeof(MSG_TYPE_4));
  memcpy(order.msgType, MSG_TYPE_4, sizeof(MSG_TYPE_4));

  order.msgSize = OUTPUT_REPLACE_PAYLOAD_SIZE;

  order.oldOrderRef = inputMsg.originalOrderRef;
  PendingOrder_t* pendingOrder = lookupOrder(inputMsg.originalOrderRef);
  pendingOrder->sizeRemaining = 0;
  assert(sizeof(order.ticker) == sizeof(pendingOrder->ticker));
  memcpy(order.ticker, pendingOrder->ticker, sizeof(order.ticker));

  order.timestamp = epochToMidnightLocalNanos + inputMsg.timestamp;

  order.newOrderRef = inputMsg.newOrderRef;

  order.newSize = inputMsg.size;

  double price = double(inputMsg.price);
  order.newPrice = price;
  
  char * ticker = new char[8];
  memcpy(ticker, pendingOrder->ticker, 8);
  orders[order.newOrderRef] = {
    ticker,
    order.newPrice,
    order.newSize
  };

  memcpy(out, order.msgType, sizeof(order.msgType));
  memcpy(&out[2], &order.msgSize, sizeof(order.msgSize));
  memcpy(&out[4], &order.ticker, sizeof(order.ticker));
  memcpy(&out[12], &order.timestamp, sizeof(order.timestamp));
  memcpy(&out[20], &order.oldOrderRef, sizeof(order.oldOrderRef));
  memcpy(&out[28], &order.newOrderRef, sizeof(order.newOrderRef));
  memcpy(&out[36], &order.newSize, sizeof(order.newSize));
  memcpy(&out[40], &order.newPrice, sizeof(order.newPrice));
}

char* Parser::popNBytes(int n, char** buf) {
  for(int i = 0 ; i < n; i++) {
    (*buf)[i] = q.front();
    q.pop();
  }
}