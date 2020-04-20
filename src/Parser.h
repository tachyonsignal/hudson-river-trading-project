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
  std::unordered_map<uint16_t, const char*> m;

  // Track Add Orders and their remaining order size.
  std::unordered_map<unsigned long long, Order_t> orders;
  // Utility to abstract away unordered_map boilerplate, lacking ::contains.
  Order_t lookupOrder(unsigned long long orderRef);

  // Functions that map from input to output message.
  char* mapAdd(char* in);
  char* mapExecuted(char* in);
  char* mapReduced(char* in);
  char* mapReplaced(char* in);

  // Utilities to interpret bytes starting at given offset.
  unsigned long long getUint64(const char *in, int offset);
  unsigned int getUint32(const char *in, int offset);

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
