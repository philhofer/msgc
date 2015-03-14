#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "msgpack.h"

// CLOCKS_PER_SEC is usually one million,
// so we'll do that many iterations
#define MILLION 1000000
#define ITERS MILLION

#define BUFSIZE 2048

// these must take string literals
#define write_strlit(e, str) msgpack_write_str(e, str, sizeof(str))
#define write_binlit(e, str) msgpack_write_bin(e, str, sizeof(str))

#define readstr(d) msgpack_read_strsize(d, &sz); msgpack_read_raw(d, scratch, (size_t)sz)

int main() {
	printf("Running benchmarks...\n");
	msgpack_encoder_t enc;
	msgpack_decoder_t dec;
	char buf[BUFSIZE];

	clock_t start = clock();
	for(int i=0; i<ITERS; i++) {
		// ENCODE BENCHMARK BODY
		msgpack_encode_mem_init(&enc, buf, BUFSIZE);
		msgpack_write_mapsize(&enc, 5);
		write_strlit(&enc, "field_label_one");
		write_strlit(&enc, "field_body_one");
		write_strlit(&enc, "a_float");
		msgpack_write_float(&enc, 3.14159);
		write_strlit(&enc, "an_integer");
		msgpack_write_int(&enc, 348);
		write_strlit(&enc, "some_binary");
		write_binlit(&enc, "thisissomeopaquebinary");
		write_strlit(&enc, "fieldfive");
		msgpack_write_uint(&enc, 5);
	}
	clock_t end = clock();
	size_t bytes = enc.off; // note: approximately 113
	double mbps = (double)(((bytes*ITERS)/(end-start))*(CLOCKS_PER_SEC/MILLION));
	printf("encode writes %g MB/sec\n", mbps);

	start = clock();
	uint32_t sz;
	char scratch[256]; // for string
	for(int i=0; i<ITERS; i++) {
		// DECODE BENCHMARK BODY
		msgpack_decode_mem_init(&dec, buf, enc.off);
		msgpack_read_mapsize(&dec, &sz);
		assert(sz == 5);
		readstr(&dec);
		readstr(&dec);
		readstr(&dec);
		float f;
		msgpack_read_float(&dec, &f);
		readstr(&dec);
		int64_t i;
		msgpack_read_int(&dec, &i);
		assert(i == 348);
		readstr(&dec);
		msgpack_read_binsize(&dec, &sz);
		msgpack_read_raw(&dec, scratch, (size_t)sz);
		readstr(&dec);
		uint64_t u;
		msgpack_read_uint(&dec, &u);
		assert(u == 5);
	}
	end = clock();
	mbps = (double)(((bytes*ITERS)/(end-start))*(CLOCKS_PER_SEC/MILLION));
	printf("decode reads at %g MB/sec\n", mbps);
	
	return 0;
}
