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
