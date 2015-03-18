#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "msgpack.h"

#define BE_ROLL(p, u, sz) \
	for (uint8_t off=0; off<sz; off++) { \
		*p++ = (u>>(8*(sz-off-1)))&0xff; \
	}

#define write_BE(e, i, tag, amt) \
	unsigned char* c; \
	TRY(next(e, amt+1, &c)); \
	*c++ = tag; \
	BE_ROLL(c, i, amt)

#define TRY(expr) int err = expr; if (err != MSGPACK_OK) return err

size_t fill_file(void* ctx, void* buf, size_t amt) {
	return fread(buf, 1, amt, (FILE*)ctx);
}

size_t flush_file(void* ctx, const void* buf, size_t amt) {
	return fwrite(buf, 1, amt, (FILE*)ctx);
}

// all type byte tags
typedef enum {
	TAG_NIL = 0xc0,
	TAG_INVALID = 0xc1,
	TAG_FALSE = 0xc2,
	TAG_TRUE = 0xc3,
	TAG_BIN8 = 0xc4,
	TAG_BIN16 = 0xc5,
	TAG_BIN32 = 0xc6,
	TAG_EXT8 = 0xc7,
	TAG_EXT16 = 0xc8,
	TAG_EXT32 = 0xc9,
	TAG_F32 = 0xca,
	TAG_F64 = 0xcb,
	TAG_UINT8 = 0xcc,
	TAG_UINT16 = 0xcd,
	TAG_UINT32 = 0xce,
	TAG_UINT64 = 0xcf,
	TAG_INT8 = 0xd0,
	TAG_INT16 = 0xd1,
	TAG_INT32 = 0xd2,
	TAG_INT64 = 0xd3,
	TAG_FIXEXT1 = 0xd4,
	TAG_FIXEXT2 = 0xd5,
	TAG_FIXEXT4 = 0xd6,
	TAG_FIXEXT8 = 0xd7,
	TAG_FIXEXT16 = 0xd8,
	TAG_STR8 = 0xd9,
	TAG_STR16 = 0xda,
	TAG_STR32 = 0xdb,
	TAG_ARRAY16 = 0xdc,
	TAG_ARRAY32 = 0xdd,
	TAG_MAP16 = 0xde,
	TAG_MAP32 = 0xdf,
} tag;

// union for type-punning f32
typedef union {
	uint32_t bits;
	float    val;
} float_pun;

// union for type-punning f64
typedef union {
	uint64_t bits;
	double   val;
} double_pun;

const char* mp_strerror(int i) {
	switch (i) {
	case MSGPACK_OK:
		return "OK";
	case ERR_MSGPACK_EOF:
		return "EOF";
	case ERR_MSGPACK_BAD_TYPE:
		return "msgpack type mismatch";
	case ERR_MSGPACK_CHECK_ERRNO:
		return strerror(errno);
	default:
		return "<unknown>";
	}
}

// get wire type of tag
WireTyp mp_type(uint8_t b) {
	// all fixed types are less than
	// TAG_NIL except TAG_NEGFIXINT
	if (b < TAG_NIL) {
		switch (b&0xf0) {
		case 0:
			return MSG_INT;
		case 0x80:
			return MSG_MAP;
		case 0x90:
			return MSG_ARRAY;
		case 0xa0:
		case 0xb0:
			return MSG_STR;
		}
	} else if (b > TAG_MAP32) {
		return MSG_INT;
	}

	switch ((tag)b) {
	case TAG_NIL:
		return MSG_NIL;
	case TAG_INVALID: // unused byte
		return MSG_INVALID;
	case TAG_FALSE:
	case TAG_TRUE:
		return MSG_BOOL;
	case TAG_BIN8:
	case TAG_BIN16:
	case TAG_BIN32:
		return MSG_BIN;
	case TAG_EXT8:
	case TAG_EXT16:
	case TAG_EXT32:
		return MSG_EXT;
	case TAG_F32:
		return MSG_F32;
	case TAG_F64:
		return MSG_F64;
	case TAG_UINT8:
	case TAG_UINT16:
	case TAG_UINT32:
	case TAG_UINT64:
		return MSG_UINT;
	case TAG_INT8:
	case TAG_INT16:
	case TAG_INT32:
	case TAG_INT64:
		return MSG_INT;
	case TAG_FIXEXT1:
	case TAG_FIXEXT2:
	case TAG_FIXEXT4:
	case TAG_FIXEXT8:
	case TAG_FIXEXT16:
		return MSG_EXT;
	case TAG_STR8:
	case TAG_STR16:
	case TAG_STR32:
		return MSG_STR;
	case TAG_ARRAY16:
	case TAG_ARRAY32:
		return MSG_ARRAY;
	case TAG_MAP16:
	case TAG_MAP32:
		return MSG_MAP;
	}
	return MSG_INVALID;
}

void mp_decode_stream_init(Decoder* d, void* ctx, Fill r, unsigned char* buf, size_t cap) {
	assert(cap >= 18);
	d->base = buf;
	d->off = 0;
	d->used = 0;
	d->cap = cap;
	d->ctx = ctx;
	d->read = r;
	return;
}

void mp_decode_mem_init(Decoder* d, unsigned char* mem, size_t cap) {
	d->base = mem;
	d->off = 0;
	d->used = cap;
	d->cap = cap;
	d->ctx = NULL;
	d->read = NULL;
	return;
}

size_t mp_dec_buffered(Decoder* d) { return d->used - d->off; }

size_t mp_dec_capacity(Decoder* d) { return d->cap; }

static int fill(Decoder* d) {
	if (d->read != NULL) {
		size_t c = d->read(d->ctx, (d->base + d->used), (d->cap - d->used));
		if (c == 0) return ERR_MSGPACK_CHECK_ERRNO;
		d->used += c;
		return MSGPACK_OK;
	}
	return ERR_MSGPACK_CHECK_ERRNO;
}

// returns a pointer to the next 'req' valid bytes
// in the reader, and increments the read cursor by
// the same amount. returns NULL if there aren't enough
// bytes left, or if the readf callback returns -1.
static int decoder_next(Decoder* d, size_t req, unsigned char** c) {
	if (req > d->cap) {
		return ERR_MSGPACK_EOF;
	}
	while (mp_dec_buffered(d) < req) {
		TRY(fill(d));
	}
	*c = d->base + d->off;
	d->off += req;
	return MSGPACK_OK;
}

static int decoder_peek(Decoder* d, unsigned char** c) {
	if (mp_dec_buffered(d) < 1) {
		TRY(fill(d));
	}
	*c = d->base + d->off;
	return MSGPACK_OK;
}

// pointer to current read offset
static unsigned char* readoff(Decoder* d) {
	return d->base + d->off;
}

// warning: this is unsafe
static void unread_byte(Decoder* d) {
	d->off--;
	return;
}

static int read_byte(Decoder* d, uint8_t* b) {
	if (mp_dec_buffered(d) < 1) {
		TRY(fill(d));
	};
	*b = *readoff(d);
	++d->off;
	return MSGPACK_OK;
}

static int peek8(Decoder* d, uint8_t* u) {
	while (mp_dec_buffered(d) < 2) {
		TRY(fill(d));
	}
	unsigned char* p = d->base + d->off + 1;
	*u = *p;
	return MSGPACK_OK;
}

static int peek16(Decoder* d, uint16_t* u) {
	while (mp_dec_buffered(d) < 3) {
		TRY(fill(d));
	}
	unsigned char* p = d->base + d->off + 1;
	uint16_t s = 0;
	s |= ((uint16_t)(*p++)) << 8;
	s |= ((uint16_t)(*p));
	*u = s;
	return MSGPACK_OK;
}

static int peek32(Decoder* d, uint32_t* u) {
	while (mp_dec_buffered(d) < 5) {
		TRY(fill(d));
	}
	unsigned char* p = d->base + d->off + 1;
	uint32_t s = 0;
	s |= ((uint32_t)(*p++)) << 24;
	s |= ((uint32_t)(*p++)) << 16;
	s |= ((uint32_t)(*p++)) << 8;
	s |= (uint32_t)(*p);
	*u = s;
	return MSGPACK_OK;
}

static int read_be16(Decoder* d, uint16_t* u) {
	unsigned char* nxt;
	TRY(decoder_next(d, 2, &nxt));
	uint16_t s = 0;
	s |= ((uint16_t)(*nxt++)) << 8;
	s |= ((uint16_t)(*nxt));
	*u = s;
	return MSGPACK_OK;
}


static bool read_be32(Decoder* d, uint32_t* u) {
	unsigned char* nxt;
	TRY(decoder_next(d, 4, &nxt));
	uint32_t s = 0;
	s |= ((uint32_t)(*nxt++)) << 24;
	s |= ((uint32_t)(*nxt++)) << 16;
	s |= ((uint32_t)(*nxt++)) << 8;
	s |= (uint32_t)(*nxt);
	*u = s;
	return MSGPACK_OK;
}

static bool read_be64(Decoder* d, uint64_t* u) {
	unsigned char* nxt;
	TRY(decoder_next(d, 8, &nxt));
	uint64_t s = 0;
	s |= ((uint64_t)(*nxt++)) << 56;
	s |= ((uint64_t)(*nxt++)) << 48;
	s |= ((uint64_t)(*nxt++)) << 40;
	s |= ((uint64_t)(*nxt++)) << 32;
	s |= ((uint64_t)(*nxt++)) << 24;
	s |= ((uint64_t)(*nxt++)) << 16;
	s |= ((uint64_t)(*nxt++)) << 8;
	s |= ((uint64_t)(*nxt));
	*u = s;
	return MSGPACK_OK;
}

static bool fixmap(uint8_t b, uint32_t* sz) {
	if ((b&0xf0) == 0x80) {
		*sz = (uint32_t)(b&0x0f);
		return true;
	}
	return false;
}

static bool fixarray(uint8_t b, uint32_t* sz) {
	if ((b&0xf0) == 0x90) {
		*sz = (uint32_t)(b&0x0f);
		return true;
	}
	return false;
}

static bool fixstr(uint8_t b, uint32_t* sz) {
	if ((b&0xe0) == 0xa0) {
		*sz = (uint32_t)(b&0x1f);
		return true;
	}
	return false;
}

// get next object size; self in *this and number of children in *sub
static int next_size(Decoder* d, size_t* this, size_t* sub) {
	int err;
	if (mp_dec_buffered(d) < 1) {
		err = fill(d);
		if (err != MSGPACK_OK) return err;
	}
	uint8_t b = *(d->base + d->off);
	uint16_t l;
	uint32_t m;
	if (b < 0x80 || b > TAG_MAP32) {
		*this = 1;
		*sub = 0;
		return MSGPACK_OK;
	} else if (fixstr(b, &m)) {
		*this = m+1;
		*sub = 0;
		return MSGPACK_OK;
	} else if (fixmap(b, &m)) {
		*this = 1;
		*sub = 2*m;
		return MSGPACK_OK;
	} else if (fixarray(b, &m)) {
		*this = 1;
		*sub = m;
		return MSGPACK_OK;
	}
	switch (b) {
	case TAG_NIL:
	case TAG_FALSE:
	case TAG_TRUE:
		*this = 1;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_BIN8:
	case TAG_STR8:
		err = peek8(d, &b);
		*this = 2+(size_t)b;
		*sub = 0;
		return err;
	case TAG_BIN16:
	case TAG_STR16:
		err = peek16(d, &l);
		*this = 3+(size_t)l;
		*sub = 0;
		return err;
	case TAG_BIN32:
	case TAG_STR32:
		err = peek32(d, &m);
		*this = 5+(size_t)m;
		*sub = 0;
		return err;
	case TAG_INT8:
	case TAG_UINT8:
		*this = 2;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_INT16:
	case TAG_UINT16:
		*this = 3;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_INT32:
	case TAG_UINT32:
	case TAG_F32:
		*this = 5;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_INT64:
	case TAG_UINT64:
	case TAG_F64:
		*this = 9;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_ARRAY16:
		err = peek16(d, &l);
		*this = 3;
		*sub = (size_t)l;
		return err;
	case TAG_ARRAY32:
		err = peek32(d, &m);
		*this = 5;
		*sub = (size_t)m;
		return err;
	case TAG_MAP16:
		err = peek16(d, &l);
		*this = 3;
		*sub = 2 * (size_t)l;
		return err;
	case TAG_MAP32:
		err = peek32(d, &m);
		*this = 5;
		*sub = 2 * (size_t)m;
		return err;
	case TAG_FIXEXT1:
		*this = 3;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_FIXEXT2:
		*this = 4;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_FIXEXT4:
		*this = 6;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_FIXEXT8:
		*this = 10;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_FIXEXT16:
		*this = 18;
		*sub = 0;
		return MSGPACK_OK;
	case TAG_EXT8:
		err = peek8(d, &b);
		*this = 3 + (size_t)b;
		*sub = 0;
		return err;
	case TAG_EXT16:
		err = peek16(d, &l);
		*this = 4 + (size_t)l;
		*sub = 0;
		return err;
	case TAG_EXT32:
		err = peek32(d, &m);
		*this = 6 + (size_t)m;
		*sub = 0;
		return err;
	case TAG_INVALID:
		return ERR_MSGPACK_BAD_TYPE;
	}
	return ERR_MSGPACK_BAD_TYPE;
}

// skip n bytes
static int skipn(Decoder* d, size_t n) {
	do {
		size_t cur = d->used - d->off;
		if (n <= cur) {
			d->off += n;
			return MSGPACK_OK;
		}
		d->off = 0;
		d->used = 0;
		n -= cur;
		TRY(fill(d));
	} while (n > 0);
	return MSGPACK_OK;
}

int mp_skip(Decoder* d) {
	size_t t; // tag size
	size_t s; // composite size
	TRY(next_size(d, &t, &s));
	err = skipn(d, t);
	if (err != MSGPACK_OK) return err;
	while (s > 0) {
		err = mp_skip(d);
		if (err != MSGPACK_OK) return err;
		--s;
	}
	return MSGPACK_OK;
}

int mp_read(Decoder* d, char* buf, size_t amt) {
	while (amt > 0) {
		if (mp_dec_buffered(d) == 0) {
			TRY(fill(d));
		}
		size_t avail = mp_dec_buffered(d);
		size_t cpy = (amt < avail) ? amt : avail;
		memcpy(buf, readoff(d), cpy);
		d->off += cpy;
		buf += cpy;
		amt -= cpy;
	}
	return MSGPACK_OK;
}

int mp_next_type(Decoder* d, WireTyp* ty) {
	unsigned char* c;
	TRY(decoder_peek(d, &c));
	*ty = mp_type((uint8_t)(*c));
	return MSGPACK_OK;
}

static bool fixint(uint8_t b, int64_t* i) {
	if ((b>>7) == 0 || (b&0xe0) == 0xe0) {
		*i = (int64_t)((int8_t)b);
		return true;
	}
	return false;
}

static bool fixuint(uint8_t b, uint64_t* u) {
	if (b>>7 == 0) {
		*u = (uint64_t)b;
		return true;
	}
	return false;
}

int mp_read_uint(Decoder* d, uint64_t* u) {
	uint8_t b;
	uint16_t m;
	uint32_t l;
	int o = read_byte(d, &b);
	if (o != MSGPACK_OK) return o;
	if (fixuint(b, u)) return MSGPACK_OK;
	switch ((tag)b) {
	case TAG_UINT8:
		o = read_byte(d, &b);
		*u = (uint64_t)b;
		return o;
	case TAG_UINT16:
		o = read_be16(d, &m);
		*u = (uint64_t)m;
		return o;
	case TAG_UINT32:
		o = read_be32(d, &l);
		*u = (uint64_t)l;
		return o;
	case TAG_UINT64:
		return read_be64(d, u);
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_int(Decoder* d, int64_t* i) {
	uint8_t b;
	uint16_t m;
	uint32_t l;
	uint64_t up;
	int o = read_byte(d, &b);
	if (o != MSGPACK_OK) return o;
	if (fixint(b, i)) return MSGPACK_OK;
	switch ((tag)b) {
	case TAG_INT8:
		o = read_byte(d, &b);
		*i = (int64_t)((int8_t)b);
		return o;
	case TAG_INT16:
		o = read_be16(d, &m);
		*i = (int64_t)((int16_t)m);
		return o;
	case TAG_INT32:
		o = read_be32(d, &l);
		*i = (int64_t)((int32_t)l);
		return o;
	case TAG_INT64:
		o = read_be64(d, &up);
		*i = (int64_t)up;
		return o;
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_float(Decoder* d, float* f) {
	uint8_t b;
	TRY(read_byte(d, &b));
	if ((tag)b != TAG_F32) {
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
	float_pun fp;
	err = read_be32(d, &fp.bits);
	*f = fp.val;
	return err;
}

int mp_read_double(Decoder* d, double* f) {
	uint8_t b;
	TRY(read_byte(d, &b));
	if ((tag)b != TAG_F64) {
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
	double_pun dp;
	err = read_be64(d, &dp.bits);
	*f = dp.val;
	return err;
}

int mp_read_bool(Decoder* d, bool* b) {
	uint8_t t;
	TRY(read_byte(d, &t));
	switch ((tag)t) {
	case TAG_TRUE:
		*b = true;
		return MSGPACK_OK;
	case TAG_FALSE:
		*b = false;
		return MSGPACK_OK;
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_nil(Decoder* d) {
	uint8_t t;
	TRY(read_byte(d, &t));
	if ((tag)t != TAG_NIL) {
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
	return MSGPACK_OK;
}

int mp_read_mapsize(Decoder* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	TRY(read_byte(d, &t));
	if (fixmap(t, sz)) return MSGPACK_OK;
	switch ((tag)t) {
	case TAG_MAP16:
		err = read_be16(d, &u);
		*sz = (uint32_t)u;
		return err;
	case TAG_MAP32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_arraysize(Decoder* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	TRY(read_byte(d, &t));
	if (fixarray(t, sz)) return MSGPACK_OK;
	switch ((tag)t) {
	case TAG_ARRAY16:
		err = read_be16(d, &u);
		*sz = (uint32_t)u;
		return err;
	case TAG_ARRAY32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_strsize(Decoder* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	TRY(read_byte(d, &t));
	if (fixstr(t, sz)) return MSGPACK_OK;
	switch ((tag)t) {
	case TAG_STR8:
		err = read_byte(d, &t);
		*sz = (uint32_t)t;
		return err;
	case TAG_STR16:
		err = read_be16(d, &u);
		*sz = (uint32_t)u;
		return err;
	case TAG_STR32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_binsize(Decoder* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	TRY(read_byte(d, &t));
	switch ((tag)t) {
	case TAG_BIN8:
		err = read_byte(d, &t);
		*sz = (uint32_t)t;
		return err;
	case TAG_BIN16:
		err = read_be16(d, &u);
		*sz = (uint32_t)u;
		return err;
	case TAG_BIN32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

int mp_read_extsize(Decoder* d, int8_t* tg, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	TRY(read_byte(d, &t));
	switch ((tag)t) {
	case TAG_EXT8:
		err = read_byte(d, &t);
		*sz = (uint32_t)t;
		goto pulltyp;
	case TAG_EXT16:
		err = read_be16(d, &u);
		*sz = (uint32_t)u;
		goto pulltyp;
	case TAG_EXT32:
		err = read_be32(d, sz);
		goto pulltyp;
	case TAG_FIXEXT1:
		*sz = 1;
		goto pulltyp;
	case TAG_FIXEXT2:
		*sz = 2;
		goto pulltyp;
	case TAG_FIXEXT4:
		*sz = 4;
		goto pulltyp;
	case TAG_FIXEXT8:
		*sz = 8;
		goto pulltyp;
	case TAG_FIXEXT16:
		*sz = 16;
	pulltyp:
		if (err != MSGPACK_OK) return err;
		err = read_byte(d, &t);
		if (err != MSGPACK_OK) return err;
		*tg = (int8_t)t;
		return MSGPACK_OK;
	default:
		unread_byte(d);
		return ERR_MSGPACK_BAD_TYPE;
	}
}

void mp_encode_stream_init(Encoder* e, void* ctx, Flush w, unsigned char* mem, size_t cap) {
	e->base = mem;
	e->off = 0;
	e->cap = cap;
	e->ctx = ctx;
	e->write = w;
	return;
}

void mp_encode_mem_init(Encoder* e, unsigned char* mem, size_t cap) {
	e->base = mem;
	e->off = 0;
	e->cap = cap;
	e->ctx = NULL;
	e->write = NULL;
	return;
}

int mp_flush(Encoder* e) {
	if (e->off == 0) return MSGPACK_OK;
	if (e->write != NULL) {
		size_t wrote = e->write(e->ctx, e->base, e->off);
		if (wrote < e->off) {
			// if we wrote *some* bytes, then copy
			// the un-written bytes to the beginning
			// of the buffer
			if (wrote > 0) {
				memmove(e->base, e->base + wrote, e->off - wrote);
				e->off -= wrote;
			}
			return ERR_MSGPACK_CHECK_ERRNO;
		}
		e->off = 0;
	}
	return MSGPACK_OK;
}

size_t mp_enc_buffered(Encoder* e) { return e->off; }

size_t mp_enc_capacity(Encoder* e) { return e->cap; }

static size_t avail(Encoder* e) {
	return e->cap - e->off;
}

int mp_write(Encoder* e, const char* buf, size_t amt) {
	if (amt > avail(e)) {
		TRY(mp_flush(e));
		if (amt > e->cap) {
			size_t wrote = e->write(e->ctx, buf, amt);
			if (wrote != amt) return ERR_MSGPACK_CHECK_ERRNO;
		}
	}
	memcpy(e->base + e->off, buf, amt);
	e->off += amt;
	return MSGPACK_OK;
}

static int next(Encoder* e, size_t amt, unsigned char** c)  {
	if (amt > avail(e)) {
		if (amt > e->cap) {
			return ERR_MSGPACK_EOF;
		}
		TRY(mp_flush(e));
	}
	*c = e->base + e->off;
	e->off += amt;
	return MSGPACK_OK;
}

static int write_byte(Encoder* e, uint8_t b) {
	if (avail(e) == 0) {
		TRY(mp_flush(e));
	}
	unsigned char* c = e->base + e->off;
	*c = b;
	++e->off;
	return MSGPACK_OK;
}

static int write_prefix8(Encoder* e, tag t, uint8_t b) {
	unsigned char* c;
	TRY(next(e, 2, &c));
	*c++ = t;
	*c = b;
	return MSGPACK_OK;
}

static int write_prefix16(Encoder* e, tag t, uint16_t u) {
	write_BE(e, u, t, 2);
	return MSGPACK_OK;
}

static int write_prefix32(Encoder* e, tag t, uint32_t u) {
	write_BE(e, u, t, 4);
	return MSGPACK_OK;
}

static int write_prefix64(Encoder* e, tag t, uint64_t u) {
	write_BE(e, u, t, 8);
	return MSGPACK_OK;
}

int mp_write_int(Encoder* e, int64_t i) {
	int64_t a = (i < 0 ? -i : i);
	if (i > -32 && i < 0) {
		int8_t j = (int8_t)i;
		return write_byte(e, (uint8_t)j);
	} else if (i >= 0 && i < 128) {
		return write_byte(e, (uint8_t)i);
	} else if (a < 128) {
		return write_prefix8(e, TAG_INT8, (uint8_t)i);
	} else if (a < (1<<16) - 1) {
		return write_prefix16(e, TAG_INT16, (uint16_t)i);
	} else if (a < ((int64_t)1<<32) - 1) {
		return write_prefix32(e, TAG_INT32, (uint32_t)i);
	}
	return write_prefix64(e, TAG_INT64, (uint64_t)i);
}

int mp_write_uint(Encoder* e, uint64_t u) {
	if (u < 127) {
		return write_byte(e, (uint8_t)u);
	} else if (u < 256) {
		return write_prefix8(e, TAG_UINT8, (uint8_t)u);
	} else if (u < (1<<16) - 1) {
		return write_prefix16(e, TAG_UINT16, (uint16_t)u);
	} else if (u < ((uint64_t)1<<32) - 1) {
		return write_prefix32(e, TAG_UINT32, (uint32_t)u);
	}
	return write_prefix64(e, TAG_UINT64, u);
}

int mp_write_float(Encoder* e, float f) {
	float_pun fp;
	fp.val = f;
	return write_prefix32(e, TAG_F32, fp.bits);
}

int mp_write_double(Encoder* e, double f) {
	double_pun dp;
	dp.val = f;
	return write_prefix64(e, TAG_F64, dp.bits);
}

int mp_write_bool(Encoder* e, bool b) {
	if (b) return write_byte(e, TAG_TRUE);
	return write_byte(e, TAG_FALSE);
}


int mp_write_mapsize(Encoder* e, uint32_t sz) {
	if (sz < (1<<4)) {
		return write_byte(e, (sz|0x80)&0xff);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_MAP16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_MAP32, sz);
}

int mp_write_arraysize(Encoder* e, uint32_t sz) {
	if (sz < (1<<4)) {
		return write_byte(e, (sz|0x90)&0xff);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_ARRAY16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_ARRAY32, sz);
}

int mp_write_str(Encoder* e, const char* c, uint32_t sz) {
	TRY(mp_write_strsize(e, sz));
	return mp_write(e, c, (size_t)sz);
}

int mp_write_strsize(Encoder* e, uint32_t sz) {
	if (sz < (1<<5)) {
		return write_byte(e, ((uint8_t)sz)|0xa0);
	} else if (sz < (1<<8) - 1) {
		return write_prefix8(e, TAG_STR8, (uint8_t)sz);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_STR16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_STR32, sz);
}

int mp_write_binsize(Encoder* e, uint32_t sz) {
	if (sz < (1<<8) - 1) {
		return write_prefix8(e, TAG_BIN8, (uint8_t)sz);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_BIN16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_BIN32, sz);
}

int mp_write_bin(Encoder* e, const char* c, uint32_t sz) {
	TRY(mp_write_binsize(e, sz));
	return mp_write(e, c, (size_t)sz);
}

int mp_write_extsize(Encoder* e, int8_t tg, uint32_t sz) {
	int err;
	switch (sz) {
	case 1:
		err = write_byte(e, TAG_FIXEXT1);
		goto typ;
	case 2:
		err = write_byte(e, TAG_FIXEXT2);
		goto typ;
	case 4:
		err = write_byte(e, TAG_FIXEXT4);
		goto typ;
	case 8:
		err = write_byte(e, TAG_FIXEXT8);
		goto typ;
	case 16:
		err = write_byte(e, TAG_FIXEXT16);
		goto typ;
	}
	if (sz < (1<<8) - 1) {
		err = write_prefix8(e, TAG_EXT8, (uint8_t)sz);
		goto typ;
	} else if (sz < (1<<16) - 1) {
		err = write_prefix16(e, TAG_EXT16, (uint16_t)sz);
		goto typ;
	}
	err = write_prefix32(e, TAG_EXT32, sz);
typ: /* type is always written at the end */
	if (err != MSGPACK_OK) return err;
	return write_byte(e, (uint8_t)tg);
}

int mp_write_ext(Encoder*e, int8_t tg, const char* c, uint32_t sz) {
	TRY(mp_write_extsize(e, tg, sz));
	return mp_write(e, c, (size_t)sz);
}

int mp_write_nil(Encoder* e) {
	return write_byte(e, TAG_NIL);
}

#undef TRY
#undef BEROLL
#undef write_BE
