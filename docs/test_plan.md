# Test plan

## Messages / Mapping
The simplest tests would involve
testing each message type and its mapping in isolation.

This could be subdivided into subcases for the different fields.

## Basic dependent sequences
Testing messages in isolation is not entirely feasible as some message types depend on the sequence of message types leading up to it. The unit test would need to establish the state up to this point. This would involve either fiddling with the internal data structure state, 
or interacting with the Parser as a black box and inserting preceding events.

## Packets vs Payload

The packets are merely carriers of the payload. My implementation somewhat decouples the two, and the 
interchange format is a queue of unprocess bytes. Once the bytes are in the queue, information about the packets from which they are originated is loss. 

For testing of business logic behavior, it suffices to have the sequence of messages in a single a packet. Alternatively, there could
be one packet for each message, and the packets would arrive in sequential order.

Testing that packets are handled out of order, that duplicates are skipped, and that messages straddling 2 or more packets can be handled would involve a few short message sequences, but testing super long sequences of messages would be distracting and better reserved for the message-based tests.

### One-message, many packets
I would test a message straddling 2 packets, maybe 3 packets, and inductively assume if it works for a message straddling n packets, it would for a message straddling n+1;

### One packet, many messages

This is self-explanatory.

## N-packets, n-messages, unaligned straddling

You can have a message straddling 
n-packets, with the 1st or n-th packet be started or ended by a different message, respectively.

packet | 0111111112222
message| abb..abcc....

This should be tested, but I'd focus 
on the core edge cases below since 
longer sequences with n > 3 messages will just be recombinations.

packet  |i   | i  ... i+k | i+k
----------------------------
message |a   | b  ...  b  | c

packet  |i   | i  ... i+k | i+k+1
----------------------------
message |a   | b  ...  b  | c

packet  |i-1 | i  ... i+k | i+k
----------------------------
message |a   | b  ...  b  | c

packet  |i-1 | i  ... i+k |i+k+1
----------------------------
message |a   | b  ...  b  | c

### Empty Packets

There should be permutation of existing test with packets with 
no payload.

## Exceptional cases / assumptions

- Every packet is atleast 6 bytes, 
  and must contain the header, while
  the payload may be 0 bytes.
- The date integer is 8 digits
- YYYY >= 1900
- 1 <= MM <= 12
- 0 <= DD <= 31

## I/O Redirection

The current interface allows client specify a filename, and the contract is to write to this file.

A more flexible interface would accept an I/O stream. Then clients and test
can specify where to output data, namely in-memory and assert against. With the current implementation, the tests would have to check files.