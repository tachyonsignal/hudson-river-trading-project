# Environment Setup

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


# System Programming notes

## Big endian vs little endian

## UDP

- TCP makes a handshake.
- Resend loss packets
- UDP just one way data stream, doesn't care if received

## Make

Makefile has "targets" or alias or rules

```
make libparser.a
```

Produces Parser.o, and libparser.a

