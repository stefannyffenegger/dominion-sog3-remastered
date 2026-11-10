/* The sprite engine's hand-written assembly routines, rewritten in portable C.
 * Each is checked byte for byte against the original by tests/blit_compare.c.
 *
 * Bitmaps ("DIBs") are a 0x24-byte header followed by the pixels: width at +4, row pitch
 * at +8, height at +0xc, flags at +0x10 (0x40: rows stored top-down, 0x80: run-length
 * coded), pixel pointer at +0x20. Bottom-up bitmaps store the last row first. */
#include "blit.h"
#include <string.h>

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

/* 0x10066992: where a plain source and the destination start, and how far to move on
 * after each row of w pixels. Rows are walked from the block's last row to its first
 * (or the other way for the source with a vertical flip). */
static void plain_setup(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip,
                        const uint8_t **ps, uint8_t **pd, int32_t *psgap, int32_t *pdgap)
{
    int fx = (flip & 0xff) != 0, fy = (flip & 0xff00) != 0;
    uint32_t dpitch = F(uint32_t, dst, 8), dh = F(uint32_t, dst, 0xc);
    int32_t dgap = (int32_t)dpitch;
    uint32_t drow;
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
    *ps = F(uint8_t *, src, 0x20) + off;
    if (!(F(uint32_t, dst, 0x10) & 0x40)) {
        drow = dh - dy - h;
    } else {
        dgap = -dgap;
        drow = dy + h - 1;
    }
    *pd = F(uint8_t *, dst, 0x20) + (drow * dpitch + dx);
    *psgap = fx ? sgap + (int32_t)w : sgap - (int32_t)w;
    *pdgap = dgap - (int32_t)w;
}

/* 0x10062f2c: puts src's pixel wherever dst is 255. Nothing for run-length sources. */
void __stdcall dib_restore(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip)
{
    if (!w || !h || (F(uint32_t, src, 0x10) & 0x80))
        return;
    const uint8_t *s;
    uint8_t *d;
    int32_t sgap, dgap;
    plain_setup(dst, src, w, h, dx, dy, sx, sy, flip, &s, &d, &sgap, &dgap);
    for (; h; h--, d += dgap, s += sgap)
        for (uint32_t i = 0; i < w; i++, d++, s++)
            if (*d == 0xff)
                *d = *s;
}

/* 0x10062ffe: clears dst to 0 wherever src is not 0. Nothing for run-length sources. */
void __stdcall dib_mask(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip)
{
    if (!w || !h || (F(uint32_t, src, 0x10) & 0x80))
        return;
    const uint8_t *s;
    uint8_t *d;
    int32_t sgap, dgap;
    plain_setup(dst, src, w, h, dx, dy, sx, sy, flip, &s, &d, &sgap, &dgap);
    for (; h; h--, d += dgap, s += sgap)
        for (uint32_t i = 0; i < w; i++, d++, s++)
            if (*s)
                *d = 0;
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
        const uint8_t *s;
        uint8_t *d;
        int32_t sgap;
        plain_setup(dst, src, w, h, dx, dy, sx, sy, flip, &s, &d, &sgap, &dgap);
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

/* ---- the run-length merge (0x10059e74 for coded sources) and its helpers ----
 *
 * A run-length coded sprite's pixels are rows of 16-bit-aligned data, stored bottom row
 * first: a 16-bit count of segments (low byte), then per segment a 16-bit word with the
 * type in the top 5 bits and the length in the low 11: 0 transparent, 1 shadow,
 * 2 transparent too (an unused effect), 3 that many pixels follow (padded to even).
 *
 * The table routines (shadow, color map) look up whole 32-bit table entries and the
 * original combines 4 of them at a time; the game's tables only hold values below 256,
 * for which that equals the per-pixel lookup done here. */

/* 0x10064e23: shadow, through the 256-entry table */
static void shadow_plain(uint8_t *p, uint32_t n, const uint32_t *table)
{
    for (; n; n--, p++)
        *p = (uint8_t)table[*p];
}

/* 0x10065000: dithered shadow: black at odd addresses on even rows, even ones on odd */
static void shadow_dither(uint8_t *p, uint32_t n, uint32_t y)
{
    for (; n; n--, p++)
        if (((uintptr_t)p ^ y) & 1)
            *p = 0;
}

/* 0x100651bd and 0x100653ad: copy through a color map, forward or mirrored */
static void cmap_copy(uint8_t *d, const uint8_t *s, uint32_t n, const uint32_t *cmap, int dir)
{
    for (; n; n--, d += dir)
        *d = (uint8_t)cmap[*s++];
}

static void lit_copy(uint8_t *d, const uint8_t *s, uint32_t n, int dir)
{
    if (dir > 0) {
        memmove(d, s, n); /* 0x1006630c, the C library's memmove */
        return;
    }
    for (; n; n--)
        *d-- = *s++; /* 0x1005bc90 */
}

void dib_shadow(uint8_t *p, uint32_t n, const uint32_t *table) { shadow_plain(p, n, table); }
void dib_shadow_dither(uint8_t *p, uint32_t n, uint32_t row) { shadow_dither(p, n, row); }

static const uint8_t *align2(const uint8_t *p) { return p + ((uintptr_t)p & 1); }

/* 0x1005bce0, type 1: the slow paths' shadow */
static void shadow_slow(const MergeEnv *e, uint8_t *p, uint32_t n, uint32_t y)
{
    if (e->dither_slow)
        shadow_dither(p, n, y);
    else
        shadow_plain(p, n, e->shadow);
}

/* 0x10064780: unclipped, unmirrored, top-down screen, no color map. The first row is
 * placed with the bitmap's width, not its pitch; the dither row counts up from dy + 1. */
static void merge_fast(const MergeEnv *e, uint8_t *bits, const uint8_t *p, int dx, int dy, int dw, int stride,
                       uint32_t h)
{
    uint8_t *row = bits + (uint32_t)dy * (uint32_t)dw + dx;
    uint32_t y = (uint32_t)dy;
    for (; h; h--) {
        y++;
        uint8_t *d = row;
        row += stride;
        uint32_t nseg = p[0];
        p += 2;
        for (; nseg; nseg--) {
            uint16_t v = *(const uint16_t *)p;
            p += 2;
            uint32_t type = v >> 11, len = v & 0x7ff;
            if (type == 1) {
                if (e->dither_fast)
                    shadow_dither(d, len, y);
                else
                    shadow_plain(d, len, e->shadow);
            } else if (type == 3) {
                memcpy(d, p, len);
                p = align2(p + len);
            }
            d += len;
        }
    }
}

void dib_merge_rle(const MergeEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, uint32_t sx,
                   uint32_t sy, uint32_t flip, const uint32_t *cmap)
{
    int fx = (flip & 0xff) != 0;
    uint32_t sw1 = F(uint32_t, src, 4) - 1, sh1 = F(uint32_t, src, 0xc) - 1;
    uint32_t x0, x1, ylast = h + sy - 1;
    int hclip;
    if (!fx) {
        x0 = sx;
        x1 = w + sx - 1;
        hclip = !(sx == 0 && sw1 <= x1);
    } else {
        x0 = sw1 - (w + sx - 1);
        x1 = sw1 - sx;
        hclip = !(x0 == 0 && sw1 <= x1);
    }
    int vclip = ylast < sh1;
    uint32_t pitch = F(uint32_t, dst, 8);
    int32_t stride;
    if (!(F(uint8_t, dst, 0x10) & 0x40)) {
        dy = (int)(F(uint32_t, dst, 0xc) - dy - h);
        stride = (int32_t)pitch;
    } else if (!(flip & 0xff00)) {
        dy = dy + (int)h - 1;
        stride = -(int32_t)pitch;
    } else {
        stride = (int32_t)pitch;
    }
    int ydir = stride < 0 ? -1 : 1;
    const uint8_t *p = F(uint8_t *, src, 0x20);
    uint8_t *bits = F(uint8_t *, dst, 0x20);
    int dir = fx ? -1 : 1;
    uint32_t y = (uint32_t)dy;

    if (hclip || sy || vclip) {
        uint8_t *row = bits + pitch * (uint32_t)dy + dx + (fx ? w - 1 : 0);
        uint32_t skip = sh1 - ylast; /* coded rows below the visible part */
        uint8_t *d = NULL;
        uint32_t yrow = 0, lskip = 0, rskip = 0;
        for (uint32_t r = 0; r <= sh1 - sy; r++) {
            uint32_t xpos = 0xffffffff;
            int shown = skip <= r;
            if (shown) {
                d = row;
                row += stride;
                yrow = y;
                y += ydir;
            }
            uint32_t nseg = p[0];
            p += 2;
            for (; nseg; nseg--) {
                uint16_t v = *(const uint16_t *)p;
                p += 2;
                uint32_t type = v >> 11, len = v & 0x7ff;
                int vis = 1; /* 0 hidden, 1 whole, else partly */
                if (hclip) {
                    uint32_t s0 = xpos + 1;
                    xpos = s0 + len - 1;
                    if (xpos < x0 || x1 < s0) {
                        vis = 0;
                    } else if (s0 < x0 || x1 < xpos) {
                        vis = 2;
                        lskip = s0 < x0 ? x0 - s0 : 0;
                        rskip = x1 < xpos ? xpos - x1 : 0;
                    }
                }
                uint32_t off = vis == 2 ? lskip : 0, n = vis == 2 ? len - lskip - rskip : len;
                if (shown && vis) {
                    if (type == 3) {
                        if (!cmap)
                            lit_copy(d, p + off, n, dir);
                        else
                            cmap_copy(d, p + off, n, cmap, dir);
                    } else if (type == 1) {
                        shadow_slow(e, fx ? d + 1 - n : d, n, yrow);
                    }
                    d += fx ? -(int32_t)n : (int32_t)n;
                }
                if (type == 3)
                    p = align2(p + len);
            }
        }
        return;
    }
    if (!fx && (F(uint8_t, dst, 0x10) & 0x40) && !cmap) {
        merge_fast(e, bits, p, dx, dy, F(int32_t, dst, 4), stride, h);
        return;
    }
    uint8_t *row = bits + pitch * (uint32_t)dy + dx + (fx ? F(uint32_t, src, 4) - 1 : 0);
    for (; h; h--, y += ydir) {
        uint8_t *d = row;
        row += stride;
        uint32_t nseg = p[0];
        p += 2;
        for (; nseg; nseg--) {
            uint16_t v = *(const uint16_t *)p;
            p += 2;
            uint32_t type = v >> 11, len = v & 0x7ff;
            if (type == 3) {
                if (!cmap)
                    lit_copy(d, p, len, dir);
                else
                    cmap_copy(d, p, len, cmap, dir);
                p = align2(p + len);
            } else if (type == 1) {
                shadow_slow(e, fx ? d + 1 - len : d, len, y);
            }
            d += fx ? -(int32_t)len : (int32_t)len;
        }
    }
}

/* ---- the plain merge (0x1005fcec, and 0x10060c14 with a color map) ----
 *
 * Its run-length format is the copy routine's, with special values: codes 0 end a row,
 * 1-0x7f are a run (count, value), 0x80-0xfe a literal of ~code raw pixels, 0xff a count
 * followed by that many effect pixels. Run and effect values: 0 transparent, 1 shadow,
 * 2 highlight (a byte table), 3 the spot color, others drawn. With a color map every
 * source value is mapped first (the special values are checked after mapping).
 * Loops over shadow, highlight and effect pixels count in 8 bits, so 0 means 256. */

typedef struct PlainCtx {
    const PlainEnv *e;
    const uint32_t *cmap;
    int dir;
} PlainCtx;

static uint8_t pmap(const PlainCtx *c, uint8_t v) { return c->cmap ? (uint8_t)c->cmap[v] : v; }

static void peffect(const PlainCtx *c, uint8_t *d, uint8_t v)
{
    if (v == 0)
        return;
    if (v == 1)
        *d = (uint8_t)c->e->shadow[*d];
    else if (v == 2)
        *d = c->e->hilite[*d];
    else
        *d = v == 3 ? c->e->spot : v;
}

/* the three item kinds; n as the original's count register */
static uint8_t *p_lit(const PlainCtx *c, uint8_t *d, const uint8_t **s, uint32_t n)
{
    for (; n; n--, d += c->dir)
        *d = pmap(c, *(*s)++);
    return d;
}

static uint8_t *p_fx(const PlainCtx *c, uint8_t *d, const uint8_t **s, uint8_t n)
{
    do {
        peffect(c, d, pmap(c, *(*s)++));
        d += c->dir;
    } while (--n);
    return d;
}

static uint8_t *p_run(const PlainCtx *c, uint8_t *d, const uint8_t **s, uint32_t n)
{
    uint8_t v = pmap(c, *(*s)++);
    if (v >= 3) {
        if (v == 3)
            v = c->e->spot;
        for (uint32_t i = 0; i < n; i++, d += c->dir)
            *d = v;
    } else if (v == 0) {
        d += c->dir * (int32_t)n;
    } else {
        uint8_t k = (uint8_t)n;
        do {
            peffect(c, d, v);
            d += c->dir;
        } while (--k);
    }
    return d;
}

static uint8_t ptrans(const PlainCtx *c, uint8_t v, uint8_t under)
{
    if (v == 1)
        return (uint8_t)c->e->shadow[under];
    if (v == 2)
        return c->e->hilite[under];
    return v == 3 ? c->e->spot : v;
}

/* Two pixels a -> *x and b -> *y, shadows looked up under tx and ty (see plain_raw) */
static void ppair(const PlainCtx *c, uint8_t a, uint8_t b, uint8_t *x, uint8_t *y, const uint8_t *tx, const uint8_t *ty)
{
    a = pmap(c, a), b = pmap(c, b);
    if (!a && !b)
        return;
    uint8_t ta = ptrans(c, a, *tx), tb = ptrans(c, b, *ty);
    if (ta) {
        *x = ta;
        if (tb)
            *y = tb;
    } else {
        *y = tb;
    }
}

static void psingle(const PlainCtx *c, uint8_t a, uint8_t *x, const uint8_t *t)
{
    a = pmap(c, a);
    if (a)
        *x = ptrans(c, a, *t);
}

/* plain bitmaps: pixel pairs. Quirks kept: a pair whose left pixel transforms to 0
 * writes the right one even if it is 0 too, but a right pixel transforming to 0 after a
 * drawn left one is skipped; mirrored, the odd last pixel looks its shadow up one pixel
 * to the right; with a color map, each row moves the source on by the map's address
 * instead of the row gap (so only the first row is right). */
static void plain_raw(const PlainCtx *c, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy,
                      int fx, int fy)
{
    uint32_t spitch = F(uint32_t, src, 8), dpitch = F(uint32_t, dst, 8);
    int32_t sgap = (int32_t)spitch, dgap = (int32_t)dpitch;
    uint32_t srow, drow;
    if (!fy) {
        srow = F(uint32_t, src, 0xc) - sy - h;
    } else {
        sgap = -sgap;
        srow = sy + h - 1;
    }
    const uint8_t *s = F(uint8_t *, src, 0x20) + (srow * spitch + (fx ? F(uint32_t, src, 4) - sx : (uint32_t)sx));
    if (!(F(uint32_t, dst, 0x10) & 0x40)) {
        drow = F(uint32_t, dst, 0xc) - dy - h;
    } else {
        dgap = -dgap;
        drow = dy + h - 1;
    }
    uint8_t *d = F(uint8_t *, dst, 0x20) + (drow * dpitch + dx);
    sgap = fx ? sgap + (int32_t)w : sgap - (int32_t)w;
    dgap -= (int32_t)w;
    if (c->cmap)
        sgap = (int32_t)(uintptr_t)c->cmap;
    uint32_t quads = w >> 2, rem = w & 3;
    for (; h; h--) {
        if (!fx) {
            for (uint32_t k = 0; k < quads; k++, s += 4, d += 4) {
                ppair(c, s[0], s[1], d, d + 1, d, d + 1);
                ppair(c, s[2], s[3], d + 2, d + 3, d + 2, d + 3);
            }
            if (rem >= 2) {
                ppair(c, s[0], s[1], d, d + 1, d, d + 1);
                s += 2, d += 2;
            }
            if (rem & 1) {
                psingle(c, s[0], d, d);
                s++, d++;
            }
        } else {
            for (uint32_t k = 0; k < quads; k++, d += 4) {
                s -= 4;
                ppair(c, s[0], s[1], d + 3, d + 2, d + 3, d + 2);
                ppair(c, s[2], s[3], d + 1, d, d + 1, d);
            }
            if (rem >= 2) {
                s -= 2;
                ppair(c, s[0], s[1], d + 1, d, d + 1, d);
                d += 2;
            }
            if (rem & 1) {
                s--;
                psingle(c, s[0], d, d + 1);
                d++;
            }
        }
        d += dgap;
        s += sgap;
    }
}

void dib_merge_plain(const PlainEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy,
                     uint32_t flip, const uint32_t *cmap)
{
    if (!w || !h)
        return;
    int fx = (flip & 0xff) != 0, fy = (flip & 0xff00) != 0;
    PlainCtx c = {e, cmap, fx ? -1 : 1};
    if (!(F(uint8_t, src, 0x10) & 0x80)) {
        plain_raw(&c, dst, src, w, h, dx, dy, sx, sy, fx, fy);
        return;
    }
    uint32_t dpitch = F(uint32_t, dst, 8), dh = F(uint32_t, dst, 0xc);
    int32_t dgap = (int32_t)dpitch;
    uint32_t drow;
    /* run-length coded source (same setup as dib_copy) */
    if (fx)
        sx = F(int32_t, src, 4) - (int32_t)w - sx;
    int lclip = sx != 0, rclip = sx + w != F(uint32_t, src, 4);
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
    for (;;) {
        int32_t rem = (int32_t)w;
        int kind = -1; /* item left over from the skip: 0 run, 1 literal, 2 effects */
        int32_t n = 0;
        if (lclip) {
            n = sx;
            for (;;) {
                uint32_t a = *s++;
                if (a <= 0x7f) {
                    s++;
                    n -= (int32_t)a;
                    if (n >= 0)
                        continue;
                    s--;
                    kind = 0;
                } else if ((a ^= 0xff) != 0) {
                    s += a;
                    n -= (int32_t)a;
                    if (n >= 0)
                        continue;
                    s += n;
                    kind = 1;
                } else {
                    a = *s++;
                    s += a;
                    n -= (int32_t)a;
                    if (n >= 0)
                        continue;
                    s += n;
                    kind = 2;
                }
                n = -n;
                break;
            }
        }
        for (;;) {
            if (kind < 0) {
                if (rclip) {
                    if (rem <= 0)
                        break;
                } 
                uint32_t code = *s++;
                if (!rclip && code == 0)
                    break;
                if (code <= 0x7f) {
                    kind = 0, n = (int32_t)code;
                } else if ((code ^= 0xff) != 0) {
                    kind = 1, n = (int32_t)code;
                } else {
                    kind = 2, n = *s++;
                }
            }
            if (rclip) {
                rem -= n;
                if (rem < 0)
                    n += rem;
            }
            if (kind == 0)
                d = p_run(&c, d, &s, (uint32_t)n);
            else if (kind == 1)
                d = p_lit(&c, d, &s, (uint32_t)n);
            else
                d = p_fx(&c, d, &s, (uint8_t)n);
            kind = -1;
        }
        d += dgap;
        if (!--h)
            return;
        if (rclip) {
            tbl -= 2;
            s = base + *(const uint16_t *)tbl;
        }
    }
}

/* ---- the effect merge (0x1005adb3): run-length sprites drawn with a blit effect ----
 *
 * The same walk as dib_merge_rle without its fast path; literal pixels go through the
 * effect instead of being copied. Effect 1 blends (or screen-doors) with the screen,
 * 2 and 3 look pixels up in the effect table. */

/* The blends work 4 pixels at a time once the destination is aligned (or right away when
 * it already is) and finish the last 1-3 in separate code; short unaligned runs go pixel
 * by pixel. The code for a single last pixel takes the source pixel 2 further on. */
static uint32_t blend_tail(const uint8_t *d, uint32_t n)
{
    if (!((uintptr_t)d & 3))
        return n & 3;
    if (n <= 12)
        return 0;
    return (n - ((0u - (uint32_t)(uintptr_t)d) & 3)) & 3;
}

/* 0x100653dd: blend, d = blend[s][d] */
void fx_blend(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend)
{
    uint32_t tail = blend_tail(d, n);
    for (n -= tail; n; n--, d++, s++)
        *d = blend[((uint32_t)*s << 8) + *d];
    if (tail == 1)
        s += 2;
    for (; tail; tail--, d++, s++)
        *d = blend[((uint32_t)*s << 8) + *d];
}

/* 0x100656a0: blend through a color map, d = blend[cmap[s]][d]. When d is 4-byte
 * aligned, the original finishes in 0x100653dd's code (it jumps through that function's
 * table): the last 1-3 pixels are blended without the color map, and it returns
 * removing 16 bytes of arguments instead of 20. */
void fx_blend_cmap(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend, const uint32_t *cmap)
{
    int aligned = !((uintptr_t)d & 3);
    uint32_t tail = blend_tail(d, n);
    for (n -= tail; n; n--, d++, s++)
        *d = blend[(cmap[*s] << 8) + *d];
    if (aligned) {
        fx_blend(d, s, tail, blend); /* aligned too, so the same last-pixel slip */
        return;
    }
    if (tail == 1)
        s += 2;
    for (; tail; tail--, d++, s++)
        *d = blend[(cmap[*s] << 8) + *d];
}

/* 0x10065a19: screen door, the source pixels at odd addresses on even rows and at even
 * addresses on odd rows */
void fx_dither(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t row)
{
    for (; n; n--, d++, s++)
        if (((uintptr_t)d ^ row) & 1)
            *d = *s;
}

/* 0x10059b6e: an effect run, left to right */
void dib_fx_run(const FxEnv *e, uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, uint32_t row,
                   const uint32_t *cmap)
{
    if (fx == 1) {
        if (e->dither)
            fx_dither(d, s, n, row);
        else if (!cmap)
            fx_blend(d, s, n, e->blend);
        else
            fx_blend_cmap(d, s, n, e->blend, cmap);
    } else if (fx == 2) {
        for (; n; n--, d++, s++)
            *d = (uint8_t)e->table[(*s - 2) * 0x100 + *d];
    } else if (fx == 3) {
        for (; n; n--, d++, s++)
            *d = (uint8_t)e->table[*s + arg * 0x100];
    }
}

/* 0x10059cda: an effect run, mirrored (right to left). No screen door; effect 3 looks
 * up the screen's pixel instead of the sprite's. */
void dib_fx_run_mirrored(const FxEnv *e, uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg,
                            const uint32_t *cmap)
{
    if (fx == 1) {
        for (; n; n--, d--, s++)
            *d = e->blend[((cmap ? cmap[*s] : *s) << 8) + *d];
    } else if (fx == 2) {
        for (; n; n--, d--, s++)
            *d = (uint8_t)e->table[(*s - 2) * 0x100 + *d];
    } else if (fx == 3) {
        for (; n; n--, d--)
            *d = (uint8_t)e->table[*d + arg * 0x100];
    }
}

/* The shadow of the effect merge: the original passes an uninitialized row (shadow_row) */
static void fx_shadow(const FxEnv *e, uint8_t *p, uint32_t n)
{
    if (e->m.dither_slow)
        shadow_dither(p, n, e->shadow_row);
    else
        shadow_plain(p, n, e->m.shadow);
}

void dib_merge_fx_rle(const FxEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, uint32_t sx,
                      uint32_t sy, uint32_t flip, uint32_t fx, int32_t arg, const uint32_t *cmap)
{
    int mir = (flip & 0xff) != 0;
    uint32_t sw1 = F(uint32_t, src, 4) - 1, sh1 = F(uint32_t, src, 0xc) - 1;
    uint32_t x0, x1, ylast = h + sy - 1;
    int hclip;
    if (!mir) {
        x0 = sx;
        x1 = w + sx - 1;
        hclip = !(sx == 0 && sw1 <= x1);
    } else {
        x0 = sw1 - (w + sx - 1);
        x1 = sw1 - sx;
        hclip = !(x0 == 0 && sw1 <= x1);
    }
    int vclip = ylast < sh1;
    uint32_t pitch = F(uint32_t, dst, 8);
    int32_t stride;
    if (!(F(uint8_t, dst, 0x10) & 0x40)) {
        dy = (int)(F(uint32_t, dst, 0xc) - dy - h);
        stride = (int32_t)pitch;
    } else if (!(flip & 0xff00)) {
        dy = dy + (int)h - 1;
        stride = -(int32_t)pitch;
    } else {
        stride = (int32_t)pitch;
    }
    const uint8_t *p = F(uint8_t *, src, 0x20);
    uint8_t *bits = F(uint8_t *, dst, 0x20);

    if (hclip || sy || vclip) {
        uint8_t *row = bits + pitch * (uint32_t)dy + dx + (mir ? w - 1 : 0);
        uint32_t skip = sh1 - ylast;
        uint8_t *d = NULL;
        uint32_t lskip = 0, rskip = 0;
        for (uint32_t r = 0; r <= sh1 - sy; r++) {
            uint32_t xpos = 0xffffffff;
            int shown = skip <= r;
            if (shown) {
                d = row;
                row += stride;
            }
            uint32_t nseg = p[0];
            p += 2;
            for (; nseg; nseg--) {
                uint16_t v = *(const uint16_t *)p;
                p += 2;
                uint32_t type = v >> 11, len = v & 0x7ff;
                int vis = 1;
                if (hclip) {
                    uint32_t s0 = xpos + 1;
                    xpos = s0 + len - 1;
                    if (xpos < x0 || x1 < s0) {
                        vis = 0;
                    } else if (s0 < x0 || x1 < xpos) {
                        vis = 2;
                        lskip = s0 < x0 ? x0 - s0 : 0;
                        rskip = x1 < xpos ? xpos - x1 : 0;
                    }
                }
                uint32_t off = vis == 2 ? lskip : 0, n = vis == 2 ? len - lskip - rskip : len;
                if (shown && vis) {
                    if (type == 3) {
                        if (!mir)
                            dib_fx_run(e, d, p + off, n, fx, arg, r, cmap);
                        else
                            dib_fx_run_mirrored(e, d, p + off, n, fx, arg, cmap);
                    } else if (type == 1)
                        fx_shadow(e, mir ? d + 1 - n : d, n);
                    d += mir ? -(int32_t)n : (int32_t)n;
                }
                if (type == 3)
                    p = align2(p + len);
            }
        }
        return;
    }
    uint8_t *row = bits + pitch * (uint32_t)dy + dx + (mir ? F(uint32_t, src, 4) - 1 : 0);
    for (; h; h--) {
        uint8_t *d = row;
        row += stride;
        uint32_t nseg = p[0];
        p += 2;
        for (; nseg; nseg--) {
            uint16_t v = *(const uint16_t *)p;
            p += 2;
            uint32_t type = v >> 11, len = v & 0x7ff;
            if (type == 3) {
                if (!mir)
                    dib_fx_run(e, d, p, len, fx, arg, e->fast_row, cmap);
                else
                    dib_fx_run_mirrored(e, d, p, len, fx, arg, cmap);
                p = align2(p + len);
            } else if (type == 1)
                fx_shadow(e, mir ? d + 1 - len : d, len);
            d += mir ? -(int32_t)len : (int32_t)len;
        }
    }
}

/* ---- points along a line and a curve (0x10062ccc, 0x10062c34) ---- */

static int32_t line_part(int32_t d, uint32_t step, uint32_t steps)
{
    uint32_t half = steps >> 1;
    if (d >= 0)
        return (int32_t)(((uint64_t)(uint32_t)d * step + half) / steps);
    return (int32_t)(((int64_t)d * (int32_t)step - half) / (int32_t)steps);
}

/* 0x10062ccc: the point step/steps of the way from (x0, y0) to (x1, y1), rounded */
void line_point(uint32_t step, uint32_t steps, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *out)
{
    out[0] = line_part(x1 - x0, step, steps) + x0;
    out[1] = line_part(y1 - y0, step, steps) + y0;
}

static int32_t curve_part(uint32_t t, uint32_t u, int32_t c, int32_t end)
{
    uint64_t sum = (uint64_t)((int64_t)(int32_t)u * (int32_t)(((uint32_t)c << 2) - (uint32_t)end)) +
                   (uint64_t)((int64_t)(int32_t)(t * t) * end);
    return (int32_t)(uint32_t)(sum >> 20);
}

/* 0x10062c34: the point step/steps along the curve from (x0, y0) to (x1, y1) that is
 * pulled towards (cx, cy), in 10-bit fixed point. Returns the fraction it used. */
uint32_t curve_point(uint32_t step, uint32_t steps, int32_t x0, int32_t y0, int32_t cx, int32_t cy, int32_t x1,
                     int32_t y1, int32_t *out)
{
    uint32_t t = ((step << 10) + (steps >> 1)) / steps;
    uint32_t u = t * (0x400 - t);
    out[0] = curve_part(t, u, cx - x0, x1 - x0) + x0;
    out[1] = curve_part(t, u, cy - y0, y1 - y0) + y0;
    return t;
}

/* ---- the pixel at a point (0x10066a1c, 0x10062d30) ---- */

/* 0x10066a1c: the pixel of a plain bitmap, or of one in the copy routine's run-length
 * format (then the value or literal byte that covers x) */
uint32_t dib_pixel_plain(const void *bm, int x, int y, uint32_t flip)
{
    const uint8_t *bits = F(const uint8_t *, bm, 0x20);
    uint32_t row = (flip & 0xff00) ? (uint32_t)y : F(uint32_t, bm, 0xc) - y - 1;
    int32_t col = (flip & 0xff) ? (int32_t)(F(uint32_t, bm, 4) - x - 1) : x;
    if (!(F(uint32_t, bm, 0x10) & 0x80))
        return bits[row * F(uint32_t, bm, 8) + (uint32_t)col];
    /* (the rows are counted the other way round here) */
    row = (flip & 0xff00) ? F(uint32_t, bm, 0xc) - y - 1 : (uint32_t)y;
    const uint8_t *p = bits + *(const uint16_t *)(bits + row * 2);
    for (;;) {
        uint32_t code = *p++;
        if (code <= 0x7f) { /* a run */
            p++;
            col -= (int32_t)code;
            if (col < 0)
                return p[-1];
        } else {
            code ^= 0xff;
            if (!code)
                code = *p++;
            p += code;
            col -= (int32_t)code;
            if (col < 0)
                return p[col];
        }
    }
}

/* 0x10062d30: whether a sprite bitmap is solid at x, y: 0xff or 0 for a run-length
 * coded one (shadow counts as empty, the unused type 2 as solid), the pixel itself for
 * a plain one. Mirrored, the coded case uses the row pitch instead of the width. */
uint32_t dib_pixel(const void *bm, int x, int y, uint32_t flip)
{
    if (!(F(uint8_t, bm, 0x10) & 0x80))
        return dib_pixel_plain(bm, x, y, flip);
    int32_t col = (flip & 0xff) ? F(int32_t, bm, 8) - 1 - x : x;
    int32_t row = (flip & 0xff00) ? y : F(int32_t, bm, 0xc) - 1 - y;
    const uint8_t *p = F(const uint8_t *, bm, 0x20);
    for (int32_t r = 0; r <= row; r++) {
        int32_t pos = -1;
        uint32_t nseg = p[0];
        p += 2;
        for (; nseg; nseg--) {
            uint16_t v = *(const uint16_t *)p;
            p += 2;
            uint32_t type = v >> 11, len = v & 0x7ff;
            pos += (int32_t)len;
            int hit = r == row && col <= pos;
            if (type == 3) {
                if (hit)
                    return 0xff;
                p = align2(p + len);
            } else if (hit)
                return type <= 1 ? 0 : 0xff;
        }
    }
    return 0;
}

/* ---- bitmap overlap (0x10064415, for SpriteHitDetect) ----
 * 1 when two bitmaps both have a non-zero pixel somewhere in a w x h area, given each
 * one's own coordinates of that area. Run-length bitmaps here are in the copy routine's
 * format, not the sprites' one. A flip only moves the area to the other side of the
 * bitmap; the pixels are not mirrored. */

/* pixel x of row `row` of a bitmap in the copy routine's run-length format */
static uint8_t copy_coded_pixel(const void *bm, uint32_t row, int32_t x)
{
    const uint8_t *bits = F(const uint8_t *, bm, 0x20);
    const uint8_t *p = bits + *(const uint16_t *)(bits + row * 2);
    for (;;) {
        uint32_t code = *p++;
        if (code <= 0x7f) {
            p++;
            x -= (int32_t)code;
            if (x < 0)
                return p[-1];
        } else {
            code ^= 0xff;
            if (!code)
                code = *p++;
            p += code;
            x -= (int32_t)code;
            if (x < 0)
                return p[x];
        }
    }
}

/* State of one coded row being walked: p is at the run's value or the literal's current
 * pixel, rem pixels of the item are left (an 8-bit counter). */
typedef struct {
    const uint8_t *p;
    uint8_t rem;
    int lit;
} CodedPos;

static void coded_load(CodedPos *c)
{
    c->lit = 0;
    c->rem = *c->p++;
    if (c->rem > 0x7f) {
        c->lit = 1;
        c->rem ^= 0xff;
        if (!c->rem)
            c->rem = *c->p++;
    }
}

/* Moves on by count pixels. enter: 0 at the start of an item, 1 inside the current one. */
static void coded_skip(CodedPos *c, int32_t count, int enter)
{
    uint32_t n = c->rem;
    int lit = c->lit;
    for (;; enter = 0) {
        if (!enter) {
            n = *c->p++;
            lit = n > 0x7f;
            if (lit) {
                n ^= 0xff;
                if (!n)
                    n = *c->p++;
            }
        }
        if (lit)
            c->p += n;
        else
            c->p++;
        count -= (int32_t)n;
        if (count < 0)
            break;
    }
    if (lit)
        c->p += count;
    else
        c->p--;
    c->rem = (uint8_t)-count;
    c->lit = lit;
}

/* One step of a row for one bitmap: takes its pixel, 1 if that was a pixel to compare.
 * A transparent run is skipped in one go instead: the other bitmap moves on by the run's
 * length and the row counter by one more than that. (So each skipped run shortens the
 * tested row by a pixel; and when it is the second bitmap's run, the first one, which had
 * already taken its pixel, ends up a pixel ahead.) Returns -1 when the row is over. */
static int coded_step(CodedPos *c, CodedPos *other, int32_t *left, uint8_t *value)
{
    *value = *c->p;
    if (c->lit) {
        c->p++;
        if (!--c->rem)
            coded_load(c);
        return 1;
    }
    if (*value) {
        if (!--c->rem) {
            c->p++;
            coded_load(c);
        }
        return 1;
    }
    *left -= c->rem;
    if (*left <= 0)
        return -1;
    c->p++;
    coded_skip(other, c->rem, 1);
    coded_load(c);
    return 0;
}

/* both bitmaps coded; rows from the area's last to its first */
static uint32_t overlap_coded(const void *a, const void *b, uint32_t w, uint32_t h, int ax, int ay, int bx, int by)
{
    const uint8_t *abits = F(const uint8_t *, a, 0x20), *bbits = F(const uint8_t *, b, 0x20);
    const uint8_t *at = abits + (uint32_t)(ay + (int32_t)h - 1) * 2, *bt = bbits + (uint32_t)(by + (int32_t)h - 1) * 2;
    for (;; at -= 2, bt -= 2) {
        CodedPos ca = {abits + *(const uint16_t *)at, 0, 0}, cb = {bbits + *(const uint16_t *)bt, 0, 0};
        coded_skip(&ca, ax, 0);
        coded_skip(&cb, bx, 0);
        for (int32_t left = (int32_t)w;;) {
            if (--left < 0)
                break;
            uint8_t va, vb;
            int r = coded_step(&cb, &ca, &left, &vb);
            if (r < 0)
                break;
            if (!r)
                continue;
            r = coded_step(&ca, &cb, &left, &va);
            if (r < 0)
                break;
            if (r && va && vb)
                return 1;
        }
        if (!--h)
            return 0;
    }
}

uint32_t __stdcall dib_overlap(void *a, void *b, uint32_t w, uint32_t h, int ax, int ay, int bx, int by, uint32_t flipa,
                               uint32_t flipb)
{
    if (!w || !h)
        return 0;
    if (flipa & 0xff)
        ax = F(int32_t, a, 4) - ax - (int32_t)w;
    if (flipa & 0xff00)
        ay = F(int32_t, a, 0xc) - ay - (int32_t)h;
    if (flipb & 0xff)
        bx = F(int32_t, b, 4) - bx - (int32_t)w;
    if (flipb & 0xff00)
        by = F(int32_t, b, 0xc) - by - (int32_t)h;
    int ca = (F(uint32_t, a, 0x10) & 0x80) != 0, cb = (F(uint32_t, b, 0x10) & 0x80) != 0;
    if (ca && cb)
        return overlap_coded(a, b, w, h, ax, ay, bx, by);
    if (ca && !cb) { /* the original swaps them too */
        void *t = a;
        a = b, b = t;
        int v = ax;
        ax = bx, bx = v;
        v = ay, ay = by, by = v;
        ca = 0, cb = 1;
    }
    const uint8_t *abits = F(const uint8_t *, a, 0x20), *bbits = F(const uint8_t *, b, 0x20);
    uint32_t apitch = F(uint32_t, a, 8), bpitch = F(uint32_t, b, 8);
    /* a plain bitmap next to a coded one may be top-down; two plain ones are taken as
     * bottom-up whatever their flags say */
    int a_top_down = cb && (F(uint32_t, a, 0x10) & 0x40);
    for (uint32_t r = 0; r < h; r++)
        for (uint32_t i = 0; i < w; i++) {
            uint8_t pa, pb;
            if (a_top_down)
                pa = abits[(ay + r) * apitch + ax + i];
            else
                pa = abits[(F(uint32_t, a, 0xc) - 1 - ay - r) * apitch + ax + i];
            if (cb)
                pb = copy_coded_pixel(b, by + r, bx + (int32_t)i);
            else
                pb = bbits[(F(uint32_t, b, 0xc) - 1 - by - r) * bpitch + bx + i];
            if (pa && pb)
                return 1;
        }
    return 0;
}

/* ---- 16-bit screens and the plain C copies (0x100636b0-0x10064334) ----
 * Compiled C in the original, for a screen bitmap of 16-bit pixels drawn from 8-bit
 * sources through the palette. Dominion runs in 8 bits; only the copy and the fill are
 * reachable at all. */

/* 0x100636b0: start pointers, row count and row gaps for a w x h copy; bpp 16 doubles
 * the destination's offsets */
void dib_setup(void *src, void *dst, int sx, int sy, int dx, int dy, int w, int h, uint32_t flip, uint8_t **ps,
               uint8_t **pd, int32_t *rows, int32_t *sgap, int32_t *dgap, int bpp)
{
    int32_t sstep = F(int32_t, src, 8), row;
    if (!(flip & 0xff00))
        row = F(int32_t, src, 0xc) - sy - h;
    else {
        sstep = -sstep;
        row = h + sy - 1;
    }
    if (flip & 0xff)
        sx = F(int32_t, src, 4) - sx;
    *ps = F(uint8_t *, src, 0x20) + (F(int32_t, src, 8) * row + sx);
    int32_t dstep = F(int32_t, dst, 8);
    if (!(F(uint8_t, dst, 0x10) & 0x40))
        row = F(int32_t, dst, 0xc) - dy - h;
    else {
        dstep = -dstep;
        row = h + dy - 1;
    }
    int32_t off = F(int32_t, dst, 8) * row + dx;
    if (bpp == 0x10)
        off *= 2;
    *pd = F(uint8_t *, dst, 0x20) + off;
    sstep += (flip & 0xff) ? w : -w;
    dstep -= w;
    if (bpp == 0x10)
        dstep *= 2;
    *sgap = sstep;
    *dgap = dstep;
    *rows = h;
}

static uint16_t pixel16(uint32_t rgb, int is565)
{
    if (is565)
        return (uint16_t)(((rgb & 0xfc00) >> 5) | ((rgb >> 8) & 0xf800) | ((rgb & 0xf8) >> 3));
    return (uint16_t)(((rgb & 0xf8) >> 3) | ((rgb & 0xf800) >> 6) | ((rgb & 0xf80000) >> 9));
}

/* 0x100637ff (unused): an 8-bit fill that steps 2 bytes per pixel and uses the width as
 * the row pitch */
void __stdcall dib_fill8_wide(void *bm, int w, int h, int x, int y, uint32_t color)
{
    if (!w || !h || (F(uint8_t, bm, 0x10) & 0x80))
        return;
    int32_t bw = F(int32_t, bm, 4);
    int32_t row = (F(uint8_t, bm, 0x10) & 0x40) ? y : F(int32_t, bm, 0xc) - y - h;
    uint8_t *p = F(uint8_t *, bm, 0x20) + (bw * row + x);
    for (; h; h--, p += bw - w)
        for (int i = w; i > 0; i--, p += 2)
            *p = (uint8_t)color;
}

/* 0x100638d8: the 16-bit fill. Each pixel is stored as 32 bits, so 2 zero bytes follow
 * the last pixel of each row. */
void __stdcall dib_fill16(void *bm, int w, int h, int x, int y, uint32_t color)
{
    if (!w || !h || (F(uint8_t, bm, 0x10) & 0x80))
        return;
    int32_t bw = F(int32_t, bm, 4);
    int32_t row = (F(uint8_t, bm, 0x10) & 0x40) ? y : F(int32_t, bm, 0xc) - y - h;
    uint8_t *p = F(uint8_t *, bm, 0x20) + (bw * row + x) * 2;
    for (; h; h--, p += (bw - w) * 2)
        for (int i = w; i > 0; i--, p += 2)
            *(uint32_t *)p = (uint16_t)color;
}

/* The pixel loops. key: 0 copy all, 1 skip source pixels that are 0, 2 also skip where
 * the destination is 0 (the 16-bit version tests 32 bits of the destination). */
static void copy_rows(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip, int key,
                      const uint32_t *pal, int is565)
{
    uint8_t *s, *d;
    int32_t rows, sgap, dgap;
    dib_setup(src, dst, sx, sy, dx, dy, w, h, flip, &s, &d, &rows, &sgap, &dgap, pal ? 0x10 : 8);
    for (; rows; rows--, s += sgap, d += dgap)
        for (int i = w; i > 0; i--, s++, d += pal ? 2 : 1) {
            if (key && !*s)
                continue;
            if (!pal) {
                if (key < 2 || *d)
                    *d = *s;
            } else if (key < 2 || *(uint32_t *)d)
                *(uint16_t *)d = pixel16(pal[*s], is565);
        }
}

/* 0x100639b9 (unused) and 0x10063a78: copy to an 8-bit or a 16-bit bitmap */
void dib_copy_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip, const uint32_t *pal,
                int is565)
{
    if (w && h)
        copy_rows(dst, src, w, h, dx, dy, sx, sy, flip, 0, pal, is565);
}

/* 0x10063c0e and 0x10063cfd (unused): the same skipping source pixels that are 0; only
 * for plain sources without a flip */
void dib_copy_key_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                    const uint32_t *pal, int is565)
{
    if (w && h && !(F(uint8_t, src, 0x10) & 0x80) && !flip)
        copy_rows(dst, src, w, h, dx, dy, sx, sy, 0, 1, pal, is565);
}

/* 0x10063ed7 and 0x10063fc6 (unused): only where both source and destination are not 0.
 * The original tests an uninitialized variable where it means the height; taken as set. */
void dib_copy_both_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                     const uint32_t *pal, int is565)
{
    if (w && !(F(uint8_t, src, 0x10) & 0x80))
        copy_rows(dst, src, w, h, dx, dy, sx, sy, flip, 2, pal, is565);
}

/* 0x10062000: copies w x h pixels from bitmap src at sx, sy (rows counted from the top)
 * into a plain buffer with row pitch `pitch`, at dx, dy. h must not be 0. */
void __stdcall dib_copy_out(uint8_t *dst, int32_t pitch, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx,
                            int sy)
{
    int32_t step = F(int32_t, src, 8);
    if (!(F(uint32_t, src, 0x10) & 0x40)) {
        sy = F(int32_t, src, 0xc) - sy - 1;
        step = -step;
    }
    const uint8_t *s = F(const uint8_t *, src, 0x20) + (sy * F(int32_t, src, 8) + sx);
    uint8_t *d = dst + dy * pitch + dx;
    for (; h; h--, d += pitch, s += step)
        memcpy(d, s, w);
}
