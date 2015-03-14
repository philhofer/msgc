#ifndef MSGPACK_H
#define MSGPACK_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// wire_t is the list of
// MessagePack wire types
typedef enum {
	MSG_INVALID,
	MSG_INT,
	MSG_UINT,
	MSG_F32,
	MSG_F64,
	MSG_BIN,
	MSG_STR,
	MSG_BOOL,
	MSG_MAP,
	MSG_ARRAY,
	MSG_EXT,
	MSG_NIL,
} wire_t;

// These are the values that
// msgpack.c uses for errno.
enum {
	ERR_MSGPACK_EOF = 0xbeef,
	ERR_MSGPACK_BAD_TYPE = 0xfeed
};

// readf is a read callback function used by msgpack_decoder_t
typedef size_t (*readf)(void* ctx, void* buf, size_t max);

typedef size_t (*writef)(void* ctx, void* buf, size_t amt);

// msgpack_decoder_t is a type that knows
// how to decode MessagePack from memory and/or streams.
typedef struct {
	// All struct fields are "private."
	// Direct modification by the user will
	// result in undefined behavior.
	unsigned char*  base;  // pointer to base of buffer
	size_t off;   // read offset
	size_t used;  // number of valid bytes
	size_t cap;   // max number of bytes
	void*  ctx;   // ctx for readf
	readf  read;  // read callback
} msgpack_decoder_t;

// msgpack_encoder_t is a type
// that knows how to encode MessagePack from
// memory and/or streams.
typedef struct {
	// All struct fields are "private."
	// Direct modification by the user will
	// result in undefined behavior.
	unsigned char* base;
	size_t off;
	size_t cap;
	void*  ctx;
	writef write;
} msgpack_encoder_t;


wire_t msgpack_type(uint8_t b);

// msgpack_decode_stream_init initializes a msgpack_decoder_t
// to read messagepack from a stream.
// 'mem' should point to a region of memory of size 'cap' that
// the decoder can use for buffering. (It must be at least 18 bytes, but
// ideally should be something on the order of 4KB.)
// 'readf' should point to a function that can read
// into spare buffer space, and 'ctx' can optionally
// be an arbitrary context passed to the read function.
void msgpack_decode_stream_init(msgpack_decoder_t* d, void* ctx, readf r, void* mem, size_t cap);

// msgpack_decode_mem_init initializes a msgpack_decoder_t to
// read messagepack from memory. 'mem' should point to the beginning
// of the memory, and 'cap' should be the size of the valid memory.
void msgpack_decode_mem_init(msgpack_decoder_t* d, void* mem, size_t cap);

void msgpack_encode_stream_init(msgpack_encoder_t* d, void* ctx, writef w, void* mem, size_t cap);

bool msgpack_flush(msgpack_encoder_t* e);

void msgpack_encode_mem_init(msgpack_encoder_t* e, void* mem, size_t cap);

bool msgpack_read_raw(msgpack_decoder_t*, char* buf, size_t amt);

bool msgpack_write_raw(msgpack_encoder_t*, char* buf, size_t amt);

bool msgpack_next_type(msgpack_decoder_t* d, wire_t* ty);

bool msgpack_read_uint(msgpack_decoder_t* d, uint64_t* u);

bool msgpack_write_uint(msgpack_encoder_t* e, uint64_t u);

bool msgpack_read_int(msgpack_decoder_t* d, int64_t* i);

bool msgpack_write_int(msgpack_encoder_t* e, int64_t i);

bool msgpack_read_float(msgpack_decoder_t* d, float* f);

bool msgpack_write_float(msgpack_encoder_t* e, float f);

bool msgpack_read_double(msgpack_decoder_t* d, double* f);

bool msgpack_write_double(msgpack_encoder_t* e, double f);

bool msgpack_read_bool(msgpack_decoder_t* d, bool* b);

bool msgpack_write_bool(msgpack_encoder_t* e, bool b);

bool msgpack_read_mapsize(msgpack_decoder_t* d, uint32_t* sz);

bool msgpack_write_mapsize(msgpack_encoder_t* e, uint32_t sz);

bool msgpack_read_arraysize(msgpack_decoder_t* d, uint32_t* sz);

bool msgpack_write_arraysize(msgpack_encoder_t* e, uint32_t sz);

bool msgpack_read_strsize(msgpack_decoder_t* d, uint32_t* sz);

bool msgpack_write_strsize(msgpack_encoder_t* e, uint32_t sz);

bool msgpack_read_binsize(msgpack_decoder_t* d, uint32_t* sz);

bool msgpack_write_binsize(msgpack_encoder_t* e, uint32_t sz);

bool msgpack_read_extsize(msgpack_decoder_t* d, int8_t* tg, uint32_t* sz);

bool msgpack_write_extsize(msgpack_encoder_t* e, int8_t tg, uint32_t sz);

bool msgpack_read_nil(msgpack_decoder_t* d);

bool msgpack_write_nil(msgpack_encoder_t* e);

#endif /* MSGPACK_H */
