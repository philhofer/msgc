#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "../msgpack.h"

#define MILLION 1000000
#define ITERS 5*MILLION

#define BUFSIZE 2048

// these must take string literals
#define write_strlit(e, str) mp_write_str(e, str, sizeof(str)-1)
#define write_binlit(e, str) mp_write_bin(e, str, sizeof(str)-1)

#define readstr(d) mp_read_strsize(d, &sz); mp_read(d, scratch, (size_t)sz)

int main() {
	printf("Running benchmarks...\n");
	mp_encoder_t enc;
	mp_decoder_t dec;
	unsigned char buf[BUFSIZE];

	clock_t start = clock();
	for(int i=0; i<ITERS; ++i) {
		// ENCODE BENCHMARK BODY
		mp_encode_mem_init(&enc, buf, BUFSIZE);
		mp_write_mapsize(&enc, 5);
		write_strlit(&enc, "field_label_one");
		write_strlit(&enc, "field_body_one");
		write_strlit(&enc, "a_float");
		mp_write_double(&enc, 3.14);
		write_strlit(&enc, "an_integer");
		mp_write_int(&enc, 348);
		write_strlit(&enc, "some_binary");
		write_binlit(&enc, "thisissomeopaquebinary");
		write_strlit(&enc, "fieldfive");
		mp_write_uint(&enc, 5);
	}
	clock_t end = clock();
	size_t bytes = enc.off; // note: approximately 113
	double mbps = (double)(((bytes*ITERS)/(end-start))*(CLOCKS_PER_SEC/MILLION));
	printf("Encode: %g MB/sec\n", mbps);

	// validation of the encoded body
	start = clock();
	size_t blen = enc.off;
	for(int i=0; i<ITERS; ++i) {
		mp_decode_mem_init(&dec, buf, blen);
		mp_skip(&dec);
	}
	end = clock();
	mbps = (double)(((bytes*ITERS)/(end-start))*(CLOCKS_PER_SEC/MILLION));
	printf("Skip: %g MB/sec\n", mbps);

	start = clock();
	uint32_t sz;
	char scratch[256]; // for string
	for(int i=0; i<ITERS; ++i) {
		mp_decode_mem_init(&dec, buf, enc.off);
		mp_read_mapsize(&dec, &sz);
		assert(sz == 5);
		readstr(&dec);
		readstr(&dec);
		readstr(&dec);
		double f;
		mp_read_double(&dec, &f);
		readstr(&dec);
		int64_t ix;
		mp_read_int(&dec, &ix);
		assert(ix == 348);
		readstr(&dec);
		mp_read_binsize(&dec, &sz);
		mp_read(&dec, scratch, (size_t)sz);
		readstr(&dec);
		uint64_t u;
		mp_read_uint(&dec, &u);
		assert(u == 5);
	}
	end = clock();
	mbps = (double)(((bytes*ITERS)/(end-start))*(CLOCKS_PER_SEC/MILLION));
	printf("Decode: %g MB/sec\n", mbps);
	
	return 0;
}
