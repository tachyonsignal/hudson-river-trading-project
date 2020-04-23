# Test plan

## Minimum viable message sequences

The most elementary test cases involve testing the four message types
and their mappings.

For Add order messages, this is straightforward. For Executed, Canceled
and Replaced orders, they hinge on a preceding Add Order. To test these three
order types, thus, the most basic sequences are:

- A
- AE
- AC
- AR

## Pure function mappings vs state

Some fields are direct mappings from the input field. Some fields depend on 
prior messages, but also as a straightforward map.  Then there is the 
Reduced order's "Size remaining field" which depends on the size of the
Add Order. If the original size is 100, and 30 is canceled, the Reduced
Order message's size is 100-30=70.

For pure mappings, the checklist to test would be:
- existence of all fields, in correct positions / offsets
- numeric values correctly map, in little endianness
- input with alphanumeric field containing space is replaced with nul
- message sizes are correctly aligned, and is consistent with msg type field
- fixed and known date selected for the parser constructor such that the mapped
  timestamp value is calculable
- test 2^32-1 (maxed unsigned int) for unsigned long fields, namely size.
- Test -1, 0, 1, 2^31 − 1, and other boundary values related to two's complement and    negative numbers. 
- For situations involving arithmetic on the size value, use combinations of special
  numeric values like the two's complemenet boundary. Also in cases where the result
  of the arithmetic crosses the the boundary. For example, adding two numbers less than 2^32 but whose sum might exceed it (not applicable here), or subtracting numbers such
  that the result is a large negative magnitude. Effectively, test for overflow
  and wrap around.
- Test with numbers whose little endian and big endian are not palindromes. 
- For prices, the original value is signed and the output is a double. Since the double mapping also involves more have more bytes allocated, there should not be a loss of information. 2^32-1 is once again a great number to test, especially since its decimal digits are relatively diverse. 
  
Other nice to have:
- alphanumeric fields are left-justified, and nul characters are only on the right
  side of sequence

### Assumption: canceled orders have cumulative effects

More generally, the "Size remaining field" depend not on one preceding message
but potentially a sequence. More concretely, if multiple Canceled orders are
issued, the effect is cumulative. Thus, this sequence should be tested:

- ACC

## Size semantics

### Assumption: Order replaced affects canceled/reduced orders
For Order Replaced, "all remaining shares of the original order must be removed".
What would the significance of this be if the new order associated with this
Order Replaced has its size specified in the Order replaced? This requirement
suggests that if an Order is replaced, there would be 0 shares remaining to cancel.

The following input sequence should be tested:

- AR(eplaced)C

The resulting Reduced order would have a remaining size of 0, regardless of
the size it canceled or the initial size in the Add order. The order Replaced would
have reduced the remaining size to 0.

### Corrolary: Canceled Order sizes exceeding prior remaining size will be rounded down

(prior remaining size) - canceled order size would yield a negative value for the 
remaining size other wise. A different form of this sequence should test this:

- AC

### Assumption: Executed orders also reduce remaining size

Orders can't be canceled if they've already been executed. If the original Add order size is 100, 50 is executed, 20 is canceled, then 30 remains. This sequence should be tested:

- AEC

Similarly, canceled orders affect the remaining size that can be executed. This sequence should be tested:

- ACE

### Assumption: canceled orders reduce the size available for Execution

Whereas in the AEC sequence, the remaining size on the Reduced order associated with the Canceled order provides an artifact to test that the E order affected the C, it's
not obvious what the artifact would be for C affecting E in the sequence ACE. Well,
suppose all shares are canceled, there would be no shares left to execute. Hence the 
"Size" field on the output Executed order is not a direct mapping from the input
Executed Order but really, 

Math.min(executed shares, remaining size).

This operation could be tested in isolation from having a C order in the loop, such that
we'd revisit this sequence:

- AE

### Correlary: Replaced order also reduce the size available for Execution

The remaining size would be 0 after an order is replaced, so there 0 shares would be executed. This sequence should be tested:

- ARE

## Sequences starting from a Replaced Order

Previously mentioned sequences start from an Add Order. Replaced, Executed, Canceled
orders can start from a Replaced order. The previous test cases could be forked with 
A replaced with AR. For example:

- A -> AR
- AE -> ARE
- AC -> ARC
- AR -> ARR
- ACC -> ARCC
- ARC -> ARCC
- AEC -> AREC
- ACE -> ARCE
- ARE -> ARRE

Permutations of E and C orders can be between A R, too. I'd test:
E, C, CE, EC, ECC and maybe some of the 3 Order permutations). Take ARRE
sequence from above for example, looping in E and C would yield:

- AERRE
- ACRRE
- ACERRE
- AECRRE
- AECCRRE

I'd just cherry-pick a few of the more complex ones, or auto-generate test cases.
It's not a simple matter of fuzzing/permuting a fixed set of payloads since these sequences involve Order Reference numbers. The generator would have to know to update 
order reference numbers for the sequence of orders starting from a Replaced order.
For Remaining Size calculations, having Cancels and Executes reduce 1 size at a time
would also make the generation easier.

Aside: this kind of reminds me of ACTG base pairs in Biology/genetics.

## Packets vs Payload

Prior discussion did not mention the packets carrying the order
payloads. 

For simplciity, test cases can be implemented by
placing each order in 1 packet, with packets in sequential order.
Packets may contain more than 1 order. So alternatively, the
tests could also encode the entire sequence of orders in a single packet. 

For sake testing a single thing at once, the test cases
focused on business logic of orders should not be bogged
down with the details of the packets. My implementation has
splits these two processes up, and their point of interchange is 
thru a queue of bytes. Given more flexibility, I'd change the Parser
interface or expose this queue of bytes, such that the packets
and orders could be tested separately. In fact, the packets are
just mules and are completely agnostic to the order types...

### Packet straddling
At any rate, orders can straddle packets. The packet would
contain some combination of these properties:

- begin with the partial tail end of an order from previous packet
- does not begin with the partial tail end of an order
- end with partial head end of an order from next packet
- does not end with the partial head of an order

The test cases would be: 
A) start / end with n messages, where n >= 0
B) start with n messages, end with partial head of order
C) start with partial tail, end with n messages
D) start with partial tail, n messages in middle, end with head of order

Values I'd use for n: 0, 1, 2, maybe 3 then inductively assume things work
for larger n, up to some reasonable extent. Then I'd might throw in some
random value of n like 517, but this would need to be auto-generated.

If I could expose the internal bytes queue of the Parser, I wouldn't even speak
in terms of messages, but would just assign a sequence of bytes in the packets 
whose values are just sequence: For example:

Packet 1 would contain byte values 0x00, 0x01, 0x02
Packet 2 would contain byte values 0x03, 0x4

### Sequence of packets

Sequence of packets should be tested involving a mix of straddled and 
unstraddled packets. There should be sequences that contain ALL of the 4 packet
types above, thus we'd test packet sequences of length 4, maybe 5, as well as sequences that only contain some of the packet, maybe length 2. Empty packets could be interleaved
for good measure.

For simplicity and focusing on packet straddling, these packets should arrive in order / be sequential. The next topic discusses packets out order.

A long sequence of packets, let's say 213, could be auto-generated.

### Out of order packets
Testing out of order packets is really just a matter of shuffling sequence of packets.
A really simple re-ordering is to reverse the sequence. If the sequence of packets for
testing is stored in an array, they could be shuffled. There's ubiquitous algorithms for shuffling like Fisher-yates. Alternatively, store the packets in some collection
and randomly select an element. After feeding it into the parser, remove this element.
I remember implementing a MP3 player and was left with a decision: shuffle the playlist upfront, or just jump to a random song index. They're effectively the same.

The test output should be the same regardless of order, so extending test cases of
sequence of packets to out of orders sequences is trivial. 

## I/O Redirection / Dependency Injection

The current interface allows client specify a filename, and the contract is to write to this file.

A more flexible interface would accept an I/O stream. Then clients 
can specify/inject what to output stream to write to. The test could namely write
to an in memory buffer. 

## Date and time

Local time is dependent on the system's time zone. Tests may pass locally but fail
remotely if the environments are not the same. Running the tests in a Docker image with
these environment details setup would make the test more reproducible.

To test that the code in fact tests local time, I'd pick some timezone that is +7 or some odd number away from GMT. It's less likely that code implementation would coincidentally passtest by being off by 7 than it would be "round" values like 0, 12
or 24. 

Boundary and large values for the timestamp value should be tested. Since date and time is involved, special values may involve leap year, leap seconds, though I did not delve
that far. 

Finally, the Parser constructor accepts a YYYYMMDD integer. There are definitely
valid YYYYMMDD values for which we do not have a corresponding timestamp. For example, 
a year before 1970, since we are dealing with Epoch time. I added some minor validation in the Parser as I don't think this is the meat of the assignment and did not want to re-implement a date library. See "Falsehoods programmers belief about dates". Other behavior to flesh out: leap days. Values that don't match pattern YYYYMMDD. Negative
values, zero values or more generally values that are out-of-range. 