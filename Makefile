CC = clang
CFLAGS = -c -std=c11 -Werror -Wall -Wno-switch-enum -pedantic-errors -O3
LINKFLAGS = -Werror -Wall -Wno-switch-enum -pedantic-errors -O3

TESTFLAGS = -std=c11 -Werror -Wall -Wno-switch-enum -pedantic-errors -fsanitize=address,undefined,integer -O3

LIBDIR = lib
TESTDIR = test
BENCHDIR = bench

TESTS = memtest streamtest
BENCHMKS = membench

.PRECIOUS: $(LIBDIR)/%.o

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

$(LIBDIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.test.out: $(TESTDIR)/%.c msgpack.c
	$(CC) $(TESTFLAGS) $^ -o $@

%.bench.out: $(BENCHDIR)/%.o $(LIBDIR)/msgpack.o
	$(CC) $(LINKFLAGS) $^ -o $@

.PHONY: test bench clean

test: streamtest.test.out memtest.test.out
	./streamtest.test.out
	./memtest.test.out

bench: membench.bench.out
	./membench.bench.out

clean:
	$(RM) -r *.o *.out
