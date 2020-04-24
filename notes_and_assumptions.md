## Assumptions

- Min packet size >=6, payload >=0 bytes. This is 
  reflected by MIN_PACKET_SIZE variable.
- duplicate packets have the same content. the current behavior 
  is to ignore subsequent packets.
- packet size is consistent with claimed packet size
- there is barebones YYYYMMDD validation as I did not want to 
  re-implement a date library or familiar enough with C++
  ecosystem to know what's in vogue
- Memory will accumulate as number of new order refs increase.
  We can't cleanup without knowing whether they'll be referenced
  by a future message.
- There are no time-complexity constraints on the onUDDPPacket method.
  Because of the nature of packets arriving early, and must be 
  queued up, when a continuous sequence of packets is ready for 
  processing, they will be flushed all at once.
- 'A', 'C', X', 'R' message types are specified. The code will throw
  otherwise.
- Order Refs referenced by Canceled, Replaced, Executed must correspond
  to existing order via an Add or replaced. The code will throw 
  "throw std::runtime_error("Unexpected message type"); otherwise
- See test plan for more assumptions. To name a few:
    - Order Executed affects remaining amount of shares that are cancellable
    - Size cannot go negative. However, this implementation will round
      to 0 if execution size exceeds remaining. Same with cancellations.   

## Environment Setup

1. Used Azure Visual Studio Online.
2. Provisioned a 8 core 16gb system
3. Checked x86_64

   ```
   $ uname -m
    x86_64
   ``` 

4. Checked gcc

   ```
   $ gcc -v
   gcc version 6.3.0 20170516 (Debian 6.3.0-18+deb9u1) 
   ```

5. Checked g++
   
   ```
   $ g++ --v
   gcc version 6.3.0 20170516 (Debian 6.3.0-18+deb9u1) 
   ```

6. Question: gcc == g++?
  
  - G++ treats C++ as a superset of C and compiles C as C++
  - g++ as a linker will link c++ libraries


## System Programming notes

### Big endian vs little endian

### UDP

- TCP makes a handshake.
- Resend loss packets
- UDP just one way data stream, doesn't care if received

### Make

Makefile has "targets" or alias or rules

```
make libparser.a
```

Produces Parser.o, and libparser.a

```
make
```

Shortcut for 

```
make feed
```

Which produces: Parser.o, libparsr.a, feed