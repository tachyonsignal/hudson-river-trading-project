#pragma once

#include <string>
#include <queue>          // std::queue
#include <unordered_map>  // std::unordered_map

// typedef typedef char ticker[8];

struct PendingOrder_t {
  // Ticker characters, with spaces replaced by nul.
  char * ticker;
  double price;
  uint32_t sizeRemaining;
};

struct InputAddOrder {
  char msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  char side;
  uint32_t size;
  char ticker[8];
  int32_t price;
};

struct InputOrderExecuted {
  char msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
};

struct InputOrderCanceled {
  char msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
};

struct InputOrderReplaced {
  char msgType;
  uint64_t timestamp;
  uint64_t originalOrderRef;
  uint64_t newOrderRef;
  uint32_t size;
  double price;
};

class Parser {
  // Sequence number of the next Packet that is ready for processing.
  uint32_t sequencePosition;
  // The file to write to.
  std::string filename;
  uint64_t epochToMidnightLocalNanos;

  // Payload bytes that have arrived in order but not yet processed.
  std::queue<char> q;

  // Utility to grab a chunk of bytes off the queue and into buf.
  char* popNBytes(int n, char** buf);

  // Stash packets that arrive "early" / out of sequence, keyed by seq number.
  std::unordered_map<uint16_t, const char*> earlyPackets;

  // Track Add Orders and their remaining order size.
  std::unordered_map<uint64_t, PendingOrder_t> orders;
  // Utility to abstract away unordered_map boilerplate, lacking ::contains.
  PendingOrder_t* lookupOrder(uint64_t orderRef);

  void deserializeInputAddOrder(char* in, InputAddOrder* msg);
  void deserializeInputOrderExecuted(char* in, InputOrderExecuted* msg);
  void deserializeInputOrderCanceled(char* in, InputOrderCanceled* msg);
  void deserializeInputOrderReplaced(char* in, InputOrderReplaced* msg);

  // Functions that map message from input to output message in second buffer.
  void mapAdd(char** outPtr, InputAddOrder inputMsg);
  void mapExecuted(char** outPtr, InputOrderExecuted inputMsg);
  void mapReduced( char** outPtr, InputOrderCanceled inputMsg);
  void mapReplaced(char** outPtr, InputOrderReplaced inputMsg);

  // Utilities to interpret bytes starting at given offset in buffer.
  uint64_t readBigEndianUint64(const char *buf, int offset);
  uint32_t readBigEndianUint32(const char *buf, int offset);
  uint16_t readBigEndianUint16(const char *buf, int offset);

  // Sub-routines of #onUDPPacket.
  // Enqueue packets that arrived early if sequence has since connected. 
  void catchupSequencePayloads();
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
