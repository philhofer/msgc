#ifndef _MSGPACK_H_
#define _MSGPACK_H_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// WireTyp is the list of
// MessagePack wire types.
typedef enum {
	MSG_INVALID, // an invalid byte (0xc1)
	MSG_INT,	 // an int(64)
	MSG_UINT,	 // a uint(64)
	MSG_F32,	 // a float
	MSG_F64,	 // a double
	MSG_BIN,	 // raw binary
	MSG_STR, 	 // a utf-8 string
	MSG_BOOL, 	 // a boolean
	MSG_MAP, 	 // a map (2N objects)
	MSG_ARRAY,	 // an array (N objects)
	MSG_EXT,	 // an extension object
	MSG_NIL 	 // 'nil'
} WireTyp;

// converts a byte to a WireTyp
WireTyp msgpack_type(uint8_t b);

// This enum describes every possible
// return value of all of the functions
// in this header that return an int.
enum {
	MSGPACK_OK,				// ok
	ERR_MSGPACK_EOF,		// buffer too small
	ERR_MSGPACK_BAD_TYPE,	// tried to read the wrong value
	ERR_MSGPACK_CHECK_ERRNO // check errno
};

// functions similarly to strerror, but takes
// one of the above enum values as an argument.
// if it is ERR_MSGPACK_CHECK_ERRNO, it returns
// strerror(errno).
const char* msgpack_strerror(int i);

// Fill is a read callback function used by Decoder.
// 'Fill' should copy no more than 'max' bytes into 'buf',
// returning the number of bytes copied. If it returns 0,
// errno should be set.
typedef size_t (*Fill)(void* ctx, void* buf, size_t max);

// Flush is a write callback function used by Encoder.
// 'Flush' should write 'amt' bytes, returning the number of
// bytes written. If it does not return 'amt', errno should be set.
typedef size_t (*Flush)(void* ctx, const void* buf, size_t amt);

// Decoder
typedef struct {
	unsigned char*  base;	// pointer to base of buffer
	size_t off;				// read offset
	size_t used;			// number of valid bytes
	size_t cap;				// max number of bytes
	void*  ctx;				// ctx for Fill
	Fill   read;			// read callback
} Decoder;

// Encoder
typedef struct {
	unsigned char* base;	// pointer to base of buffer
	size_t off;				// write offset
	size_t cap;				// buffer capacity
	void*  ctx;				// Flush ctx
	Flush  write;			// write callback
} Encoder;

// msgpack_decode_stream_init initializes a Decoder
// to read messagepack from a stream.
// 'mem' should point to a region of memory of size 'cap' that
// the decoder can use for buffering. (It must be at least 9 bytes, but
// ideally should be something on the order of 4KB.)
// 'Fill' should point to a function that can read
// into spare buffer space, and 'ctx' can optionally
// be an arbitrary context passed to the read function.
void msgpack_decode_stream_init(Decoder* d, void* ctx, Fill r, void* mem, size_t cap);

// initializes a decoder to read from a chunk of memory
void msgpack_decode_mem_init(Decoder* d, void* mem, size_t cap);

// reads the type of the next object in the decoder
int msgpack_next_type(Decoder* d, WireTyp* ty);

int msgpack_skip(Decoder* d);

// initializes an encoder to write to a stream,
// using the supplied memory as a buffer. 'cap'
// must be at least 9 bytes, but ideally much more.
// 'Flush' should be a write callback function pointer,
// and 'ctx' can optionally be an arbitrary context passed 
// to the write function.
void msgpack_encode_stream_init(Encoder* d, void* ctx, Flush w, void* mem, size_t cap);

// flushes the remaining buffered data to the writer,
// assuming the encoder is in 'stream' mode.
int msgpack_flush(Encoder* e);

/* 
----    Modes    ----

Encoder* and Decoder* each have two
different modes of operation. If they are initialized
with their corresponding '_mem_init' function, then they
will read/write directly to the chunk of memory provided to them.
If they are initialized with their corresponding '_stream_init'
function, then they will use the chunk of memory provided to them
as a scratch buffer, and use the provided read/write callback to
fill/flush the buffer, respectively.

---- Conventions ----

For each type that is read/write-able, 
there is a pair of corresponding functions
that operate on Decoder* and Encoder*,
respectively. The return value of each of these functions 
will be one of:

MSGPACK_OK: no error
ERR_MSGPACK_EOF: in 'mem' mode, ran out of buffer to read/write
ERR_MSGPACK_BAD_TYPE: (read functions only): attempted to read the wrong value
ERR_MSGPACK_CHECK_ERRNO: Fill/Flush: check errno

Variable-length types (bin, str, ext) can be written incrementally
(by writing the size and then writing raw bytes) or all at once. However,
they can only be read incrementally.
*/

// initializes an encoder to write to a fixed-size chunk of memory
void msgpack_encode_mem_init(Encoder* e, void* mem, size_t cap);

/* Raw Bytes */

int msgpack_read_raw(Decoder*, char* buf, size_t amt);
int msgpack_write_raw(Encoder*, const char* buf, size_t amt);

/* Unsigned Integers */

int msgpack_read_uint(Decoder* d, uint64_t* u);
int msgpack_write_uint(Encoder* e, uint64_t u);

/* Signed Integers */

int msgpack_read_int(Decoder* d, int64_t* i);
int msgpack_write_int(Encoder* e, int64_t i);

/* Floating-point Numbers */

int msgpack_read_float(Decoder* d, float* f);
int msgpack_write_float(Encoder* e, float f);

int msgpack_read_double(Decoder* d, double* f);
int msgpack_write_double(Encoder* e, double f);

/* Booleans */

int msgpack_read_bool(Decoder* d, bool* b);
int msgpack_write_bool(Encoder* e, bool b);

/* Map Headers */

int msgpack_read_mapsize(Decoder* d, uint32_t* sz);
int msgpack_write_mapsize(Encoder* e, uint32_t sz);

/* Array Headers */

int msgpack_read_arraysize(Decoder* d, uint32_t* sz);
int msgpack_write_arraysize(Encoder* e, uint32_t sz);

/* Strings */

int msgpack_read_strsize(Decoder* d, uint32_t* sz);
int msgpack_write_strsize(Encoder* e, uint32_t sz);
int msgpack_write_str(Encoder* e, const char* c, uint32_t sz);

/* Binary */

int msgpack_read_binsize(Decoder* d, uint32_t* sz);
int msgpack_write_binsize(Encoder* e, uint32_t sz);
int msgpack_write_bin(Encoder* d, const void* c, uint32_t sz);

/* Extensions */

int msgpack_read_extsize(Decoder* d, int8_t* tg, uint32_t* sz);
int msgpack_write_extsize(Encoder* e, int8_t tg, uint32_t sz);
int msgpack_write_ext(Encoder* e, int8_t tg, const void* mem, uint32_t sz);

/* Nil */

int msgpack_read_nil(Decoder* d);
int msgpack_write_nil(Encoder* e);

#endif /* _MSGPACK_H_ */
