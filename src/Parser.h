#pragma once

#include <string>
#include <queue>          // std::queue
#include <unordered_map>  // std::unordered_map

struct Order_t {
  char * ticker;
  double price;
  unsigned int size;
};

class Parser {
  // Sequence number of the next Packet whose payload can be queued.
  unsigned int sequencePosition;
  // The file to write to.
  std::string filename;
  unsigned long long epochToMidnightLocalNanos;

  // Payload bytes that have been received but not yet processed.
  std::queue<char> q;
  // Utility to grab a chunk of bytes off the queue.
  char* popNBytes(int n);

  // Stash packets that arrive "early" / out of sequence, keyed by seq number.
  std::unordered_map<uint16_t, const char*> packets;

  // Track Add Orders and their remaining order size.
  std::unordered_map<unsigned long long, Order_t> orders;
  // Utility to abstract away unordered_map boilerplate, lacking ::contains.
  Order_t lookupOrder(unsigned long long orderRef);

  // Functions that map from input to output message.
  char* mapAdd(const char* in);
  char* mapExecuted(const char* in);
  char* mapReduced(const char* in);
  char* mapReplaced(const char* in);

  // Utilities to interpret bytes starting at given offset in buffer.
  uint64_t readBigEndianUint64(const char *buf, int offset);
  unsigned int readBigEndianUint32(const char *buf, int offset);
  uint16_t readBigEndianUint16(const char *buf, int offset);

  // Sub-routines of #onUDPPacket.
  // Enqueue payload bytes, catching up sequence of skipped packets.
  void enqueuePayloads(const char *buf, size_t len);
  void processQueue();

  public:
    // date - the day on which the data being parsed was generated.
    // It is specified as an integer in the form yyyymmdd.
    // For instance, 18 Jun 2018 would be specified as 20180618.
    //
    // outputFilename - name of the file output events should be written to.
    Parser(int date, const std::string &outputFilename);

    // buf - points to a char buffer containing bytes from a single UDP packet.
    // len - length of the packet.
    void onUDPPacket(const char *buf, size_t len);
};
