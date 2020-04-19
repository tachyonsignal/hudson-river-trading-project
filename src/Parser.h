#pragma once

#include <string>
#include <queue>          // std::queue
#include <unordered_map>  // std::unordered_map

struct Order_t {
  char a;
  char b;
  char c;
  char d;
  char e;
  char f;
  char g;
  char h;
};
class Parser {
  // The next desired packet in sequence. 
  int pos;
  // Accumulated sequence of bytes spread across between adjacent packets.
  // Processed/Flushed when a complete order message is formed.
  std::queue<char> q;
  // Remembers packets that arrive "early" / out of sequence.
  std::unordered_map<uint16_t, const char*> m;
  std::unordered_map<unsigned long long, Order_t> orders;

  char* mapAdd(char* in);
  char* mapExecuted(char* in);
  char* mapReduced(char* in);
  char* mapReplaced(char* in);

  // Get the order dictionary key given the buffer, and offset to
  // start looking for the key.
  unsigned long long getOrderRef(char *in, int offset);

  // Map alphanumeric character.
  char mapAscii(char c);

  char* popNBytes(int n);

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
