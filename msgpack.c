#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "msgpack.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define ABS(i) (((i) < 0) ? (i) : (-(i)))

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
} tag_t;

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

// get wire type of tag_t
wire_t msgpack_type(uint8_t b) {
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

	switch ((tag_t)b) {
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

void msgpack_decode_stream_init(msgpack_decoder_t* d, void* ctx, readf r, void* buf, size_t cap) {
	assert(cap >= 18);
	d->base = buf;
	d->off = 0;
	d->used = 0;
	d->cap = cap;
	d->ctx = ctx;
	d->read = r;
	return;
}

void msgpack_decode_mem_init(msgpack_decoder_t* d, void* mem, size_t cap) {
	d->base = mem;
	d->off = 0;
	d->used = cap;
	d->cap = cap;
	d->ctx = NULL;
	d->read = NULL;
	return;
}

// number of bytes immediately available for reading
static size_t buffered(msgpack_decoder_t* d) { return d->used - d->off; }

static bool fill(msgpack_decoder_t* d) {
	if (d->read != NULL) {
		size_t c = d->read(d->ctx, (d->base + d->used), (d->cap - d->used));
		if (c == 0) return false;
		d->used += c;
		return true;
	}
	errno = ERR_MSGPACK_EOF;
	return false;
}

// returns a pointer to the next 'req' valid bytes
// in the reader, and increments the read cursor by
// the same amount. returns NULL if there aren't enough
// bytes left, or if the readf callback returns -1.
static unsigned char* decoder_next(msgpack_decoder_t* d, size_t req) {
	if (req > d->cap) {
		errno = ERR_MSGPACK_EOF;
		return NULL;
	}
	while (buffered(d) < req) {
		if (!fill(d)) return NULL;
	}
	unsigned char* o = d->base + d->off;
	d->off += req;
	return o;
}

static unsigned char* decoder_peek(msgpack_decoder_t* d) {
	if (buffered(d) < 1 && !fill(d)) {
		return NULL;
	}
	return d->base + d->off;
}

// pointer to current read offset
static unsigned char* readoff(msgpack_decoder_t* d) {
	return d->base + d->off;
}

// warning: this is unsafe
static void unread_byte(msgpack_decoder_t* d) {
	d->off--;
	return;
}

static bool read_byte(msgpack_decoder_t* d, uint8_t* b) {
	unsigned char* nxt = decoder_next(d, 1);
	if (nxt == NULL) return false;
	*b = (uint8_t)(*nxt);
	return true;
}

static bool read_be16(msgpack_decoder_t* d, uint16_t* u) {
	unsigned char* nxt = decoder_next(d, 2);
	if (nxt == NULL) return false;
	uint16_t s = 0;
	s |= (((uint16_t)(*nxt++))<<8);
	s |= (uint16_t)(*nxt);
	*u = s;
	return true;
}

static bool read_be32(msgpack_decoder_t* d, uint32_t* u) {
	unsigned char* nxt = decoder_next(d, 4);
	if (nxt == NULL) return false;
	uint32_t s = 0;
	s |= ((uint32_t)(*nxt++)) << 24;
	s |= ((uint32_t)(*nxt++)) << 16;
	s |= ((uint32_t)(*nxt++)) << 8;
	s |= (uint32_t)(*nxt);
	*u = s;
	return true;
}

static bool read_be64(msgpack_decoder_t* d, uint64_t* u) {
	unsigned char* nxt = decoder_next(d, 8);
	if (nxt == NULL) return false;
	uint64_t s = 0;
	s |= (((uint64_t)(*nxt++))<<56);
	s |= (((uint64_t)(*nxt++))<<48);
	s |= (((uint64_t)(*nxt++))<<40);
	s |= (((uint64_t)(*nxt++))<<36);
	s |= (((uint64_t)(*nxt++))<<24);
	s |= (((uint64_t)(*nxt++))<<16);
	s |= (((uint64_t)(*nxt++))<<8);
	s |= (uint64_t)(*nxt);
	*u = __builtin_bswap64(s);
	return true;
}

bool msgpack_read_raw(msgpack_decoder_t* d, char* buf, size_t amt) {
	while (amt > 0) {
		size_t avail = buffered(d);
		if (avail == 0 && !fill(d)) {
			return false;
		}
		size_t cpy = MIN(amt, avail);
		memcpy(buf, readoff(d), cpy);
		d->off += cpy;
		buf += cpy;
		amt -= cpy;
	}
	return true;
}

bool msgpack_next_type(msgpack_decoder_t* d, wire_t* ty) {
	unsigned char* c = decoder_peek(d);
	if (c == NULL) return false;
	*ty = msgpack_type((uint8_t)(*c));
	return true;
}

static bool fixint(uint8_t b, int64_t* i) {
	if (b<0x80) {
		*i = (int64_t)b;
		return true;
	} else if (b > TAG_MAP32) {
		*i = (int64_t)(b & 0x1f);
		return true;
	}
	return false;
}

static bool fixuint(uint8_t b, uint64_t* u) {
	if (b<0x80) {
		*u = (uint64_t)b;
		return true;
	}
	return false;
}

bool msgpack_read_uint(msgpack_decoder_t* d, uint64_t* u) {
	uint8_t b;
	uint16_t m;
	uint32_t l;
	if (!read_byte(d, &b)) return false;
	if (fixuint(b, u)) return true;
	switch ((tag_t)b) {
	case TAG_UINT8:
		if (!read_byte(d, &b)) return false;
		*u = (uint64_t)b;
		return true;
	case TAG_UINT16:
		if (!read_be16(d, &m)) return false;
		*u = (uint64_t)m;
		return true;
	case TAG_UINT32:
		if (!read_be32(d, &l)) return false;
		*u = (uint64_t)l;
		return true;
	case TAG_UINT64:
		return read_be64(d, u);
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

bool msgpack_read_int(msgpack_decoder_t* d, int64_t* i) {
	uint8_t b;
	uint16_t m;
	uint32_t l;
	uint64_t q;
	if (!read_byte(d, &b)) return false;
	if (fixint(b, i)) return true;
	switch ((tag_t)b) {
	case TAG_UINT8:
		if (!read_byte(d, &b)) return false;
		*i = (int64_t)b;
		return true;
	case TAG_UINT16:
		if (!read_be16(d, &m)) return false;
		*i = (int64_t)m;
		return true;
	case TAG_UINT32:
		if (!read_be32(d, &l)) return false;
		*i = (int64_t)l;
		return true;
	case TAG_UINT64:
		if (!read_be64(d, &q)) return false;
		*i = (int64_t)q;
		return true;
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

bool msgpack_read_float(msgpack_decoder_t* d, float* f) {
	uint8_t b;
	if (!read_byte(d, &b)) return false;
	if ((tag_t)b != TAG_F32) {
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
	float_pun fp;
	if (!read_be32(d, &fp.bits)) return false;
	*f = fp.val;
	return true;
}

bool msgpack_read_double(msgpack_decoder_t* d, double* f) {
	uint8_t b;
	if (!read_byte(d, &b)) return false;
	if ((tag_t)b != TAG_F64) {
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
	double_pun dp;
	if (!read_be64(d, &dp.bits)) return false;
	*f = dp.val;
	return true;
}

bool msgpack_read_bool(msgpack_decoder_t* d, bool* b) {
	uint8_t t;
	if (!read_byte(d, &t)) return false;
	tag_t tg = (tag_t)t;
	if (tg == TAG_TRUE) {
		*b = true;
		return true;
	} 
	if (tg == TAG_FALSE) {
		*b = false;
		return true;
	}
	unread_byte(d);
	errno = ERR_MSGPACK_BAD_TYPE;
	return false;
}

bool msgpack_read_nil(msgpack_decoder_t* d) {
	uint8_t t;
	if (!read_byte(d, &t)) return false;
	if ((tag_t)t != TAG_NIL) {
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
	return true;
}

static bool fixmap(uint8_t b, uint32_t* sz) {
	if ((b&0xf0) == 0x80) {
		*sz = (uint32_t)(b&0x0f);
		return true;
	}
	return false;
}

bool msgpack_read_mapsize(msgpack_decoder_t* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	if (!read_byte(d, &t)) return false;
	if (fixmap(t, sz)) return true;
	switch ((tag_t)t) {
	case TAG_MAP16:
		if (!read_be16(d, &u)) return false;
		*sz = (uint32_t)u;
		return true;
	case TAG_MAP32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

static bool fixarray(uint8_t b, uint32_t* sz) {
	if ((b&0xf0) == 0x90) {
		*sz = (uint32_t)(b&0x0f);
		return true;
	}
	return false;
}

bool msgpack_read_arraysize(msgpack_decoder_t* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	if (!read_byte(d, &t)) return false;
	if (fixarray(t, sz)) return true;
	switch ((tag_t)t) {
	case TAG_ARRAY16:
		if (!read_be16(d, &u)) return false;
		*sz = (uint32_t)u;
		return true;
	case TAG_ARRAY32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

static bool fixstr(uint8_t b, uint32_t* sz) {
	if ((b&0xe0) == 0xa0) {
		*sz = (uint32_t)(b&0x1f);
		return true;
	}
	return false;
}

bool msgpack_read_strsize(msgpack_decoder_t* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	if (!read_byte(d, &t)) return false;
	if (fixstr(t, sz)) return true;
	switch ((tag_t)t) {
	case TAG_STR8:
		if (!read_byte(d, &t)) return false;
		*sz = (uint32_t)t;
		return true;
	case TAG_STR16:
		if (!read_be16(d, &u)) return false;
		*sz = (uint32_t)u;
		return true;
	case TAG_STR32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

bool msgpack_read_binsize(msgpack_decoder_t* d, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	if (!read_byte(d, &t)) return false;
	switch ((tag_t)t) {
	case TAG_BIN8:
		if (!read_byte(d, &t)) return false;
		*sz = (uint32_t)t;
		return true;
	case TAG_BIN16:
		if (!read_be16(d, &u)) return false;
		*sz = (uint32_t)u;
		return true;
	case TAG_BIN32:
		return read_be32(d, sz);
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

bool msgpack_read_extsize(msgpack_decoder_t* d, int8_t* tg, uint32_t* sz) {
	uint8_t t;
	uint16_t u;
	if (!read_byte(d, &t)) return false;
	switch ((tag_t)t) {
	case TAG_EXT8:
		if (!read_byte(d, &t)) return false;
		*sz = (uint32_t)t;
		goto pulltyp;
	case TAG_EXT16:
		if (!read_be16(d, &u)) return false;
		*sz = (uint32_t)u;
		goto pulltyp;
	case TAG_EXT32:
		if (!read_be32(d, sz)) return false;
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
		if (!read_byte(d, &t)) return false;
		*tg = (int8_t)t;
		return true;
	default:
		unread_byte(d);
		errno = ERR_MSGPACK_BAD_TYPE;
		return false;
	}
}

void msgpack_encode_stream_init(msgpack_encoder_t* e, void* ctx, writef w, void* mem, size_t cap) {
	e->base = mem;
	e->off = 0;
	e->cap = cap;
	e->ctx = ctx;
	e->write = w;
	return;
}

void msgpack_encode_mem_init(msgpack_encoder_t* e, void* mem, size_t cap) {
	e->base = mem;
	e->off = 0;
	e->cap = cap;
	e->ctx = NULL;
	e->write = NULL;
	return;
}

bool msgpack_flush(msgpack_encoder_t* e) {
	if (e->off == 0) return true;
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
			return false;
		}
		e->off = 0;
		return true;
	}
	return false;
}

static size_t avail(msgpack_encoder_t* e) {
	return e->cap - e->off;
}

bool msgpack_write_raw(msgpack_encoder_t* e, char* buf, size_t amt) {
	if (amt > e->cap) {
		if (!msgpack_flush(e)) return false;
		return amt == e->write(e->ctx, buf, amt);
	}
	if ((amt > avail(e)) && !msgpack_flush(e)) return false;
	memcpy(e->base + e->off, buf, amt);
	e->off += amt;
	return true;
}

static unsigned char* next(msgpack_encoder_t* e, size_t amt) {
	if (amt > e->cap) return NULL;
	if (amt > avail(e) && !msgpack_flush(e)) return NULL;
	unsigned char* c = e->base + e->off;
	e->off += amt;
	return c;
}

static bool write_byte(msgpack_encoder_t* e, uint8_t b) {
	unsigned char* c = next(e, 1);
	if (c == NULL) return false;
	*c = b;
	return true;
}

static bool write_prefix8(msgpack_encoder_t* e, tag_t t, uint8_t b) {
	unsigned char* c = next(e, 2);
	if (c == NULL) return false;
	*c++ = t;
	*c = b;
	return true;
}

static bool write_prefix16(msgpack_encoder_t* e, tag_t t, uint16_t u) {
	unsigned char* c = next(e, 3);
	if (c == NULL) return false;
	*c++ = t;
	*c++ = (u>>8)&0xff;
	*c = (u)&0xff;
	return true;
}

static bool write_prefix32(msgpack_encoder_t* e, tag_t t, uint32_t u) {
	unsigned char* c = next(e, 5);
	if (c == NULL) return false;
	*c++ = t;
	*c++ = (u>>24)&0xff;
	*c++ = (u>>16)&0xff;
	*c++ = (u>>8)&0xff;
	*c = (u)&0xff;
	return true;
}

static bool write_prefix64(msgpack_encoder_t* e, tag_t t, uint64_t u) {
	unsigned char* c = next(e, 9);
	if (c == NULL) return false;
	*c++ = t;
	*c++ = (u>>56);
	*c++ = (u>>48)&0xff;
	*c++ = (u>>40)&0xff;
	*c++ = (u>>32)&0xff;
	*c++ = (u>>24)&0xff;
	*c++ = (u>>16)&0xff;
	*c++ = (u>>8)&0xff;
	*c = (u)&0xff;
	return true;
}

bool msgpack_write_int(msgpack_encoder_t* e, int64_t i) {
	int64_t a = ABS(i);
	if (i < 0 && i > -32) {
		return write_byte(e, ((uint8_t)i)&0xe0);
	} else if (i >= 0 && i < 128) {
		return write_byte(e, (uint8_t)i);
	} else if (a < 255) {
		return write_prefix8(e, TAG_INT8, (uint8_t)i);
	} else if (a < (1<<16) - 1) {
		return write_prefix16(e, TAG_INT16, (uint16_t)i);
	} else if (a < ((int64_t)1<<32) - 1) {
		return write_prefix32(e, TAG_INT32, (uint32_t)i);
	}
	return write_prefix64(e, TAG_INT64, (uint64_t)i);
}

bool msgpack_write_uint(msgpack_encoder_t* e, uint64_t u) {
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

bool msgpack_write_float(msgpack_encoder_t* e, float f) {
	float_pun fp;
	fp.val = f;
	return write_prefix32(e, TAG_F32, fp.bits);
}

bool msgpack_write_double(msgpack_encoder_t* e, double f) {
	double_pun dp;
	dp.val = f;
	return write_prefix64(e, TAG_F64, dp.bits);
}

bool msgpack_write_bool(msgpack_encoder_t* e, bool b) {
	if (b) return write_byte(e, TAG_TRUE);
	return write_byte(e, TAG_FALSE);
}


bool msgpack_write_mapsize(msgpack_encoder_t* e, uint32_t sz) {
	if (sz < (1<<4)) {
		return write_byte(e, (sz|0x80)&0xff);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_MAP16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_MAP32, sz);
}

bool msgpack_write_arraysize(msgpack_encoder_t* e, uint32_t sz) {
	if (sz < (1<<4)) {
		return write_byte(e, (sz|0x90)&0xff);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_ARRAY16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_ARRAY32, sz);
}

bool msgpack_write_strsize(msgpack_encoder_t* e, uint32_t sz) {
	if (sz < (1<<6)-1) {
		return write_byte(e, ((uint8_t)sz)|0xa0);
	} else if (sz < (1<<8) - 1) {
		return write_prefix8(e, TAG_STR8, (uint8_t)sz);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_STR16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_STR32, sz);
}

bool msgpack_write_binsize(msgpack_encoder_t* e, uint32_t sz) {
	if (sz < (1<<8) - 1) {
		return write_prefix8(e, TAG_BIN8, (uint8_t)sz);
	} else if (sz < (1<<16) - 1) {
		return write_prefix16(e, TAG_BIN16, (uint16_t)sz);
	}
	return write_prefix32(e, TAG_BIN32, sz);
}

bool msgpack_write_extsize(msgpack_encoder_t* e, int8_t tg, uint32_t sz) {
	switch (sz) {
	case 1:
		if (!write_byte(e, TAG_FIXEXT1)) return false;
		goto typ;
	case 2:
		if (!write_byte(e, TAG_FIXEXT2)) return false;
		goto typ;
	case 4:
		if (!write_byte(e, TAG_FIXEXT4)) return false;
		goto typ;
	case 8:
		if (!write_byte(e, TAG_FIXEXT8)) return false;
		goto typ;
	case 16:
		if (!write_byte(e, TAG_FIXEXT16)) return false;
		goto typ;
	}
	if (sz < (1<<8) - 1) {
		if (!write_prefix8(e, TAG_EXT8, (uint8_t)sz)) return false;
		goto typ;
	} else if (sz < (1<<16) - 1) {
		if (!write_prefix16(e, TAG_EXT16, (uint16_t)sz)) return false;
		goto typ;
	}
	if (!write_prefix32(e, TAG_EXT32, sz)) return false;
typ:
	return write_byte(e, (uint8_t)tg);
}

bool msgpack_write_nil(msgpack_encoder_t* e) {
	return write_byte(e, TAG_NIL);
}
