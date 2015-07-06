#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "msgpack.h"

typedef struct {
	unsigned char *ptr;
	size_t woff;
	size_t roff;
	size_t cap;
} buf_t;

static size_t buffered(buf_t *b) {
	return b->roff - b->woff;
}

static size_t availspc(buf_t *b) {
	return b->cap - b->woff;
}

static void *rptr(buf_t *b) { return b->ptr + b->roff; }
static void *wptr(buf_t *b) { return b->ptr + b->woff; }

static void prealloc(buf_t *b, size_t amt) {
	if (availspc(b) < amt) {
		size_t dbl = 2 * b->cap;
		size_t nxt = (dbl < amt) ? amt : dbl;
		b->ptr = realloc(b->ptr, nxt);
		b->cap = nxt;

		assert(b->ptr);
	}
	return;
}

static ssize_t buf_write(buf_t *b, const void *mem, size_t amt) {
	prealloc(b, amt);
	memcpy(wptr(b), mem, amt);
	b->woff += amt;
	return (ssize_t)amt;
}

static ssize_t buf_read(buf_t *b, void *mem, size_t amt) {
	size_t bf = buffered(b);
	size_t cpy = (bf < amt) ? bf : amt;
	if (cpy > 0) {
		memcpy(mem, rptr(b), amt);
		b->roff += cpy;
		return (ssize_t)cpy;
	}
	return 0;
}

static void buf_init(buf_t *b, size_t cap) {
	b->ptr = malloc(cap);
	b->woff = 0;
	b->roff = 0;
	b->cap = cap;

	assert(b->ptr);
	return;
}

static ssize_t buf_flush(void *ctx, const void *mem, size_t amt) {
	return buf_write((buf_t*)ctx, mem, amt);
}

static ssize_t buf_fill(void *ctx, void *mem, size_t amt) {
	return buf_read((buf_t*)ctx, mem, amt);
}

static void buf_destroy(buf_t *b) {
	if (b->ptr != NULL) {
		free(b->ptr);
		b->woff = 0;
		b->roff = 0;
		b->cap = 0;
		b->ptr = NULL;
	}
	return;
}

int main(void) {
	printf("Running stream tests...\n");
	bool failed = false;
	buf_t buf; mp_encoder_t enc; mp_decoder_t dec;
	unsigned char stack[18];
	buf_init(&buf, 256);
	assert(buffered(&buf) == 0);
	assert(availspc(&buf) == 256);
	mp_encode_stream_init(&enc, &buf, buf_flush, stack, 18);
	assert(mp_write_arraysize(&enc, 4) == MSGPACK_OK);
	assert(mp_write_double(&enc, 3.14) == MSGPACK_OK);
	assert(mp_write_str(&enc, "hello, world!", sizeof("hello, world!")-1) == MSGPACK_OK);
	assert(mp_write_int(&enc, -1) == MSGPACK_OK);
	assert(mp_write_ext(&enc, 38, "extension 38", sizeof("extension 38")-1) == MSGPACK_OK);
	mp_flush(&enc);

	mp_decode_stream_init(&dec, &buf, buf_fill, stack, 18);
	int err = mp_skip(&dec);
	if (err) {
		printf("ERROR: mp_skip: %s\n", mp_strerror(err));
		failed = true;
	}

	buf_destroy(&buf);
	if (failed) return 1;
	printf("Stream tests OK.\n");
	return 0;
}
