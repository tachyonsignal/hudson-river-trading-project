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

void test_basic() {
  const char *inputFile = "test_artifacts/test_basic.in";
  const char *outputFile = "test_artifacts/test_result.out";
  const char *expectedFile = "test_artifacts/test_expected.out";

  constexpr int currentDate = 19700102;
  Parser myParser(currentDate, std::string(outputFile));

  int fd = openFile(inputFile);
  read(myParser, fd);

  AddOrder addOrder;
  std::fstream fh;
  fh.open(outputFile, std::fstream::in | std::fstream::binary);
  fh.read((char*)&addOrder.msgType, sizeof(addOrder.msgType));
  fh.read((char*)&addOrder.msgSize, sizeof(addOrder.msgSize));
  fh.read((char*)&addOrder.ticker, sizeof(addOrder.ticker));
  fh.read((char*)&addOrder.timestamp, sizeof(addOrder.timestamp));
  fh.read((char*)&addOrder.orderRef, sizeof(addOrder.orderRef));
  fh.read((char*)&addOrder.side, sizeof(addOrder.side));
  fh.read((char*)&addOrder.padding, sizeof(addOrder.padding));
  fh.read((char*)&addOrder.size, sizeof(addOrder.size));
  fh.read((char*)&addOrder.price, sizeof(addOrder.price));

  fh.close();

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

  close(fd);
}

int main(int argc, char **argv) {
  test_basic();

  return 0;
}