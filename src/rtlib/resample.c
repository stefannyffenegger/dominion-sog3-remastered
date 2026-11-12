/* The engine's audio library, 0x1004504c-0x10045480 and 0x10045f08-0x10046250
 * (hand-written assembly): converting PCM between 8 and 16 bits, mono and stereo, and
 * sample rates. Rates change by whole factors: down by averaging (a factor of 2, else
 * the sum shifted by 2), up by linear steps. Stereo to mono keeps the left channel.
 * Checked against the original by tests/resample_compare.c. Rewritten from the
 * original. */
#include <stdint.h>
#include <string.h>
#include "resample.h"

/* the state (9 bytes): */
enum {
    IN16, OUT16,      /* 16-bit samples */
    IN_ST, OUT_ST,    /* stereo */
    DOWN, UP,         /* rate factors (1 when unused) */
    DOWN_SHIFT, UP_SHIFT,
    MODE,             /* 0-9 the converters below, 10 copy, 11 rate change */
};

/* 0x1004504c */
static void classify(uint8_t *st)
{
    uint8_t mode = 11, in16 = st[IN16];
    if (st[DOWN] == st[UP]) {
        if (st[IN_ST] == st[OUT_ST])
            mode = in16 == st[OUT16] ? 10 : in16 ? 9 : 8;
        else if (!st[IN_ST])
            mode = in16 == st[OUT16] ? (in16 ? 3 : 0) : (in16 ? 2 : 1);
        else
            mode = in16 == st[OUT16] ? (in16 ? 7 : 4) : (in16 ? 6 : 5);
    }
    st[MODE] = mode;
}

/* 0x100452e0: sets up converting len bytes of `in` to `out`; returns the output size */
uint32_t __stdcall resample_setup(uint8_t *st, const WaveFormat *out, const WaveFormat *in, uint32_t len)
{
    memset(st, 0, 4);
    st[DOWN_SHIFT] = 0;
    st[DOWN] = st[UP] = 1;
    if (in->channels == 2)
        st[IN_ST]++;
    if (out->channels == 2)
        st[OUT_ST]++;
    if (in->bits == 16)
        st[IN16]++;
    if (out->bits == 16)
        st[OUT16]++;
    uint32_t blocks = len / in->block_align;
    if (out->rate > in->rate) {
        uint32_t f = out->rate / in->rate;
        st[UP] = (uint8_t)f;
        st[UP_SHIFT] = (uint16_t)f == 2 ? 1 : 2;
        blocks *= f;
    } else if (out->rate < in->rate) {
        uint32_t f = in->rate / out->rate;
        st[DOWN] = (uint8_t)f;
        st[DOWN_SHIFT] = (uint16_t)f == 2 ? 1 : 2;
        blocks /= (uint32_t)(int16_t)f;
    }
    classify(st);
    return blocks * out->block_align;
}

/* ---- modes 0-9 (0x10045f08-0x100461f6): one frame at a time ---- */

static int32_t s8(uint8_t b) { return (int16_t)(uint16_t)((uint8_t)(b - 0x80) << 8); }
static uint8_t to8(int16_t v) { return (uint8_t)((v >> 8) + 0x80); }

/* out and in frame sizes (as shifts) per mode */
static const uint8_t g_shift[10][2] = {{1, 0}, {2, 0}, {1, 1}, {2, 1}, {0, 1}, {1, 1}, {0, 2}, {1, 2}, {1, 0}, {0, 1}};

static uint32_t convert(int mode, uint8_t *dst, const uint8_t *src, uint32_t dst_len, uint32_t src_len, uint32_t *used)
{
    uint32_t os = g_shift[mode][0], is = g_shift[mode][1];
    uint32_t out = dst_len >> os, in = src_len >> is;
    for (; out && in; out--, in--) {
        switch (mode) {
        case 0: /* 8 mono to 8 stereo */
            dst[0] = dst[1] = *src++;
            break;
        case 1: /* 8 mono to 16 stereo */
            ((int16_t *)dst)[0] = ((int16_t *)dst)[1] = (int16_t)s8(*src++);
            break;
        case 2: /* 16 mono to 8 stereo */
            dst[0] = dst[1] = to8(*(const int16_t *)src);
            src += 2;
            break;
        case 3: /* 16 mono to 16 stereo */
            ((int16_t *)dst)[0] = ((int16_t *)dst)[1] = *(const int16_t *)src;
            src += 2;
            break;
        case 4: /* 8 stereo to 8 mono: the left */
            dst[0] = src[0];
            src += 2;
            break;
        case 5:
            *(int16_t *)dst = (int16_t)s8(src[0]);
            src += 2;
            break;
        case 6:
            dst[0] = to8(*(const int16_t *)src);
            src += 4;
            break;
        case 7:
            *(int16_t *)dst = *(const int16_t *)src;
            src += 4;
            break;
        case 8: /* 8 to 16 bits */
            *(int16_t *)dst = (int16_t)s8(*src++);
            break;
        default: /* 16 to 8 bits */
            dst[0] = to8(*(const int16_t *)src);
            src += 2;
        }
        dst += 1u << os;
    }
    if (used)
        *used = src_len - (in << is);
    return dst_len - (out << os);
}

/* ---- mode 11 (0x100450a9-0x1004523c); the original's globals 0x10079cf4-0x10079d10 ---- */

static uint32_t g_out_left, g_in_left;
static int32_t g_l, g_r, g_l2, g_r2, g_dl, g_dr;
static uint8_t *g_dst;
static const uint8_t *g_src;

static int32_t read_sample(const uint8_t *st)
{
    int32_t v;
    if (!st[IN16]) {
        v = (int32_t)*g_src * 0x100 - 0x8000;
        g_src++;
        g_in_left--;
    } else {
        v = *(const int16_t *)g_src;
        g_src += 2;
        g_in_left -= 2;
    }
    return v;
}

static int32_t peek_sample(const uint8_t *st, const uint8_t *p)
{
    return st[IN16] ? *(const int16_t *)p : ((int32_t)*p - 0x80) * 0x100;
}

/* 0x100450a9: averages DOWN frames */
static void read_average(const uint8_t *st)
{
    g_l = g_r = 0;
    uint8_t n = st[DOWN];
    do {
        int32_t v = read_sample(st);
        g_l += v;
        if (st[IN_ST])
            v = read_sample(st);
        g_r += v;
    } while (g_in_left && --n);
    g_l >>= st[DOWN_SHIFT] & 0x1f;
    g_r >>= st[DOWN_SHIFT] & 0x1f;
}

static void write_frame(const uint8_t *st)
{
    if (!st[OUT16]) {
        *g_dst++ = (uint8_t)((uint32_t)(g_l + 0x8000) >> 8);
        g_out_left--;
    } else {
        *(int16_t *)g_dst = (int16_t)g_l;
        g_dst += 2;
        g_out_left -= 2;
    }
    if (!st[OUT_ST])
        return;
    if (!st[OUT16]) {
        *g_dst++ = (uint8_t)((uint32_t)(g_r + 0x8000) >> 8);
        g_out_left--;
    } else {
        *(int16_t *)g_dst = (int16_t)g_r;
        g_dst += 2;
        g_out_left -= 2;
    }
}

/* 0x10045186: this frame, and the next to interpolate towards */
static void read_pair(const uint8_t *st)
{
    g_l = read_sample(st);
    g_r = st[IN_ST] ? read_sample(st) : g_l;
    if (!g_in_left) {
        g_r2 = g_r;
        g_l2 = g_l;
        return;
    }
    uint32_t size = st[IN16] ? 2 : 1;
    g_r2 = peek_sample(st, g_src);
    g_l2 = g_r2;
    if (st[IN_ST])
        g_r2 = peek_sample(st, g_src + size);
}

/* 0x1004523c: UP frames stepping towards the next */
static void write_steps(const uint8_t *st)
{
    g_dl = (g_l2 - g_l) >> (st[UP_SHIFT] & 0x1f);
    g_dr = (g_r2 - g_r) >> (st[UP_SHIFT] & 0x1f);
    uint8_t n = st[UP];
    do {
        write_frame(st);
        g_l += g_dl;
        g_r += g_dr;
    } while (g_out_left && --n);
}

/* 0x10045394: converts src into dst; returns the bytes written, *used the bytes read */
uint32_t __stdcall resample(const uint8_t *st, uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len,
                            uint32_t *used)
{
    if (st[MODE] < 10)
        return convert(st[MODE], dst, src, dst_len, src_len, used);
    if (st[MODE] == 10) {
        if (dst_len >= src_len)
            dst_len = src_len;
        memcpy(dst, src, dst_len);
        if (used)
            *used = dst_len;
        return dst_len;
    }
    g_out_left = dst_len;
    g_in_left = src_len;
    g_dst = dst;
    g_src = src;
    if (st[DOWN] > st[UP]) {
        while (g_out_left && g_in_left) {
            read_average(st);
            write_frame(st);
        }
    } else {
        while (g_out_left && g_in_left) {
            read_pair(st);
            write_steps(st);
        }
    }
    if (used)
        *used = src_len - g_in_left;
    return dst_len - g_out_left;
}
