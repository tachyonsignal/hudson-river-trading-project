#include "Parser.h"

#include <string>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdexcept>


// Buffer of size 64
// Index of packet waiting to write

Parser::Parser(int date, const std::string &outputFilename)
{
    pos = 1;

    int fd = open(outputFilename.c_str(), O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Couldn't open file '%s'.\n", outputFilename.c_str());
    }
}

void Parser::onUDPPacket(const char *buf, size_t len)
{
    printf("Received packet of size %zu\n", len);
    if(static_cast<int>(len) < 4) {
       throw std::runtime_error("Packet must be atleast 4 bytes");
    }

    uint16_t packetSize = 0;
    packetSize = (uint16_t)((buf[0] << 8) | buf[1]);
    printf("Packet size %zu\n", packetSize);
    if(static_cast<int>(packetSize) != static_cast<int>(len)) {
       throw std::runtime_error("Packet size does match buffer length");
    }
    uint16_t sequenceNumber = (uint16_t)((
        buf[2] << 24) | buf[3] << 16 | buf[4] << 8 | buf[5]);
    printf("Sequence number %zu\n", sequenceNumber);

    int payloadLength = static_cast<int>(len) - 6;
    printf("Payload length %zu\n", payloadLength);

    if(sequenceNumber == pos) {
       // Queue current payload.
       for( int i = 6; i < static_cast<int>(len); i++) {
           q.push(buf[i]);
       }
       pos++;

       // Queue previously skipped payloads.
       auto payload = m.find(pos);
       while(payload != m.end()) {
          uint16_t prevPacketSize  = (uint16_t)((buf[0] << 8) | buf[1]);
          for( int i = 6; i < static_cast<int>(prevPacketSize); i++) {
            q.push(buf[i]);
          }
          pos++;
          payload = m.find(pos);
       }
    } else if (sequenceNumber < pos) {
      // Skip, already proccessed
    } else {
      // Store for revisit until the gap between sequence is closed.
      m[sequenceNumber] = buf;
    }
}

