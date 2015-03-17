#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "msgpack.h"

/* WARNING: GROSS MACRO NONSENSE AHEAD */

#define BUFSIZE 4096	

#define ASSERT_CIRCULAR_SIZES(typ) \
	ASSERT_CIRCULAR_SIZE(typ, 0); \
	ASSERT_CIRCULAR_SIZE(typ, 1); \
	ASSERT_CIRCULAR_SIZE(typ, 18); \
	ASSERT_CIRCULAR_SIZE(typ, 34); \
	ASSERT_CIRCULAR_SIZE(typ, 240); \
	ASSERT_CIRCULAR_SIZE(typ, 280); \
	ASSERT_CIRCULAR_SIZE(typ, 4000); \
	ASSERT_CIRCULAR_SIZE(typ, 16600); \
	ASSERT_CIRCULAR_SIZE(typ, 908145);

#define ASSERT_CIRCULAR_SIZE(typ, sz) \
	msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
	assert(msgpack_write_## typ ##size(&enc, sz) == MSGPACK_OK); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	{ \
		uint32_t out; \
		int err; \
		err = msgpack_read_## typ ##size(&dec, &out); \
		if (err != MSGPACK_OK) { \
			printf("FAIL: (%s): %s\n", #typ, msgpack_strerror(err)); \
		} else if (out != sz) { \
			printf("FAIL: (%s): put %d in and got %d out\n", #typ, sz, out); \
			failed = true; \
		} \
		if (msgpack_read_nil(&dec) == MSGPACK_OK) { \
			printf("FAIL: (%s): not at EOF\n", #typ); \
		} \
	}

#define ASSERT_RAW_SIZE_EQ(sz) \
	{ \
		void* raw = malloc(sz); \
		memset(raw, 1, sz); \
		msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
		assert(msgpack_write_raw(&enc, raw, sz) == MSGPACK_OK); \
		assert(enc.off == sz); \
		assert(0 == memcmp(enc.base, raw, enc.off)); \
		msgpack_decode_mem_init(&dec, raw, enc.off); \
		void* out = malloc(sz); \
		assert(msgpack_read_raw(&dec, out, sz) == MSGPACK_OK); \
		assert(dec.off == sz); \
		assert(0 == memcmp(raw, out, sz)); \
		free(raw); free(out); \
	}

#define ASSERT_STR_EQ(typ, val) \
	{ \
		uint32_t sz = sizeof(val)-1; \
		msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
		assert(msgpack_write_## typ (&enc, val, sz) == MSGPACK_OK); \
		msgpack_decode_mem_init(&dec, buf, enc.off); \
		char* o = malloc((size_t)sz); \
		uint32_t osz; \
		assert(msgpack_read_ ## typ ## size(&dec, &osz) == MSGPACK_OK); \
		if (sz != osz) { \
			printf("FAIL: %s(size: %d): read size %d\n", #typ, sz, osz); \
			failed = true; \
		} else { \
			assert(msgpack_read_raw(&dec, o, sz) == MSGPACK_OK); \
			if (memcmp(o, val, sz) != 0) { \
				printf("FAIL: %s(size: %d): in != out\n", #typ, sz); \
				failed = true; \
			}  \
		} \
		free(o); \
	}


#define ASSERT_VAL_EQ(typ, typt, val) \
	msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
	assert(msgpack_write_## typ (&enc, val) == MSGPACK_OK); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	assert(msgpack_skip(&dec) == MSGPACK_OK); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	{ \
		typt out; \
		int err; \
		err = msgpack_read_ ##typ (&dec, &out); \
		if (err != MSGPACK_OK) { \
			printf("FAIL: %s(%s): %s\n", #typ, #val, msgpack_strerror(err)); \
			failed = true; \
		} \
		if (out != val) { \
			printf("FAIL: %s(%s): not cicrcular\n", #typ, #val); \
			failed = true; \
		} \
		if (msgpack_read_nil(&dec) == MSGPACK_OK) { \
			printf("FAIL: %s(%s): not at EOF\n", #typ, #val); \
		} \
	}

// for floating-point tests
#define ASSERT_APPROX_EQ(typ, typt, val) \
	msgpack_encode_mem_init(&enc, buf, BUFSIZE); \
	assert(msgpack_write_## typ (&enc, val) == MSGPACK_OK); \
	msgpack_decode_mem_init(&dec, buf, enc.off); \
	{ \
		typt out; \
		int err; \
		err = msgpack_read_## typ (&dec, &out); \
		if (err != MSGPACK_OK) { \
			printf("FAIL: %s(%s): %s\n", #typ, #val, msgpack_strerror(err)); \
			failed = true; \
		} \
		typt diff = out - val; \
		if (diff < 0) diff *= -1.0; \
		if (diff > 10E-4) { \
			printf("FAIL: %s(%s): not circular\n", #typ, #val); \
			failed = true; \
		} \
		if (msgpack_read_nil(&dec) == MSGPACK_OK) { \
			printf("FAIL: %s(%s): not at EOF\n", #typ, #val); \
			failed = true; \
		} \
	}

int main() {
	printf("RUNNING TESTS...\n");
	Encoder enc;
	Decoder dec;
	char buf[BUFSIZE];
	bool failed = false;

	ASSERT_CIRCULAR_SIZES(map);
	ASSERT_CIRCULAR_SIZES(array);
	ASSERT_CIRCULAR_SIZES(str);
	ASSERT_CIRCULAR_SIZES(bin);

	ASSERT_APPROX_EQ(float, float, (float)3.14);
	ASSERT_APPROX_EQ(double, double, 3.1415926);

	ASSERT_VAL_EQ(int, int64_t, 0);      	  // zero
	ASSERT_VAL_EQ(int, int64_t, -1);     	  // nfixint
	ASSERT_VAL_EQ(int, int64_t, -5);     	  // nfixint
	ASSERT_VAL_EQ(int, int64_t, -200);   	  // -int8
	ASSERT_VAL_EQ(int, int64_t, -400);   	  // -int16
	ASSERT_VAL_EQ(int, int64_t, -30982); 	  // -int32
	ASSERT_VAL_EQ(int, int64_t, -5000000000); // -int64
	ASSERT_VAL_EQ(int, int64_t, 40);     	  // fixint
	ASSERT_VAL_EQ(int, int64_t, 220);		  // int8
	ASSERT_VAL_EQ(int, int64_t, 3908);   	  // int16
	ASSERT_VAL_EQ(int, int64_t, 16600);  	  // int32
	ASSERT_VAL_EQ(int, int64_t, 50000000000); // int64

	ASSERT_VAL_EQ(uint, uint64_t, 0); 		   // zero
	ASSERT_VAL_EQ(uint, uint64_t, 1); 		   // fixint
	ASSERT_VAL_EQ(uint, uint64_t, 14); 		   // fixint
	ASSERT_VAL_EQ(uint, uint64_t, 200); 	   // uint8
	ASSERT_VAL_EQ(uint, uint64_t, 300); 	   // uint16
	ASSERT_VAL_EQ(uint, uint64_t, 20000);      // uint32
	ASSERT_VAL_EQ(uint, uint64_t, 5000000000); // uint64

	ASSERT_RAW_SIZE_EQ(5);
	ASSERT_RAW_SIZE_EQ(2048);

	ASSERT_STR_EQ(str, "hello, world!");
	ASSERT_STR_EQ(bin, "hello, world!");

	if (failed) {
		printf("WARNING: TESTS FAILED!\n");
		return 1;
	}
	printf("ALL TESTS OK.\n");
	return 0;
}
