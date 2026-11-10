/* The isometric tile blitters (0x1005c010-0x1005e056, 0x1005e4be), rewritten in portable C.
 * Each is checked byte for byte against the original by tests/blit_compare.c.
 *
 * A tile is a 64 x 32 diamond. Its pixels are stored row by row without the transparent
 * corners: rows 0-14 are 2, 6, ..., 58 pixels wide and centered, rows 15 and 16 are
 * 64 wide, rows 17-31 shrink again. Each row except the two full ones is followed by 2
 * unused bytes. The blitters copy whole rows; the clipped variants differ by which
 * edges of the tile are cut off. Their quirks are kept (see docs/HIDDEN_AND_UNUSED.md). */
#include "tileblit.h"
#include <string.h>

#define F(type, p, off) (*(type *)((uint8_t *)(p) + (off)))
#define PITCH(bm) F(int32_t, bm, 8)
#define BITS(bm) F(uint8_t *, bm, 0x20)

/* row widths (0x1007b530) */
static const uint32_t W[32] = {2,  6,  10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 64,
                               64, 58, 54, 50, 46, 42, 38, 34, 30, 26, 22, 18, 14, 10, 6,  2};
/* row offsets in the tile's pixels (0x1007b5b0). The builder also reads one past the end,
 * which in the original is the first entry of the next table. */
static const int32_t O[33] = {0,   4,   12,  24,  40,  60,  84,  112, 144, 180, 220,
                              264, 312, 364, 420, 480, 544, 608, 668, 724, 776, 824,
                              868, 908, 944, 976, 1004, 1028, 1048, 1064, 1076, 1084, 15};
/* the upper row that starts at or left of column x (0x1007b630) */
static const uint32_t ROW_AT[64] = {15, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9,  9,  8,
                                    8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0,
                                    0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,
                                    8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15};
/* first column of upper row r (0x1007b730) */
static const uint32_t ROW_START[16] = {31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9, 7, 5, 3, 0};

#define HALF(n) ((0x40u - (n)) >> 1) /* first column of a row n pixels wide */

/* ---- tables ---- */

/* 0x1005c471 */
void tile_tables_start_build(TileTables *t)
{
    for (int x = 0; x < 0x40; x++) {
        int r = ROW_AT[x];
        int v = O[r];
        if (r == 0xf)
            v += x;
        else if (x < 0x20) {
            if (!(x & 1))
                v++;
        } else {
            v = W[r] - 1 + v;
            if (x & 1)
                v--;
        }
        t->start[x] = v;
    }
}

/* 0x1005c213 */
void tile_tables_adv_build(TileTables *t)
{
    memset(t->adv, 0, sizeof t->adv);
    memset(t->adv_r, 0, sizeof t->adv_r);
    int lo = 0x1f, hi = 0x20;
    for (int r = 0; r < 0xf; r++) {
        for (int x = lo; x <= hi; x++) {
            int v = (O[r + 1] - O[r]) - (x - lo);
            t->adv[r][x] = v;
            int l = r == 0xe ? lo - 3 : lo - 2;
            t->adv_r[r][x] = v + (x - l);
        }
        lo -= 2;
        hi += 2;
    }
    for (int x = 0; x < 0x40; x++) { /* row 15, the first full one */
        t->adv_r[0xf][x] = 0x40;
        t->adv[0xf][x] = 0x40 - x;
    }
    lo = 0;
    hi = 0x3f;
    for (int r = 0x10; r < 0x20; r++) {
        for (int x = lo; x <= hi; x++) {
            int v = (O[r + 1] - O[r]) - (x - lo);
            t->adv[r][x] = v;
            int l = r == 0x10 ? lo - 3 : lo - 2;
            t->adv_r[r][x] = v + (x - l);
        }
        if (r == 0x10) {
            lo += 3;
            hi -= 3;
        } else {
            lo += 2;
            hi -= 2;
        }
    }
}

/* 0x1005c569 without its debug message: 0x1005c471, 0x1005c213, 0x1005c51e */
void tile_tables_build(TileTables *t, int pitch)
{
    tile_tables_start_build(t);
    tile_tables_adv_build(t);
    for (int i = 0; i < 0xf; i++)
        t->mirror[i] = (i * -2 + 0x1f) * pitch;
}

/* ---- blitters ---- */

/* 0x1005c5d6: a whole tile, its top corner at x + 31 */
void tile_full(const TileTables *t, void *dst, const void *tile, int x, int y)
{
    (void)t;
    tile_clip_bottom(t, dst, tile, 0x40, 0x20, x, y);
}

/* 0x1005cb5c: full width, the top h rows */
void tile_clip_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y)
{
    (void)t;
    (void)w;
    int p = PITCH(dst);
    uint8_t *d = BITS(dst) + p * y + x + 0x1f;
    const uint8_t *s = BITS(tile) + 4;
    uint32_t n = 2;
    uint32_t last = h - 1 < 0xe ? h - 1 : 0xe;
    for (uint32_t r = 0; r <= last; r++) {
        memcpy(d, s, n);
        d += p - 2;
        s += n + 2;
        n += 4;
    }
    if (h <= 0xf)
        return;
    memcpy(d - 1, s, 0x40);
    if (h <= 0x10)
        return;
    memcpy(d + p - 1, s + 0x40, 0x40);
    s += 0x80;
    if (h <= 0x11)
        return;
    n = 0x3a;
    d += p * 2 + 2;
    last = h - 1 > 0x1e ? 0x1f : h - 1;
    for (uint32_t r = 0x11; r <= last; r++) {
        memcpy(d, s, n);
        d += p + 2;
        s += n + 2;
        n -= 4;
    }
}

/* 0x1005ccf1: full width, rows cy to 31 */
void tile_clip_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                   uint32_t cx, uint32_t cy)
{
    (void)t;
    (void)w;
    (void)h;
    (void)cx;
    int p = PITCH(dst);
    const uint8_t *s = BITS(tile) + O[cy] + 4;
    uint8_t *d = BITS(dst) + HALF(W[cy]) + p * y + x;
    if (cy < 0xf) {
        uint32_t n = W[cy];
        for (uint32_t r = cy; r < 0xf; r++) {
            memcpy(d, s, n);
            d += p - 2;
            s += n + 2;
            n += 4;
        }
        d--;
    }
    if (cy < 0x10) {
        memcpy(d, s, 0x40);
        s += 0x40;
        d += p;
    }
    if (cy < 0x11) {
        memcpy(d, s, 0x40);
        s += 0x40;
        d += p + 3;
        cy = 0x11;
    }
    uint32_t n = W[cy];
    for (uint32_t r = cy < 0x12 ? 0x11 : cy; r < 0x20; r++) {
        memcpy(d, s, n);
        d += p + 2;
        s += n + 2;
        n -= 4;
    }
}

/* Rows 15 and 16, w pixels from the left, if any of rows cy to cy + h - 1 */
static void middle_rows(uint8_t *d, const uint8_t *s, int p, uint32_t w, uint32_t h, uint32_t cy)
{
    if (cy < 0x10 && 0xe < cy + h) {
        memcpy(d, s, w);
        s += 0x40;
        d += p;
    }
    if (cy < 0x11 && 0xf < cy + h)
        memcpy(d, s, w);
}

/* 0x1005ce78: whole height, columns 0 to w - 1. The upper rows also draw the mirrored
 * lower row, if it is within h. */
void tile_clip_right(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                     uint32_t cx, uint32_t cy)
{
    (void)cx;
    int p = PITCH(dst);
    const uint8_t *s = BITS(tile) + 4;
    if (w < 0x21) {
        uint32_t k = w - 1;
        uint32_t r0 = ROW_AT[k];
        if (ROW_START[r0] < k) {
            k = ROW_START[r0];
            r0 = ROW_AT[k];
        }
        uint8_t *d = BITS(dst) + (y + r0) * p + x + k;
        uint32_t c = k;
        uint32_t n = (k - HALF(W[r0])) + 1;
        s = BITS(tile) + t->start[k] + 4;
        const int32_t *m = &t->mirror[r0];
        for (uint32_t r = r0; r < 0xf; r++) {
            memcpy(d, s, n);
            if (0x1f - r < h)
                memcpy(d + *m, s + (O[0x1f - r] - O[r]), n);
            s += t->adv[r][c];
            n += 2;
            if (!(c & 1)) {
                d--;
                c--;
            }
            d += p - 2;
            c -= 2;
            m++;
        }
        middle_rows(d - 1, s, p, w, h, cy);
    } else {
        uint8_t *d = BITS(dst) + p * y + x + 0x1f;
        uint32_t lo = 0x1f, hi = 0x20;
        const int32_t *m = t->mirror;
        for (uint32_t r = 0; r < 0xf; r++) {
            memcpy(d, s, (hi - lo) + 1);
            if (0x1f - r < h)
                memcpy(d + *m, s + (O[0x1f - r] - O[r]), (hi - lo) + 1);
            s += t->adv[r][lo];
            d += p - 2;
            lo -= 2;
            hi += 2;
            if (w - 1 < hi)
                hi = w - 1;
            m++;
        }
        middle_rows(d - 1, s, p, w, h, cy);
    }
}

/* 0x1005d1b7: whole height, columns cx to 63 */
void tile_clip_left(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                    uint32_t cx, uint32_t cy)
{
    int p = PITCH(dst);
    const uint8_t *s = BITS(tile) + 4;
    if (w < 0x21) {
        uint32_t r0 = ROW_AT[cx];
        uint8_t *d = BITS(dst) + (y + r0) * p + x;
        s = BITS(tile) + ((O[r0] + cx) - HALF(W[r0])) + 4;
        uint32_t e = HALF(W[r0]) + W[r0] - 1;
        const int32_t *m = &t->mirror[r0];
        for (uint32_t r = r0; r < 0xf; r++) {
            memcpy(d, s, (e - cx) + 1);
            if (0x1f - r < h)
                memcpy(d + *m, s + (O[0x1f - r] - O[r]), (e - cx) + 1);
            s += t->adv_r[r][cx];
            d += p;
            e += 2;
            m++;
        }
        middle_rows(d, s, p, w, h, cy);
    } else {
        uint8_t *d = BITS(dst) + (p * y - cx) + 0x1f + x;
        uint32_t lo = 0x1f;
        int hi = 0x20;
        int clipped = 0;
        const int32_t *m = t->mirror;
        for (uint32_t r = 0; r < 0xf; r++) {
            memcpy(d, s, (hi - lo) + 1);
            if (0x1f - r < h)
                memcpy(d + *m, s + (O[0x1f - r] - O[r]), (hi - lo) + 1);
            int step = r == 0xe ? 3 : 2;
            int ds, dd;
            if (clipped) {
                ds = t->adv_r[r][lo];
                dd = p;
            } else {
                lo -= step;
                if (lo < cx) {
                    clipped = 1;
                    int cut = cx - lo;
                    ds = t->adv[r][step + lo] + cut;
                    lo = cx;
                    dd = (p - step) + cut;
                } else {
                    ds = t->adv[r][step + lo];
                    dd = p - 2;
                }
            }
            s += ds;
            d += dd;
            hi += step;
            m++;
        }
        middle_rows(d, s, p, w, h, cy);
    }
}

/* Draws the part of a row n wide that lies in columns cx and right of it */
static void row_from(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t cx)
{
    uint32_t st = HALF(n);
    if (st < cx) {
        uint32_t e = st + n - 1;
        if (cx <= e)
            memcpy(d + (cx - st), s + (cx - st), (e - cx) + 1);
    } else
        memcpy(d, s, n);
}

/* Draws the part of a row n wide that lies left of column end */
static void row_to(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t end)
{
    uint32_t st = HALF(n);
    if (st + n - 1 < end)
        memcpy(d, s, n);
    else if (st < end)
        memcpy(d, s, end - st);
}

/* The common walk of the four corner variants: visible(r) says whether row r is drawn,
 * row() draws it. off moves from the visible corner back to the tile's corner. */
#define CORNER_WALK(VISIBLE, ROW, MID15, MID16)                                                                   \
    int p = PITCH(dst);                                                                                           \
    int off = p * cy + cx;                                                                                        \
    const uint8_t *s = BITS(tile) + O[0] + 4;                                                                     \
    uint8_t *d = BITS(dst) + HALF(W[0]) + p * y + x;                                                              \
    uint32_t n = 2;                                                                                               \
    for (uint32_t r = 0; r < 0xf; r++) {                                                                          \
        if (VISIBLE)                                                                                              \
            ROW;                                                                                                  \
        d += p - 2;                                                                                               \
        s += n + 2;                                                                                               \
        n += 4;                                                                                                   \
    }                                                                                                             \
    MID15;                                                                                                        \
    d = d - 1 + p;                                                                                                \
    MID16;                                                                                                        \
    s += 0x80;                                                                                                    \
    d += p + 3;                                                                                                   \
    n = W[0x11];                                                                                                  \
    for (uint32_t r = 0x11; r < 0x20; r++) {                                                                      \
        if (VISIBLE)                                                                                              \
            ROW_LOWER;                                                                                            \
        d += p + 2;                                                                                               \
        s += n + 2;                                                                                               \
        n -= 4;                                                                                                   \
    }

/* 0x1005d550: rows cy to 31, columns cx to 63 */
void tile_clip_left_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                        uint32_t cx, uint32_t cy)
{
    (void)t;
    (void)w;
    (void)h;
#define ROW_LOWER row_from(d - off, s, n, cx)
    CORNER_WALK(cy <= r, row_from(d - off, s, n, cx),
                if (cy < 0x10) memcpy(d - 1 - off + cx, s + cx, 0x40 - cx),
                if (cy < 0x11) memcpy(d - off + cx, s + cx + 0x40, 0x40 - cx))
#undef ROW_LOWER
}

/* 0x1005d7dc: rows cy to 31, columns 0 to cx + w - 1 */
void tile_clip_right_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                         uint32_t cx, uint32_t cy)
{
    (void)t;
    (void)h;
#define ROW_LOWER row_to(d - off, s, n, cx + w)
    CORNER_WALK(cy <= r, row_to(d - off, s, n, cx + w),
                if (cy < 0x10) memcpy(d - 1 - off + cx, s + cx, w),
                if (cy < 0x11) memcpy(d - off + cx, s + cx + 0x40, w))
#undef ROW_LOWER
}

/* 0x1005da50: rows 0 to cy + h - 1, columns cx to 63 */
void tile_clip_left_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                           uint32_t cx, uint32_t cy)
{
    (void)t;
    (void)w;
#define ROW_LOWER row_from(d - off, s, n, cx)
    CORNER_WALK(r < h + cy, row_from(d - off, s, n, cx),
                if (0xf < h + cy) memcpy(d - 1 - off + cx, s + cx, 0x40 - cx),
                if (0x10 < h + cy) memcpy(d - off + cx, s + cx + 0x40, 0x40 - cx))
#undef ROW_LOWER
}

/* The lower rows of 0x1005dcec test the row's end against cx instead of cx + w, so they
 * draw cx + w - start pixels even where the row is narrower: past its end, into the
 * next rows' pixels. */
static void row_to_lower(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t cx, uint32_t w)
{
    uint32_t st = HALF(n);
    if (st + n - 1 < cx)
        memcpy(d, s, n);
    else if (st <= cx + w - 1)
        memcpy(d, s, cx + w - st);
}

/* 0x1005dcec: rows 0 to cy + h - 1, columns 0 to cx + w - 1 */
void tile_clip_right_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                            uint32_t cx, uint32_t cy)
{
    (void)t;
#define ROW_LOWER row_to_lower(d - off, s, n, cx, w)
    CORNER_WALK(r < cy + h, row_to(d - off, s, n, cx + w),
                if (0xf < cy + h) memcpy(d - 1 - off + cx, s + cx, w),
                if (0x10 < cy + h) memcpy(d - off + cx, s + cx + 0x40, w))
#undef ROW_LOWER
}

/* 0x1005c99a (unused): rows y0 to y1, columns x0 to x1, drawn at x, y */
void tile_rect(void *dst, const void *tile, int x, int y, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1)
{
    const uint8_t *s = BITS(tile);
    int p = PITCH(dst);
    uint8_t *d = BITS(dst) + p * y + x;
    uint32_t a = 0, b = 0;
    for (uint32_t r = y0; r <= y1; r++) {
        uint32_t st = HALF(W[r]);
        uint32_t e = st + W[r] - 1;
        int vis = 1;
        if (st >= x0 && e <= x1) {
            a = st;
            b = e;
        } else if (x0 >= st && x1 <= e) {
            a = x0;
            b = x1;
        } else if (e < x0 || x1 < st)
            vis = 0;
        else {
            a = x0 <= st ? st : x0;
            b = e <= x1 ? e : x1;
        }
        if (vis)
            memcpy(d + (a - x0), s + O[r] + (a - st) + 4, (b - a) + 1);
        d += p;
    }
}

/* 0x1005e4be: fills a tile's diamond with color 10 (shroud) */
void tile_fill(void *dst, int x, int y)
{
    int p = PITCH(dst);
    uint8_t *d = BITS(dst) + p * y + x + 0x1f;
    uint32_t n = 2;
    for (int r = 0; r < 0xf; r++) {
        memset(d, 10, n);
        d += p - 2;
        n += 4;
    }
    memset(d - 1, 10, 0x40);
    d = d - 1 + p;
    memset(d, 10, 0x40);
    n = 0x3a;
    d += p + 3;
    for (int r = 0x11; r < 0x20; r++) {
        memset(d, 10, n);
        d += p + 2;
        n -= 4;
    }
}

/* 0x1005df6e (unused): a w x h frame in color 9 */
void tile_frame(void *dst, int x, uint32_t y, uint32_t w, int h)
{
    int p = PITCH(dst);
    uint8_t *d = BITS(dst) + p * y + x;
    memset(d, 9, w);
    uint8_t *c = d + p + w - 1;
    for (uint32_t i = y + 1; i < h + y; i++) {
        *c = 9;
        c += p;
    }
    c += -(int)(w - 1) - p;
    memset(c, 9, w);
    c += -p;
    for (int i = h + y - 2; (int)y <= i; i--) {
        *c = 9;
        c -= p;
    }
}
