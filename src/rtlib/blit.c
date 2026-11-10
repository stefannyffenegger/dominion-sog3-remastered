/* The sprite engine's hand-written assembly routines, rewritten in portable C.
 * Each is checked byte for byte against the original by tests/blit_compare.c.
 *
 * Bitmaps ("DIBs") are a 0x24-byte header followed by the pixels: width at +4, row pitch
 * at +8, height at +0xc, flags at +0x10 (0x40: rows stored top-down, 0x80: run-length
 * coded), pixel pointer at +0x20. Bottom-up bitmaps store the last row first. */
#include "blit.h"

#define F(type, p, off) (*(type *)((uint8_t *)(p) + (off)))

/* 0x1005f6ac. Leaves out untouched when the rectangles don't overlap. */
int __stdcall rect_intersect(int32_t *out, const int32_t *a, const int32_t *b)
{
    int32_t l = a[0] > b[0] ? a[0] : b[0];
    int32_t r = a[2] < b[2] ? a[2] : b[2];
    if (l >= r)
        return 0;
    int32_t t = a[1] > b[1] ? a[1] : b[1];
    int32_t bo = b[3] < a[3] ? b[3] : a[3];
    if (t >= bo)
        return 0;
    out[0] = l, out[1] = t, out[2] = r, out[3] = bo;
    return 1;
}

/* 0x1005f70a */
int __stdcall rect_union(int32_t *out, const int32_t *a, const int32_t *b)
{
    int32_t l = a[0] < b[0] ? a[0] : b[0];
    int32_t r = a[2] > b[2] ? a[2] : b[2];
    int32_t t = a[1] < b[1] ? a[1] : b[1];
    int32_t bo = b[3] > a[3] ? b[3] : a[3];
    out[0] = l, out[1] = t, out[2] = r, out[3] = bo;
    return 1;
}

/* 0x10062ed8: n palette entries scaled by step/steps, rounded; the flag byte is copied.
 * 8-bit multiply and divide as in the original (steps < 256); n == 0 means 65536. */
void __stdcall palette_fade(uint8_t *dst, const uint8_t *src, uint32_t step, uint32_t steps, uint32_t n)
{
    uint16_t half = (uint16_t)(steps & 0xffff) >> 1;
    uint8_t s = (uint8_t)step, d = (uint8_t)steps;
    uint32_t count = n & 0xffff ? n & 0xffff : 0x10000;
    for (; count; count--, src += 4, dst += 4) {
        dst[3] = src[3];
        for (int i = 0; i < 3; i++)
            dst[i] = (uint8_t)((uint16_t)(s * src[i] + half) / d);
    }
}

/* First byte of rectangle row y (in the rectangle's top-down order) */
static uint8_t *row_at(void *bm, int x, int y, int h)
{
    uint32_t pitch = F(uint32_t, bm, 8);
    uint32_t row = (F(uint32_t, bm, 0x10) & 0x40) ? (uint32_t)y : F(uint32_t, bm, 0xc) - y - h;
    return F(uint8_t *, bm, 0x20) + row * pitch + x;
}

/* 0x100630cc: fills a w x h rectangle at x, y with one color (h >= 1). The original walks
 * the rows in memory order: from the bottom row up for bottom-up bitmaps. */
void __stdcall dib_fill8(void *bm, int x, int y, int w, int h, uint32_t color)
{
    uint8_t *p = row_at(bm, x, y, h);
    uint32_t pitch = F(uint32_t, bm, 8);
    for (; h; h--, p += pitch)
        for (int i = 0; i < w; i++)
            p[i] = (uint8_t)color;
}

/* The original's `inc si`: advances only the low 16 bits of the pointer */
static const uint8_t *inc_si(const uint8_t *p)
{
    uintptr_t v = (uintptr_t)p;
    return (const uint8_t *)((v & ~(uintptr_t)0xffff) | ((v + 1) & 0xffff));
}

/* 0x100629f4: copies a w x h block from src at sx, sy to dst at dx, dy. flip: low byte
 * mirrors horizontally, second byte vertically. Run-length coded sources are drawn
 * opaque. Reproduces the original exactly, including:
 *  - raw sources with a horizontal flip are not mirrored: the copy starts at src.w - sx
 *    and every row moves the source on by twice the width;
 *  - run-length rows after the first read their offset as 32 bits from the 16-bit table;
 *  - a literal of length 0 copies 512 bytes; runs advance the source with a 16-bit add. */
void __stdcall dib_copy(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip)
{
    if (!w || !h)
        return;
    int fx = (flip & 0xff) != 0, fy = (flip & 0xff00) != 0;
    uint32_t dpitch = F(uint32_t, dst, 8), dh = F(uint32_t, dst, 0xc);
    int32_t dgap = (int32_t)dpitch;
    uint32_t drow;
    if (!(F(uint8_t, src, 0x10) & 0x80)) {
        uint32_t spitch = F(uint32_t, src, 8);
        int32_t sgap = (int32_t)spitch;
        uint32_t srow;
        if (!fy) {
            srow = F(uint32_t, src, 0xc) - sy - h;
        } else {
            sgap = -sgap;
            srow = sy + h - 1;
        }
        uint32_t off = srow * spitch + (fx ? F(uint32_t, src, 4) - sx : (uint32_t)sx);
        const uint8_t *s = F(uint8_t *, src, 0x20) + off;
        if (!(F(uint32_t, dst, 0x10) & 0x40)) {
            drow = dh - dy - h;
        } else {
            dgap = -dgap;
            drow = dy + h - 1;
        }
        uint8_t *d = F(uint8_t *, dst, 0x20) + (drow * dpitch + dx);
        sgap = fx ? sgap + (int32_t)w : sgap - (int32_t)w;
        dgap -= (int32_t)w;
        for (; h; h--) {
            for (uint32_t i = 0; i < w; i++)
                *d++ = *s++;
            d += dgap;
            s += sgap;
        }
        return;
    }
    /* run-length coded source */
    if (fx)
        sx = F(int32_t, src, 4) - (int32_t)w - sx;
    uint32_t row = fy ? F(uint32_t, src, 0xc) - h - sy : (uint32_t)sy;
    const uint8_t *base = F(uint8_t *, src, 0x20);
    const uint8_t *tbl = base + (row + h - 1) * 2;
    const uint8_t *s = base + *(const uint16_t *)tbl;
    if (!(F(uint32_t, dst, 0x10) & 0x40)) {
        drow = dh - dy;
        if (!fy)
            drow -= h;
        else
            dgap = -dgap, drow--;
    } else {
        drow = dy;
        if (!fy)
            dgap = -dgap, drow += h - 1;
    }
    uint8_t *d = F(uint8_t *, dst, 0x20) + (drow * dpitch + dx);
    dgap -= (int32_t)w;
    if (fx)
        d += w - 1, dgap += 2 * (int32_t)w;
    int dir = fx ? -1 : 1;
    for (;;) {
        int32_t rem = (int32_t)w, c = sx;
        int lit;
        /* skip the clipped left part */
        for (;;) {
            uint32_t a = *s++;
            if (a <= 0x7f) {
                s++;
                c -= (int32_t)a;
                if (c >= 0)
                    continue;
                s--;
                c = -c;
                lit = 0;
                break;
            }
            a ^= 0xff;
            if (!a)
                a = *s++;
            s += a;
            c -= (int32_t)a;
            if (c >= 0)
                continue;
            s += c;
            c = -c;
            lit = 1;
            break;
        }
        for (;;) {
            rem -= c;
            if (rem < 0)
                c += rem;
            uint8_t n = (uint8_t)c;
            if (lit) {
                int odd = n & 1;
                n >>= 1;
                if (odd) {
                    *d = *s++;
                    d += dir;
                }
                if (!odd || n) {
                    do {
                        *d = *s++;
                        d += dir;
                        *d = *s++;
                        d += dir;
                    } while (--n);
                }
            } else {
                uint8_t v = *s;
                s = inc_si(s);
                if (n & 1) {
                    *d = v;
                    d += dir;
                }
                for (n >>= 1; n; n--) {
                    *d = v;
                    d += dir;
                    *d = v;
                    d += dir;
                }
            }
            if (rem <= 0)
                break;
            uint32_t code = *s++;
            if (code <= 0x7f) {
                c = (int32_t)code;
                lit = 0;
            } else {
                code ^= 0xff;
                if (!code)
                    code = *s++;
                c = (int32_t)code;
                lit = 1;
            }
        }
        d += dgap;
        if (!--h)
            return;
        tbl -= 2;
        s = base + *(const uint32_t *)tbl;
    }
}
