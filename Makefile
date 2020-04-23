OBJS = Parser.o

all: feed

test: test_runner.cc libparser.a
	g++ -W -O2 -std=c++17 -o $@ $^

feed: main.cc libparser.a
	g++ -W -O2 -std=c++17 -o $@ $^

%.o : %.cc
	g++ -W -O2 -c -std=c++17 -o $@ $<

libparser.a: $(OBJS)
	ar rcs libparser.a $^

clean:
	rm -f *.o *.a feed
