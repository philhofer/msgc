#ifndef _MSGPACK_H_
#define _MSGPACK_H_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

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
WireTyp mp_type(uint8_t b);

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
const char* mp_strerror(int i);

// Fill is a read callback function used by Decoder.
// 'Fill' should copy no more than 'max' bytes into 'buf',
// returning the number of bytes copied. If it returns 0,
// errno should be set.
typedef size_t (*Fill)(void* ctx, void* buf, size_t max);

// an example Fill for FILE*, which can be used like
// 
//	Decoder dec;
//  unsigned char buf[2048];
//  setbuf(stdin, NULL);
//  mp_decode_stream_init(&dec, stdin, fill_file, buf, 2048);
//
size_t fill_file(void* ctx, void* buf, size_t amt);


// Flush is a write callback function used by Encoder.
// 'Flush' should write 'amt' bytes, returning the number of
// bytes written. If it does not return 'amt', errno should be set.
typedef size_t (*Flush)(void* ctx, const void* buf, size_t amt);

// an example Flush for FILE*, which can be used like
// 
//  Encoder enc;
//  unsigned char buf[2048];
//  setbuf(stdout, NULL);
//  mp_encode_stream_init(&enc, stdout, flush_file, buf, 2048);
//
size_t flush_file(void* ctx, const void* buf, size_t amt);

#define BUF_FIELDS unsigned char* base; size_t off; size_t cap
#define DECODER_FIELDS size_t used; void* ctx; Fill read
#define ENCODER_FIELDS void* ctx; Flush write

// Decoder type
typedef struct {
	// no exported fields

	BUF_FIELDS;
	DECODER_FIELDS;
} Decoder;

// Encoder type
typedef struct {
	// no exported fields

	BUF_FIELDS;
	ENCODER_FIELDS;
} Encoder;

/* Decoder */

// mp_decode_stream_init initializes a Decoder
// to read messagepack from a stream.
// 'mem' should point to a region of memory of size 'cap' that
// the decoder can use for buffering. (It must be at least 9 bytes, but
// ideally should be something on the order of 4KB.)
// 'Fill' should point to a function that can read
// into spare buffer space, and 'ctx' can optionally
// be an arbitrary context passed to the read function.
// It is recommended that the stream being read from is unbuffered.
void mp_decode_stream_init(Decoder* d, void* ctx, Fill r, unsigned char* mem, size_t cap);

// initializes a decoder to read from a chunk of memory
void mp_decode_mem_init(Decoder* d, unsigned char* mem, size_t cap);

// puts the next object's type into 'ty'
int mp_next_type(Decoder* d, WireTyp* ty);

// skips the next object
int mp_skip(Decoder* d);

// returns the number of buffered bytes
// (available for reading immediately)
size_t mp_dec_buffered(Decoder* d);

// returns the capacity of the buffer
size_t mp_dec_capacity(Decoder* d);

/* ----------------- */

/* Encoder */

// initializes an encoder to write to a stream,
// using the supplied memory as a buffer. 'cap'
// must be at least 9 bytes, but ideally much more.
// 'Flush' should be a write callback function pointer,
// and 'ctx' can optionally be an arbitrary context passed 
// to the write function. It is recommended that the stream
// being written to is unbuffered.
void mp_encode_stream_init(Encoder* d, void* ctx, Flush w, unsigned char* mem, size_t cap);

// initializes an encoder to write to a fixed-size chunk of memory
void mp_encode_mem_init(Encoder* e, unsigned char* mem, size_t cap);

// flushes any unwritten bytes to the stream,
// if the encoder was initialized with a stream
// and there are unwritten bytes
int mp_flush(Encoder* e);

// returns the number of currently buffered bytes
size_t mp_enc_buffered(Encoder* e);

// returns the capacity of the encoder's buffer
size_t mp_enc_capacity(Encoder* e);

/* ----------------- */

/*  ----    Modes    ----

Encoder* and Decoder* each have two
different modes of operation. If they are initialized
with their corresponding '_mem_init' function, then they
will read/write directly to the chunk of memory provided to them.
If they are initialized with their corresponding '_stream_init'
function, then they will use the chunk of memory provided to them
as a scratch buffer, and use the provided read/write callback to
fill/flush the buffer, respectively.

    ---- Conventions ----

Functions in snake_case, typedefs with Capital letters, 
enums and constants in CAPITAL_SNAKE_CASE. (Technically, 
POSIX reserves (foo)_t typedefs.)

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

/* Direct read/write */

int mp_read(Decoder*, char* buf, size_t amt);
int mp_write(Encoder*, const char* buf, size_t amt);

/* Unsigned Integers */

int mp_read_uint(Decoder* d, uint64_t* u);
int mp_write_uint(Encoder* e, uint64_t u);

/* Signed Integers */

int mp_read_int(Decoder* d, int64_t* i);
int mp_write_int(Encoder* e, int64_t i);

/* Floating-point Numbers */

int mp_read_float(Decoder* d, float* f);
int mp_write_float(Encoder* e, float f);

int mp_read_double(Decoder* d, double* f);
int mp_write_double(Encoder* e, double f);

/* Booleans */

int mp_read_bool(Decoder* d, bool* b);
int mp_write_bool(Encoder* e, bool b);

/* Map Headers */

int mp_read_mapsize(Decoder* d, uint32_t* sz);
int mp_write_mapsize(Encoder* e, uint32_t sz);

/* Array Headers */

int mp_read_arraysize(Decoder* d, uint32_t* sz);
int mp_write_arraysize(Encoder* e, uint32_t sz);

/* Strings */

int mp_read_strsize(Decoder* d, uint32_t* sz);
int mp_write_strsize(Encoder* e, uint32_t sz);
int mp_write_str(Encoder* e, const char* c, uint32_t sz);

/* Binary */

int mp_read_binsize(Decoder* d, uint32_t* sz);
int mp_write_binsize(Encoder* e, uint32_t sz);
int mp_write_bin(Encoder* d, const char* c, uint32_t sz);

/* Extensions */

int mp_read_extsize(Decoder* d, int8_t* tg, uint32_t* sz);
int mp_write_extsize(Encoder* e, int8_t tg, uint32_t sz);
int mp_write_ext(Encoder* e, int8_t tg, const char* c, uint32_t sz);

/* Nil */

int mp_read_nil(Decoder* d);
int mp_write_nil(Encoder* e);

#endif /* _MSGPACK_H_ */
