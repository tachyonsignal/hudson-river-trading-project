#include "Parser.h"

#include <cstdio>

#include <cstdint>

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <iostream>
#include <fstream>
#include <assert.h>     /* assert */
#include <cmath>        // std::abs

// https://stackoverflow.com/questions/2193544/how-to-print-additional-information-when-assert-fails
#define ASSERT_EQUALS(left,right) { if(!((left) == (right))){ std::cerr << "ASSERT FAILED: " << #left << "==" << #right << " @ " << __FILE__ << " (" << __LINE__ << "). " << #left << "=" << (left) << "; " << #right << "=" << (right) << std::endl; } }

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


int openFile(const char* inputFile) {
  int fd = open(inputFile, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Couldn't open %s\n", inputFile);
    return 1;
  }
  return fd;
}

void read(Parser myParser, int fd) {
  char bigbuf[5000];
  while (read(fd, bigbuf, 2) != 0) {
    uint16_t packetSize = htons(*(uint16_t *)bigbuf);
    read(fd, bigbuf + 2, packetSize - 2);

    myParser.onUDPPacket(bigbuf, packetSize);
  }
}

void readAddOrder(std::fstream &fh, AddOrder &addOrder) {
  fh.read((char*)&addOrder.msgType, sizeof(addOrder.msgType));
  fh.read((char*)&addOrder.msgSize, sizeof(addOrder.msgSize));
  fh.read((char*)&addOrder.ticker, sizeof(addOrder.ticker));
  fh.read((char*)&addOrder.timestamp, sizeof(addOrder.timestamp));
  fh.read((char*)&addOrder.orderRef, sizeof(addOrder.orderRef));
  fh.read((char*)&addOrder.side, sizeof(addOrder.side));
  fh.read((char*)&addOrder.padding, sizeof(addOrder.padding));
  fh.read((char*)&addOrder.size, sizeof(addOrder.size));
  fh.read((char*)&addOrder.price, sizeof(addOrder.price));
}

void readExecutedOrder(std::fstream &fh, ExecutedOrder &executedOrder) {
  fh.read((char*)&executedOrder.msgType, sizeof(executedOrder.msgType));
  fh.read((char*)&executedOrder.msgSize, sizeof(executedOrder.msgSize));
  fh.read((char*)&executedOrder.ticker, sizeof(executedOrder.ticker));
  fh.read((char*)&executedOrder.timestamp, sizeof(executedOrder.timestamp));
  fh.read((char*)&executedOrder.orderRef, sizeof(executedOrder.orderRef));
  fh.read((char*)&executedOrder.size, sizeof(executedOrder.size));
  fh.read((char*)&executedOrder.price, sizeof(executedOrder.price));
}

void test_basic() {
  const char *inputFile = "test_artifacts/test_basic.in";
  const char *outputFile = "test_artifacts/test_result.out";
  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);

  ASSERT_EQUALS(addOrder.msgType[0], 0x00);
  ASSERT_EQUALS(addOrder.msgType[1], 0x01);
  ASSERT_EQUALS(addOrder.msgSize, 44);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, addOrder.ticker));
  ASSERT_EQUALS(addOrder.timestamp, 86401123456789L);
  ASSERT_EQUALS(addOrder.orderRef, 1);
  ASSERT_EQUALS(addOrder.side, 'B');
  const char * expectedPadding = "\0\0\0";
  assert(std::equal(expectedPadding, expectedPadding+3, addOrder.padding));
  ASSERT_EQUALS(addOrder.size, 100);
  ASSERT_EQUALS(addOrder.price, -128.0);

  AddOrder addOrder2;
  readAddOrder(fh, addOrder2);
  ASSERT_EQUALS(addOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(addOrder2.msgType[1], 0x01);
  ASSERT_EQUALS(addOrder2.msgSize, 44);
  const char * expectedTicker2 = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker2, expectedTicker2+8, addOrder2.ticker));
  ASSERT_EQUALS(addOrder2.timestamp, 86402123456789L);
  ASSERT_EQUALS(addOrder2.orderRef, 2);
  ASSERT_EQUALS(addOrder2.side, 'B');
  const char * expectedPadding2 = "\0\0\0";
  assert(std::equal(expectedPadding2, expectedPadding2+3, addOrder2.padding));
  ASSERT_EQUALS(addOrder2.size, 100);
  ASSERT_EQUALS(addOrder2.price, -128.0);

  fh.close();
  close(fd);
}

void test_add_execute() {
 const char *inputFile = "test_artifacts/test_add_execute.in";
  const char *outputFile = "test_artifacts/test_add_execute.out";
  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);

  ASSERT_EQUALS(addOrder.msgType[0], 0x00);
  ASSERT_EQUALS(addOrder.msgType[1], 0x01);
  ASSERT_EQUALS(addOrder.msgSize, 44);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, addOrder.ticker));
  ASSERT_EQUALS(addOrder.timestamp, 86401123456789L);
  ASSERT_EQUALS(addOrder.orderRef, 1);
  ASSERT_EQUALS(addOrder.side, 'B');
  const char * expectedPadding = "\0\0\0";
  assert(std::equal(expectedPadding, expectedPadding+3, addOrder.padding));
  ASSERT_EQUALS(addOrder.size, 100);
  ASSERT_EQUALS(addOrder.price, -128.0);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 1);
  ASSERT_EQUALS(executedOrder.size, 49);
  ASSERT_EQUALS(executedOrder.price, -128.0);
}

int main(int argc, char **argv) {
  // test_basic();
  test_add_execute();

  return 0;
}