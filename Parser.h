#pragma once

#include <string>
#include <queue>          // std::queue
#include <unordered_map>  // std::unordered_map

typedef char msgsymbol_t;
typedef char msgtype_t[2];
typedef char ticker_t[8];
typedef char side_t;
typedef char padding_t[3];

struct PendingOrder_t {
  // Ticker characters, with spaces replaced by nul.
  char * ticker;
  double price;
  uint32_t sizeRemaining;
};

struct InputAddOrder {
  msgsymbol_t msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  side_t side;
  uint32_t size;
  ticker_t ticker;
  int32_t price;
};

struct InputOrderExecuted {
  msgsymbol_t msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
};

struct InputOrderCanceled {
  msgsymbol_t msgType;
  uint64_t timestamp;
  uint64_t orderRef;
  uint32_t size;
};

struct InputOrderReplaced {
  msgsymbol_t msgType;
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

  // Deserializes input buffer into the input message struct.
  void deserializeAddOrder(char* in, InputAddOrder* msg);
  void deserializeOrderExecuted(char* in, InputOrderExecuted* msg);
  void deserializeOrderCanceled(char* in, InputOrderCanceled* msg);
  void deserializeOrderReplaced(char* in, InputOrderReplaced* msg);

  // Serializes input struct to buffer for output struct.
  void serializeAddOrder(char** outPtr, InputAddOrder inputMsg);
  void serializeOrderExecuted(char** outPtr, InputOrderExecuted inputMsg);
  void serializeOrderReduced( char** outPtr, InputOrderCanceled inputMsg);
  void serializeOrderReplaced(char** outPtr, InputOrderReplaced inputMsg);

  // Utilities to interpret bytes starting at given offset in buffer.
  uint64_t readBigEndianUint64(const char *buf, int offset);
  uint32_t readBigEndianUint32(const char *buf, int offset);
  uint16_t readBigEndianUint16(const char *buf, int offset);

  // Sub-routines of #onUDPPacket.
  // Enqueue packets that arrived early if sequence has since connected. 
  void catchupSequencePayloads();
  // Seeks fully received input messages and writes output messages to file. 
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
