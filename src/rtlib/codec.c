/* The engine's data codecs (see codec.h), rewritten from the original
 * (0x10046250-0x10048580). Portable C; no Windows, no C runtime beyond memset/memcpy.
 *
 * Bitmap stream: blocks, each a u16 header (method | data size) and its data:
 *  - raw: the bytes themselves
 *  - LZW (10, 11 or 12-bit codes): sub-chunks of u16 length + codes. Each sub-chunk
 *    holds up to 0x2000 input bytes and starts with an empty dictionary. Codes are
 *    packed MSB first; 0-255 are bytes, new strings get 256 and up while there is room
 *    (up to 2^bits - 2), 2^bits - 1 ends the sub-chunk. A zero code pads the end.
 *  - end: header 0xe000.
 * The compressor cuts the input into blocks of up to 0x1fff bytes and keeps a block raw
 * when packing doesn't make it smaller. */
#include "codec.h"
#include <string.h>

/* ---- LZW unpacking ---- */

typedef struct BitReader {
    const uint8_t *p, *end;
    uint32_t acc;
    int bits;
} BitReader;

static unsigned read_code(BitReader *r, int width)
{
    while (r->bits < width) {
        /* original: reads past a sub-chunk's end until the end code; we stop at the block */
        r->acc = (r->acc << 8) | (r->p < r->end ? *r->p : 0);
        r->p++;
        r->bits += 8;
    }
    r->bits -= width;
    return (r->acc >> r->bits) & ((1u << width) - 1);
}

/* Writes the string for code backwards from its last byte; returns its first byte. */
static uint8_t put_string(const LzwDecoder *z, unsigned code, uint8_t *at)
{
    if (code > 0xff) {
        uint8_t *p = at + z->length[code];
        do {
            *p-- = z->suffix[code];
            code = z->prefix[code];
        } while (code > 0xff);
    }
    *at = (uint8_t)code;
    return (uint8_t)code;
}

static size_t string_length(const LzwDecoder *z, unsigned code)
{
    return code > 0xff ? (size_t)z->length[code] + 1 : 1;
}

/* One sub-chunk; returns bytes written, or (size_t)-1 on overflow */
static size_t lzw_chunk(LzwDecoder *z, int bits, BitReader *r, uint8_t *dst, size_t room)
{
    const unsigned stop = (1u << bits) - 1, max = stop - 1;
    unsigned next = 0x100, old, code;
    uint8_t first;
    size_t n = 0;
    memset(z->length, 0, sizeof z->length);
    r->acc = 0;
    r->bits = 0;
    old = read_code(r, bits);
    if (room < 1)
        return (size_t)-1;
    first = dst[n++] = (uint8_t)old;
    for (;;) {
        size_t len;
        code = read_code(r, bits);
        if (code == stop)
            return n;
        if (code < next) {
            len = string_length(z, code);
            if (len > room - n)
                return (size_t)-1;
            first = put_string(z, code, dst + n);
            n += len;
        } else {
            /* the string being defined right now: old string + its first byte */
            len = string_length(z, old) + 1;
            if (len > room - n)
                return (size_t)-1;
            put_string(z, old, dst + n);
            dst[n + len - 1] = first;
            n += len;
        }
        if (next <= max) {
            z->suffix[next] = first;
            z->length[next] = (uint8_t)(z->length[old] + 1);
            z->prefix[next] = (uint16_t)old;
            next++;
        }
        old = code;
    }
}

size_t lzw_unpack_block(LzwDecoder *z, int bits, const uint8_t *src, size_t n, uint8_t *dst,
                        size_t room)
{
    size_t done = 0;
    long left = (long)n;
    const uint8_t *block_end = src + n;
    while (left > 0) {
        uint16_t len;
        BitReader r;
        size_t k;
        memcpy(&len, src, 2);
        r.p = src + 2;
        r.end = block_end;
        src += 2 + len;
        left -= len + 2;
        k = lzw_chunk(z, bits, &r, dst + done, room - done);
        if (k == (size_t)-1)
            return 0;
        done += k;
    }
    return done;
}

int bitmap_unpack(LzwDecoder *z, const uint8_t *src, size_t src_size, uint8_t *dst, size_t size)
{
    const uint8_t *end = src_size == (size_t)-1 ? (const uint8_t *)-1 : src + src_size;
    for (;;) {
        uint16_t head;
        unsigned method, n;
        size_t done;
        if ((size_t)(end - src) < 2)
            return 0;
        memcpy(&head, src, 2);
        src += 2;
        method = head & PACK_METHOD;
        n = head & PACK_SIZE;
        if (method == PACK_END)
            return size == 0;
        if (n > (size_t)(end - src))
            return 0;
        if (method == PACK_RAW) {
            if (size < n)
                return 0;
            memcpy(dst, src, n);
            done = n;
        } else if (method == PACK_LZW10 || method == PACK_LZW11 || method == PACK_LZW12) {
            int bits = method == PACK_LZW10 ? 10 : method == PACK_LZW11 ? 11 : 12;
            done = lzw_unpack_block(z, bits, src, n, dst, size);
        } else {
            return 0; /* original: PACK_RLE (and the rest) can't be unpacked */
        }
        if (!done)
            return 0;
        src += n;
        dst += done;
        size -= done;
    }
}

/* ---- LZW packing ---- */

typedef struct BitWriter {
    uint8_t *out;
    size_t n;
    uint32_t acc;
    int bits;
} BitWriter;

static void put_code(BitWriter *w, unsigned code, int width)
{
    w->acc |= (uint32_t)code << (32 - width - w->bits);
    for (w->bits += width; w->bits > 7; w->bits -= 8) {
        w->out[w->n++] = (uint8_t)(w->acc >> 24);
        w->acc <<= 8;
    }
}

size_t lzw_pack(LzwEncoder *z, int bits, const uint8_t *in, size_t n, uint8_t *out)
{
    const int shift = bits - 8;
    const unsigned stop = (1u << bits) - 1, max = stop - 1;
    size_t total = 0;
    if (bits < 9 || bits > LZW_MAX_BITS)
        return 0; /* original: falls back to 11 bits */
    while (n) {
        size_t len = n < 0x2000 ? n : 0x2000, i;
        BitWriter w = {out + total + 2, 0, 0, 0};
        unsigned next = 0x100, string = in[0];
        uint16_t chunk;
        n -= len;
        memset(z->code, 0xff, sizeof z->code);
        for (i = 1; i < len; i++) {
            uint8_t c = in[i];
            int index = (int)(uint16_t)((c << shift) ^ string);
            int step = index == 0 ? -1 : LZW_HASH - index;
            while (z->code[index] != 0xffff &&
                   !(z->prefix[index] == string && z->suffix[index] == c)) {
                index -= step;
                if (index < 0)
                    index += LZW_HASH;
            }
            if (z->code[index] != 0xffff) {
                string = z->code[index];
                continue;
            }
            if (next <= max) {
                z->code[index] = (uint16_t)next++;
                z->suffix[index] = c;
                z->prefix[index] = (uint16_t)string;
            }
            put_code(&w, string, bits);
            string = c;
        }
        put_code(&w, string, bits);
        put_code(&w, stop, bits);
        put_code(&w, 0, bits); /* flushes; leftover bits are dropped */
        chunk = (uint16_t)w.n;
        memcpy(out + total, &chunk, 2);
        total += w.n + 2;
        in += len;
    }
    return total;
}

/* ---- run-length packing (PACK_RLE) ----
 * 1..200: a run of that many copies of the next byte; 0xc8 + n: n literal bytes (n < 56);
 * 0 + s16 count: a long run (count > 0, then the byte) or long literal (-count bytes). */

static void put_bytes(uint8_t *out, size_t *pos, const void *p, size_t n)
{
    memcpy(out + *pos, p, n);
    *pos += n;
}

static void put_run(uint8_t *out, size_t *pos, uint8_t c, int len)
{
    uint8_t b[4];
    if (len < 0xc9) {
        b[0] = (uint8_t)len;
        b[1] = c;
        put_bytes(out, pos, b, 2);
        return;
    }
    for (; len > 0x7fff; len -= 0x7fff) {
        b[0] = 0;
        b[1] = 0xff;
        b[2] = 0x7f;
        b[3] = c;
        put_bytes(out, pos, b, 4);
    }
    if (len > 0) {
        b[0] = 0;
        b[1] = (uint8_t)len;
        b[2] = (uint8_t)(len >> 8);
        b[3] = c;
        put_bytes(out, pos, b, 4);
    }
}

static void put_literal(uint8_t *out, size_t *pos, const uint8_t *p, int len)
{
    uint8_t b[3];
    if (len < 0x38) {
        b[0] = (uint8_t)(len - 0x38);
        put_bytes(out, pos, b, 1);
        put_bytes(out, pos, p, (size_t)len);
        return;
    }
    for (; len > 0x7fff; len -= 0x7fff, p += 0x7fff) {
        b[0] = 0;
        b[1] = 0x01; /* -0x7fff */
        b[2] = 0x80;
        put_bytes(out, pos, b, 3);
        put_bytes(out, pos, p, 0x7fff);
    }
    if (len > 0) {
        int16_t neg = (int16_t)-len;
        b[0] = 0;
        memcpy(b + 1, &neg, 2);
        put_bytes(out, pos, b, 3);
        put_bytes(out, pos, p, (size_t)len);
    }
}

/* original: a run starts where two pairs repeat the first byte (p0 == p1 == p2,
 * checked two bytes at a time) */
static int starts_run(const uint8_t *p, int n)
{
    for (; n >= 3; p += 2, n -= 2) {
        if (p[1] != p[0])
            return 0;
        if (p[2] == p[0])
            return 1;
    }
    return 0;
}

static int pack_runs(const uint8_t *p, int n, uint8_t *out, size_t *pos)
{
    int done = 0;
    while (done < n) {
        int len = 1;
        uint8_t c = *p;
        while (done < n - 1 && *++p == c) {
            len++;
            done++;
        }
        if (len == 1)
            break;
        put_run(out, pos, c, len);
        done++;
    }
    return done;
}

static int pack_literals(const uint8_t *p, int n, uint8_t *out, size_t *pos)
{
    const uint8_t *start = p;
    int count = 1;
    for (;;) {
        if (count >= n)
            break;
        if (p[1] == p[0] && count < n - 1 && p[2] == p[0]) {
            count--;
            break;
        }
        p++;
        count++;
    }
    put_literal(out, pos, start, count);
    return count;
}

size_t rle_pack(const uint8_t *in, int n, uint8_t *out)
{
    size_t pos = 0;
    while (n > 0) {
        int k = starts_run(in, n) ? pack_runs(in, n, out, &pos) : pack_literals(in, n, out, &pos);
        if (!k)
            break;
        n -= k;
        in += k;
    }
    return pos;
}

/* ---- IMA ADPCM ---- */

static const uint16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
    66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
    408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
    1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
    7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
};
static const int16_t IMA_ADJUST[8] = {-8, -8, -8, -8, 16, 32, 48, 64}; /* index steps * 8 */

static int ima_diff(unsigned index8, unsigned nibble)
{
    int step = IMA_STEP[index8 >> 3], d = step >> 3;
    if (nibble & 4)
        d += step;
    if (nibble & 2)
        d += step >> 1;
    if (nibble & 1)
        d += step >> 2;
    return d;
}

static void ima_step(ImaState *s, int ch, unsigned nibble)
{
    int d = ima_diff(s->index[ch], nibble & 7);
    int v = s->sample[ch] + ((nibble & 8) ? -d : d);
    int i = (int16_t)(IMA_ADJUST[nibble & 7] + (int16_t)s->index[ch]);
    s->sample[ch] = v >= 0x8000 ? 0x7fff : v < -0x8000 ? -0x8000 : v;
    s->index[ch] = (uint16_t)(i < 0 ? 0 : i > 0x2c0 ? 0x2c0 : i);
}

void ima_unpack(ImaState *s, int16_t *out, const uint8_t *in, uint32_t n, int stereo)
{
    for (; n; n--, in++, out += 2) {
        ima_step(s, 0, *in & 0xf);
        out[0] = (int16_t)s->sample[0];
        ima_step(s, stereo ? 1 : 0, *in >> 4);
        out[1] = (int16_t)s->sample[stereo ? 1 : 0];
    }
}

static unsigned ima_nibble(ImaState *s, int ch, int sample)
{
    int diff = sample - s->sample[ch], step = IMA_STEP[s->index[ch] >> 3];
    unsigned nibble = 0;
    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }
    if (diff >= step) {
        nibble |= 4;
        diff -= step;
    }
    if (diff >= step >> 1) {
        nibble |= 2;
        diff -= step >> 1;
    }
    if (diff >= step >> 2)
        nibble |= 1;
    ima_step(s, ch, nibble);
    return nibble;
}

void ima_pack(ImaState *s, uint8_t *out, const int16_t *in, uint32_t n, int stereo)
{
    for (n >>= 2; n; n--, in += 2) {
        unsigned lo = ima_nibble(s, 0, in[0]);
        unsigned hi = ima_nibble(s, stereo ? 1 : 0, in[1]);
        *out++ = (uint8_t)(lo | hi << 4);
    }
}
