## Same sequence number == same packet content
Duplicate packets as defined by packets with the same sequence number
have the exact same data. The current implementation uses the first
packet as the source-of-truth and duplicate packets arriving later are ignored.