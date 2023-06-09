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

using namespace std;

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

struct ReducedOrder {
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t sizeRemaining;
};

struct ReplacedOrder {
  char msgType[2];
  uint16_t msgSize;
  char ticker[8];
  uint64_t timestamp;
  uint64_t oldOrderRef;
  uint64_t newOrderRef;
  uint32_t newSize;
  double newPrice;
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

void readReducedOrder(std::fstream &fh, ReducedOrder &reducedOrder) {
  fh.read((char*)&reducedOrder.msgType, sizeof(reducedOrder.msgType));
  fh.read((char*)&reducedOrder.msgSize, sizeof(reducedOrder.msgSize));
  fh.read((char*)&reducedOrder.ticker, sizeof(reducedOrder.ticker));
  fh.read((char*)&reducedOrder.timestamp, sizeof(reducedOrder.timestamp));
  fh.read((char*)&reducedOrder.orderRef, sizeof(reducedOrder.orderRef));
  fh.read((char*)&reducedOrder.sizeRemaining, sizeof(reducedOrder.sizeRemaining));
}

void readReplacedOrder(std::fstream &fh, ReplacedOrder &replacedOrder) {
  fh.read((char*)&replacedOrder.msgType, sizeof(replacedOrder.msgType));
  fh.read((char*)&replacedOrder.msgSize, sizeof(replacedOrder.msgSize));
  fh.read((char*)&replacedOrder.ticker, sizeof(replacedOrder.ticker));
  fh.read((char*)&replacedOrder.timestamp, sizeof(replacedOrder.timestamp));
  fh.read((char*)&replacedOrder.oldOrderRef, sizeof(replacedOrder.oldOrderRef));
  fh.read((char*)&replacedOrder.newOrderRef, sizeof(replacedOrder.newOrderRef));
  fh.read((char*)&replacedOrder.newSize, sizeof(replacedOrder.newSize));
  fh.read((char*)&replacedOrder.newPrice, sizeof(replacedOrder.newPrice));
}

void test_add_add() {
  const char *inputFile = "test_input/AA.in";
  const char *outputFile = "test_output/AA.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

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
  ASSERT_EQUALS(addOrder.price, 2000000.0);

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
  ASSERT_EQUALS(addOrder2.price, 2000000.0);

  fh.close();
}

void test_add_execute() {
 const char *inputFile = "test_input/AE.in";
  const char *outputFile = "test_output/AE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

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
  ASSERT_EQUALS(addOrder.price, 2000000.0);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 1);
  ASSERT_EQUALS(executedOrder.size, 49);
  ASSERT_EQUALS(executedOrder.price, 2000000.0);
  fh.close();
}

void test_add_canceled() {
 const char *inputFile = "test_input/AC.in";
  const char *outputFile = "test_output/AC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);


  AddOrder addOrder;
  readAddOrder(fh, addOrder);

  ASSERT_EQUALS(addOrder.size, 100);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder.msgSize, 32);
  ASSERT_EQUALS(reducedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder.orderRef, 1);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 52);

  fh.close();
}


void test_add_canceled_surplus() {
 const char *inputFile = "test_input/AC_exceeding_size.in";
  const char *outputFile = "test_output/AC_exceeding_size.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);


  AddOrder addOrder;
  readAddOrder(fh, addOrder);

  ASSERT_EQUALS(addOrder.size, 100);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 0);

  fh.close();
}

void test_add_executed_canceled() {
 const char *inputFile = "test_input/AEC.in";
  const char *outputFile = "test_output/AEC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.size, 49);
  
  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 3);

  fh.close();
}

void test_add_executed_surplus() {
 const char *inputFile = "test_input/AE_exceeding_size.in";
  const char *outputFile = "test_output/AE_exceeding_size.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.size, 100);
  
  fh.close();
}

void test_add_executed_executed() {
 const char *inputFile = "test_input/AEE.in";
  const char *outputFile = "test_output/AEE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.size, 98);

  ExecutedOrder executedOrder2;
  readExecutedOrder(fh, executedOrder2);
  ASSERT_EQUALS(executedOrder2.size, 2);
  
  fh.close();
}

void test_add_canceled_executed() {
 const char *inputFile = "test_input/ACE.in";
  const char *outputFile = "test_output/ACE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 4);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.size, 4);

  fh.close();
}


void test_add_canceled_canceled() {
  // TODO: Split test input / output directorys.
  const char *inputFile = "test_input/ACC.in";
  const char *outputFile = "test_output/ACC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);

  ASSERT_EQUALS(addOrder.size, 100);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder.msgSize, 32);
  ASSERT_EQUALS(reducedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder.orderRef, 1);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 52);

  ReducedOrder reducedOrder2;
  readReducedOrder(fh, reducedOrder2);
  ASSERT_EQUALS(reducedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder2.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder2.msgSize, 32);
  ASSERT_EQUALS(reducedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder2.orderRef, 1);
  ASSERT_EQUALS(reducedOrder2.sizeRemaining, 4);

  fh.close();
}

void test_add_replaced() {
  const char *inputFile = "test_input/AR.in";
  const char *outputFile = "test_output/AR.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  fh.close();
}

void test_replaced_canceled() {
  const char *inputFile = "test_input/RC.in";
  const char *outputFile = "test_output/RC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder.msgSize, 32);
  ASSERT_EQUALS(reducedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder.orderRef, 2);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 4294967247);

  fh.close();
}

void test_replaced_executed() {
  const char *inputFile = "test_input/RE.in";
  const char *outputFile = "test_output/RE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 255);
  ASSERT_EQUALS(replacedOrder.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 2);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}

void test_replaced_replaced() {
  const char *inputFile = "test_input/RR.in";
  const char *outputFile = "test_output/RR.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  fh.close();
}

void test_add_replaced_canceled() {
  const char *inputFile = "test_input/ARC.in";
  const char *outputFile = "test_output/ARC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder.msgSize, 32);
  ASSERT_EQUALS(reducedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder.orderRef, 1);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 0);

  fh.close();
}

void test_replaced_replaced_executed() {
  const char *inputFile = "test_input/RRE.in";
  const char *outputFile = "test_output/RRE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 3);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}

void test_replaced_replaced_canceled() {
  const char *inputFile = "test_input/RRC.in";
  const char *outputFile = "test_output/RRC.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ReducedOrder reducedOrder;
  readReducedOrder(fh, reducedOrder);
  ASSERT_EQUALS(reducedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(reducedOrder.msgType[1], 0x03);
  ASSERT_EQUALS(reducedOrder.msgSize, 32);
  ASSERT_EQUALS(reducedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(reducedOrder.orderRef, 3);
  ASSERT_EQUALS(reducedOrder.sizeRemaining, 207);

  fh.close();
}

void test_add_replaced_replaced_executed_single_packet() {
  const char *inputFile = "test_input/RRE.in";
  const char *outputFile = "test_output/RRE.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 3);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}

void test_add_replaced_replaced_executed_straddled() {
  const char *inputFile = "test_input/ARRE_straddled.in";
  const char *outputFile = "test_output/ARRE_straddled.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);

  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 3);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}

void test_add_replaced_replaced_executed_out_of_order() {
  const char *inputFile = "test_input/ARRE_out_of_order.in";
  const char *outputFile = "test_output/ARRE_out_of_order.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);
  
  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 3);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}

void test_add_replaced_replaced_executed_straddled_out_of_order() {
  const char *inputFile = "test_input/ARRE_straddled_out_of_order.in";
  const char *outputFile = "test_output/ARRE_straddled_out_of_order.out";

  int fd = openFile(inputFile);
  Parser myParser(19700102, std::string(outputFile));
  read(myParser, fd);
  close(fd);

  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);
  
  AddOrder addOrder;
  readAddOrder(fh, addOrder);
  ASSERT_EQUALS(addOrder.size, 100);

  ReplacedOrder replacedOrder;
  readReplacedOrder(fh, replacedOrder);
  ASSERT_EQUALS(replacedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder.msgSize, 48);
  ASSERT_EQUALS(replacedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder.oldOrderRef, 1);
  ASSERT_EQUALS(replacedOrder.newOrderRef, 2);
  ASSERT_EQUALS(replacedOrder.newSize, 4294967295);
  ASSERT_EQUALS(replacedOrder.newPrice - 2.147483647e+09, 0);

  ReplacedOrder replacedOrder2;
  readReplacedOrder(fh, replacedOrder2);
  ASSERT_EQUALS(replacedOrder2.msgType[0], 0x00);
  ASSERT_EQUALS(replacedOrder2.msgType[1], 0x04);
  ASSERT_EQUALS(replacedOrder2.msgSize, 48);
  ASSERT_EQUALS(replacedOrder2.timestamp, 86402123456789);
  ASSERT_EQUALS(replacedOrder2.oldOrderRef, 2);
  ASSERT_EQUALS(replacedOrder2.newOrderRef, 3);
  ASSERT_EQUALS(replacedOrder2.newSize, 255);
  ASSERT_EQUALS(replacedOrder2.newPrice, 9);

  ExecutedOrder executedOrder;
  readExecutedOrder(fh, executedOrder);
  ASSERT_EQUALS(executedOrder.msgType[0], 0x00);
  ASSERT_EQUALS(executedOrder.msgType[1], 0x02);
  ASSERT_EQUALS(executedOrder.msgSize, 40);
  const char * expectedTicker = "SPY\0\0\0\0\0";
  assert(std::equal(expectedTicker, expectedTicker+8, executedOrder.ticker));
  ASSERT_EQUALS(executedOrder.timestamp, 86402123456789);
  ASSERT_EQUALS(executedOrder.orderRef, 3);
  ASSERT_EQUALS(executedOrder.size, 255);
  ASSERT_EQUALS(executedOrder.price, 9);

  fh.close();
}


int main(int argc, char **argv) {
  if (mkdir("./test_output", 0755) != 0) {
    cout << "Please create a directory ./test_output first." << endl;
    return -1;
  }

  // Test messages
  test_add_add();
  test_add_execute();
  test_add_canceled();
  test_add_canceled_canceled();
  test_add_canceled_surplus();
  test_add_executed_canceled();
  test_add_executed_surplus();
  test_add_executed_executed();
  test_add_canceled_executed();
  test_add_replaced();
  test_add_replaced_canceled();
  test_replaced_canceled();
  test_replaced_executed();
  test_replaced_replaced();
  test_replaced_replaced_executed();
  test_replaced_replaced_canceled();

  // // Test packets.
  test_add_replaced_replaced_executed_single_packet();
  test_add_replaced_replaced_executed_out_of_order();
  test_add_replaced_replaced_executed_straddled_out_of_order();

  return 0;
}