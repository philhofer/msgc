# compiler
CC = clang

# compile flags
# NOTE: -emit-llvm depends on clang; however, using llvm bitcode enables LTO
CFLAGS = -c -emit-llvm -Werror -Weverything -Wno-switch-enum -pedantic-errors -O2
TESTFLAGS = -c -emit-llvm -Werror -Weverything -pedantic-errors -O2

# link flags
LINKFLAGS = -Werror -Weverything -Wno-switch-enum -pedantic-errors -O2

msgpack.o: msgpack.c
	$(CC) $(CFLAGS) msgpack.c -o msgpack.o

memtest.o: memtest.c
	$(CC) $(TESTFLAGS) memtest.c -o memtest.o

memtest.out: memtest.o msgpack.o
	$(CC) $(LINKFLAGS) memtest.o msgpack.o -o memtest.out

test: memtest.out
	@./memtest.out

membench.o: membench.c
	$(CC) $(TESTFLAGS) membench.c -o membench.o

membench.out: membench.o msgpack.o
	$(CC) $(LINKFLAGS) membench.o msgpack.o -o membench.out

bench: membench.out
	@./membench.out
	
clean:
	@$(RM) -r *.o *.gch *.bc *.out *.dSYM
