#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "msgpack.h"

#define SIZES 0, 1, 240, 280, 4000, 16600

#define BUFSIZE 4096

#define ASSERT_CIRCULAR_SIZES(typ) \
	printf("testing %s...\n", #typ); \
	ASSERT_CIRCULAR_SIZE(typ, 0); \
	ASSERT_CIRCULAR_SIZE(typ, 1); \
	ASSERT_CIRCULAR_SIZE(typ, 240); \
	ASSERT_CIRCULAR_SIZE(typ, 280); \
	ASSERT_CIRCULAR_SIZE(typ, 4000); \
	ASSERT_CIRCULAR_SIZE(typ, 16600); \
	ASSERT_CIRCULAR_SIZE(typ, 908145);

#define ASSERT_CIRCULAR_SIZE(typ, sz) \
	msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
	assert(msgpack_write_ ## typ ## size(&enc, sz)); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	{ \
		uint32_t out; \
		if (!msgpack_read_ ## typ ## size(&dec, &out)) { \
			printf("\tunexpected EOF\n"); \
			failed = true; \
		} \
		if (out != sz) { \
			printf("\tput %d in and got %d out\n", sz, out);	\
			failed = true; \
		} \
		if (msgpack_read_nil(&dec)) { \
			printf("\tnot at EOF\n"); \
		} \
	}

#define ASSERT_ZERO_EQ(typ,typt) \
	printf("testing zero for %s...\n", #typ); \
	msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
	assert(msgpack_write_ ## typ (&enc, 0)); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	{ \
		typt out; \
		if (!msgpack_read_ ## typ (&dec, &out)) { \
			printf("\tunexpected EOF\n"); \
			failed = true; \
		} \
		if (out != 0) { \
			printf("\rnot circular!\n"); \
			failed = true; \
		} \
		if (msgpack_read_nil(&dec)) { \
			printf("\tnot at EOF\n"); \
		} \
	}

int main() {
	printf("RUNNING TESTS...\n");
	bool failed = false;
	char buf[BUFSIZE];
	msgpack_encoder_t enc;
	msgpack_decoder_t dec;

	ASSERT_CIRCULAR_SIZES(map);
	ASSERT_CIRCULAR_SIZES(array);
	ASSERT_CIRCULAR_SIZES(str);
	ASSERT_CIRCULAR_SIZES(bin);

	ASSERT_ZERO_EQ(uint,uint64_t);
	ASSERT_ZERO_EQ(int,int64_t);
	ASSERT_ZERO_EQ(float,float);
	ASSERT_ZERO_EQ(double,double);

	if (failed) {
		printf("WARNING: TESTS FAILED!\n");
		return 1;
	}
	printf("ALL TESTS OK.\n");
	return 0;
}
