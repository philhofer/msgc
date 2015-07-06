#ifndef MSGPACK_H__
#define MSGPACK_H__
#include <stdbool.h> /* bool */
#include <stdint.h>  /* (u)int{8,16,32,64}_t */
#include <stddef.h>  /* size_t */
#include <unistd.h>  /* ssize_t */

/* 
 * mp_typ_t is the list of
 * MessagePack wire types.
 */
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
} mp_typ_t;

/* 
 * mp_type returns the type
 * indicated by the supplied
 * byte.
 */
mp_typ_t mp_type(uint8_t b);

/* 
 * This enum describes every possible
 * return value of all of the functions
 * in this header that return an int.
 */ 
enum {
	MSGPACK_OK = 0,				 // ok
	ERR_MSGPACK_EOF = 1,		 // buffer too small
	ERR_MSGPACK_BAD_TYPE = 2,	 // tried to read the wrong value
	ERR_MSGPACK_CHECK_ERRNO = 3, // check errno
};

/* 
 * functions similarly to strerror, but takes
 * one of the above enum values as an argument.
 * if it is ERR_MSGPACK_CHECK_ERRNO, it returns
 * strerror(errno).
 */
const char *mp_strerror(int i);

/*
 * mp_fill_t is a read callback function used by mp_decoder_t.
 * 'mp_fill_t' should copy no more than 'max' bytes into 'buf',
 * returning the number of bytes copied. Zero should be returned
 * to indicate EOF, and -1 should be returned to indicate that
 * an error was encountered. errno should be set appropriately.
 */
typedef ssize_t (*mp_fill_t)(void *ctx, void *buf, size_t max);


/*
 * mp_flush_t is a write callback function used by mp_encoder_t.
 * 'mp_flush_t' should write up to 'amt' bytes, returning the number
 * of bytes written. -1 should be returned to indicate an error, in
 * which case errno should be set appropriately.
 */
typedef ssize_t (*mp_flush_t)(void *ctx, const void *buf, size_t amt);

/*
 * mp_decoder_t
 *
 * mp_decoder_t keeps track
 * of the state of a stream
 * of messagepack objects.
 * It can deserialize messagepack
 * from streams and/or from contiguous
 * chunks of memory.
 */
typedef struct {
	/* 
	 * NOTE: none of these
	 * fields should be 
	 * touched except by
	 * the functions 
	 * defined in this 
	 * header.
	 */
	unsigned char *base;
	size_t    off;
	size_t    cap;
	size_t    used;
	void      *ctx;
	mp_fill_t read;
} mp_decoder_t;

/*
 * mp_encoder_t
 *
 * mp_encoder_t keeps track
 * of the state of a stream
 * of messagepack objects.
 * It can serialize messagepack
 * to streams or to chunks of memory.
 */
typedef struct {
	/* 
	 * NOTE: none of these
	 * fields should be 
	 * touched except by
	 * the functions 
	 * defined in this 
	 * header.
	 */
	unsigned char *base;
	size_t     off;
	size_t     cap;
	void       *ctx;
	mp_flush_t write;
} mp_encoder_t;

/* mp_decoder_t */

/* 
 * mp_decode_stream_init initializes a mp_decoder_t
 * to read messagepack from a stream.
 * 'mem' should point to a region of memory of size 'cap' that
 * the decoder can use for buffering. (It must be at least 9 bytes, but
 * ideally should be something on the order of 4KB.)
 * 'mp_fill_t' should point to a function that can read
 * into spare buffer space, and 'ctx' can optionally
 * be an arbitrary context passed to the read function.
 * It is recommended that the stream being read from is unbuffered.
 */
void mp_decode_stream_init(mp_decoder_t *d, void *ctx, mp_fill_t r, unsigned char *mem, size_t cap);

/* initializes a decoder to read from a chunk of memory */
void mp_decode_mem_init(mp_decoder_t *d, unsigned char *mem, size_t cap);

/* puts the next object's type into 'ty' */
int mp_next_type(mp_decoder_t *d, mp_typ_t *ty);

/* skips the next object */
int mp_skip(mp_decoder_t *d);

/* 
 * returns the number of buffered bytes
 * (available for reading immediately)
 */
size_t mp_dec_buffered(mp_decoder_t *d);

/* returns the capacity of the buffer */
size_t mp_dec_capacity(mp_decoder_t *d);

/* mp_encoder_t */

/* 
 * initializes an encoder to write to a stream,
 * using the supplied memory as a buffer. 'cap'
 * must be at least 18 bytes, but ideally much more.
 * 'mp_flush_t' should be a write callback function pointer,
 * and 'ctx' can optionally be an arbitrary context passed 
 * to the write function. It is recommended that the stream
 * being written to is unbuffered.
 */
void mp_encode_stream_init(mp_encoder_t *d, void *ctx, mp_flush_t w, unsigned char *mem, size_t cap);

/* initializes an encoder to write to a fixed-size chunk of memory */
void mp_encode_mem_init(mp_encoder_t *e, unsigned char *mem, size_t cap);

/*
 * flushes any unwritten bytes to the stream,
 * if the encoder was initialized with a stream
 * and there are unwritten bytes
 */
int mp_flush(mp_encoder_t *e);

/* returns the number of currently buffered bytes */
size_t mp_enc_buffered(mp_encoder_t *e);

/* returns the capacity of the encoder's buffer */
size_t mp_enc_capacity(mp_encoder_t *e);

/*

	----    Modes    ----

mp_encoder_t* and mp_decoder_t* each have two
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
that operate on mp_decoder_t* and mp_encoder_t*,
respectively. The return value of each of these functions 
will be one of:

MSGPACK_OK: no error
ERR_MSGPACK_EOF: in 'mem' mode, ran out of buffer to read/write
ERR_MSGPACK_BAD_TYPE: (read functions only): attempted to read the wrong value
ERR_MSGPACK_CHECK_ERRNO: mp_fill_t/mp_flush_t: check errno

Variable-length types (bin, str, ext) can be written incrementally
(by writing the size and then writing raw bytes) or all at once. However,
they can only be read incrementally.

*/

/* Direct read/write */

int mp_read_byte(mp_decoder_t *d, unsigned char *b);
int mp_write_byte(mp_encoder_t *e, unsigned char b);

/*
 * mp_read and mp_write return the number of bytes
 * read or written, respectively, or -1 to indicate
 * an error condition. Note that the number of bytes
 * read or written may be less than the amount specified.
 */
ssize_t mp_read(mp_decoder_t *d, char *buf, size_t amt);
ssize_t mp_write(mp_encoder_t *e, const char *buf, size_t amt);

/* Unsigned Integers */

int mp_read_uint(mp_decoder_t *d, uint64_t *u);
int mp_write_uint(mp_encoder_t *e, uint64_t u);

/* Signed Integers */

int mp_read_int(mp_decoder_t *d, int64_t *i);
int mp_write_int(mp_encoder_t *e, int64_t i);

/* Floating-point Numbers */

int mp_read_float(mp_decoder_t *d, float *f);
int mp_write_float(mp_encoder_t *e, float f);

int mp_read_double(mp_decoder_t *d, double *f);
int mp_write_double(mp_encoder_t *e, double f);

/* Booleans */

int mp_read_bool(mp_decoder_t *d, bool *b);
int mp_write_bool(mp_encoder_t *e, bool b);

/* Map Headers */

int mp_read_mapsize(mp_decoder_t *d, uint32_t *sz);
int mp_write_mapsize(mp_encoder_t *e, uint32_t sz);

/* Array Headers */

int mp_read_arraysize(mp_decoder_t *d, uint32_t *sz);
int mp_write_arraysize(mp_encoder_t *e, uint32_t sz);

/* Strings */

int mp_read_strsize(mp_decoder_t *d, uint32_t *sz);
int mp_write_strsize(mp_encoder_t *e, uint32_t sz);
int mp_write_str(mp_encoder_t *e, const char *c, uint32_t sz);

/* Binary */

int mp_read_binsize(mp_decoder_t *d, uint32_t *sz);
int mp_write_binsize(mp_encoder_t *e, uint32_t sz);
int mp_write_bin(mp_encoder_t *d, const char *c, uint32_t sz);

/* Extensions */

int mp_read_extsize(mp_decoder_t *d, int8_t *tg, uint32_t *sz);
int mp_write_extsize(mp_encoder_t *e, int8_t tg, uint32_t sz);
int mp_write_ext(mp_encoder_t *e, int8_t tg, const char *c, uint32_t sz);

/* Nil */

int mp_read_nil(mp_decoder_t *d);
int mp_write_nil(mp_encoder_t *e);

#endif
