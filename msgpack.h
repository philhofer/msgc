#ifndef _MSGPACK_H_
#define _MSGPACK_H_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// wire_t is the list of
// MessagePack wire types.
typedef enum {
	MSG_INVALID, // an invalid byte
	MSG_INT,	 // an int(64)
	MSG_UINT,	 // a uint(64)
	MSG_F32,	 // a float
	MSG_F64,	 // a double
	MSG_BIN,	 // raw binary
	MSG_STR, 	 // a utf-8 string
	MSG_BOOL, 	 // a boolean
	MSG_MAP, 	 // a map header
	MSG_ARRAY,	 // an array header
	MSG_EXT,	 // an extension object
	MSG_NIL, 	 // 'nil'
} wire_t;

wire_t msgpack_type(uint8_t b);

// possible return values of a read/write function
enum {
	MSGPACK_OK,
	ERR_MSGPACK_EOF,
	ERR_MSGPACK_BAD_TYPE,
	ERR_MSGPACK_CHECK_ERRNO
};

// functions similarly to strerror, but takes
// one of the above enum values as an argument.
// if it is ERR_MSGPACK_CHECK_ERRNO, it returns
// strerror(errno).
const char* msgpack_strerror(int i);

// readf is a read callback function used by msgpack_decoder_t.
// 'readf' should copy no more than 'max' bytes into 'buf',
// returning the number of bytes copied. if it returns 0,
// errno should be set.
typedef size_t (*readf)(void* ctx, void* buf, size_t max);

// writef is a write callback function used by msgpack_encoder_t.
// 'writef' should write 'amt' bytes, returning the number of
// bytes written. if it does not return 'amt', errno should be set.
typedef size_t (*writef)(void* ctx, const void* buf, size_t amt);

// msgpack_decoder_t is a type that knows
// how to decode MessagePack from memory and/or streams.
typedef struct {
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
	unsigned char* base;
	size_t off;
	size_t cap;
	void*  ctx;
	writef write;
} msgpack_encoder_t;

// msgpack_decode_stream_init initializes a msgpack_decoder_t
// to read messagepack from a stream.
// 'mem' should point to a region of memory of size 'cap' that
// the decoder can use for buffering. (It must be at least 18 bytes, but
// ideally should be something on the order of 4KB.)
// 'readf' should point to a function that can read
// into spare buffer space, and 'ctx' can optionally
// be an arbitrary context passed to the read function.
void msgpack_decode_stream_init(msgpack_decoder_t* d, void* ctx, readf r, void* mem, size_t cap);

// initializes a decoder to read from a chunk of memory
void msgpack_decode_mem_init(msgpack_decoder_t* d, void* mem, size_t cap);

// initializes an encoder to write to a stream,
// using the supplied memory as a buffer
void msgpack_encode_stream_init(msgpack_encoder_t* d, void* ctx, writef w, void* mem, size_t cap);

// flushes the remaining buffered data to the writer
int msgpack_flush(msgpack_encoder_t* e);

// initializes an encoder to write to a fixed-size chunk of memory
void msgpack_encode_mem_init(msgpack_encoder_t* e, void* mem, size_t cap);

// reads 'amt' bytes directly into 'buf'
int msgpack_read_raw(msgpack_decoder_t*, char* buf, size_t amt);

// writes 'amt' bytes into the encoder from 'buf'
int msgpack_write_raw(msgpack_encoder_t*, const char* buf, size_t amt);

// puts the next wire type into 'ty'
int msgpack_next_type(msgpack_decoder_t* d, wire_t* ty);

int msgpack_read_uint(msgpack_decoder_t* d, uint64_t* u);

int msgpack_write_uint(msgpack_encoder_t* e, uint64_t u);

int msgpack_read_int(msgpack_decoder_t* d, int64_t* i);

int msgpack_write_int(msgpack_encoder_t* e, int64_t i);

int msgpack_read_float(msgpack_decoder_t* d, float* f);

int msgpack_write_float(msgpack_encoder_t* e, float f);

int msgpack_read_double(msgpack_decoder_t* d, double* f);

int msgpack_write_double(msgpack_encoder_t* e, double f);

int msgpack_read_bool(msgpack_decoder_t* d, bool* b);

int msgpack_write_bool(msgpack_encoder_t* e, bool b);

int msgpack_read_mapsize(msgpack_decoder_t* d, uint32_t* sz);

int msgpack_write_mapsize(msgpack_encoder_t* e, uint32_t sz);

int msgpack_read_arraysize(msgpack_decoder_t* d, uint32_t* sz);

int msgpack_write_arraysize(msgpack_encoder_t* e, uint32_t sz);

int msgpack_read_strsize(msgpack_decoder_t* d, uint32_t* sz);

int msgpack_write_strsize(msgpack_encoder_t* e, uint32_t sz);

int msgpack_write_str(msgpack_encoder_t* e, const char* c, uint32_t sz);

int msgpack_read_binsize(msgpack_decoder_t* d, uint32_t* sz);

int msgpack_write_binsize(msgpack_encoder_t* e, uint32_t sz);

int msgpack_write_bin(msgpack_encoder_t* d, const void* c, uint32_t sz);

int msgpack_read_extsize(msgpack_decoder_t* d, int8_t* tg, uint32_t* sz);

int msgpack_write_extsize(msgpack_encoder_t* e, int8_t tg, uint32_t sz);

int msgpack_write_ext(msgpack_encoder_t* e, int8_t tg, const void* mem, uint32_t sz);

int msgpack_read_nil(msgpack_decoder_t* d);

int msgpack_write_nil(msgpack_encoder_t* e);

#endif /* _MSGPACK_H_ */
