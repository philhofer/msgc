# compiler
CC = clang

# flags
CFLAGS = -c -Wall -pedantic-errors -O2

# flags for linking into test files
TESTFLAGS = -g -Wall -pedantic-errors -O2

# memtest output
MTEST = memtest.out

# library files
FILES = msgpack.h msgpack.c

# build artifacts
OBJECTS = msgpack.o

all:
	$(CC) $(CFLAGS) $(FILES)

buildmemtest: all
	$(CC) $(TESTFLAGS) $(OBJECTS) memtest.c -o memtest.out

test: buildmemtest
	./$(MTEST)
	
clean:
	$(RM) -r memtest.out* *.o *.gch

