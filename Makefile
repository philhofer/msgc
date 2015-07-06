CC = clang
CFLAGS = -c -Werror -Weverything -Wno-switch-enum -pedantic-errors -O3
LINKFLAGS = -Werror -Weverything -Wno-switch-enum -pedantic-errors -O3

msgpack.o: msgpack.c
	$(CC) $(CFLAGS) msgpack.c -o msgpack.o

memtest.o: memtest.c
	$(CC) $(CFLAGS) memtest.c -o memtest.o

memtest.out: memtest.o msgpack.o
	$(CC) $(LINKFLAGS) memtest.o msgpack.o -o memtest.out

streamtest.o: streamtest.c
	$(CC) $(CFLAGS) streamtest.c -o streamtest.o

streamtest.out: streamtest.o msgpack.o
	$(CC) $(LINKFLAGS) streamtest.o msgpack.o -o streamtest.out

membench.o: membench.c
	$(CC) $(CFLAGS) membench.c -o membench.o

membench.out: membench.o msgpack.o
	$(CC) $(LINKFLAGS) membench.o msgpack.o -o membench.out

.PHONY: test bench clean

test: memtest.out streamtest.out
	./memtest.out
	./streamtest.out

bench: membench.out
	./membench.out
	
clean:
	$(RM) -r *.o *.gch *.bc *.out *.dSYM
