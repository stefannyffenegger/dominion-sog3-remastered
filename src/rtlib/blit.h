/* The sprite engine's assembly routines in portable C (blit.c) */
#ifndef DOMINION_RTLIB_BLIT_H
#define DOMINION_RTLIB_BLIT_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif /* __stdcall */

int __stdcall rect_intersect(int32_t *out, const int32_t *a, const int32_t *b);
int __stdcall rect_union(int32_t *out, const int32_t *a, const int32_t *b);
void __stdcall palette_fade(uint8_t *dst, const uint8_t *src, uint32_t step, uint32_t steps, uint32_t n);
void __stdcall dib_fill8(void *bm, int x, int y, int w, int h, uint32_t color);
void __stdcall dib_restore(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip);
void __stdcall dib_mask(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip);
void __stdcall dib_copy(void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy, uint32_t flip);

/* engine state the merge reads */
typedef struct MergeEnv {
    const uint32_t *shadow; /* 256-entry shadow table */
    int dither_slow;        /* quality on and screen wider than 640 */
    int dither_fast;        /* quality on and the interface panel on */
} MergeEnv;
void dib_merge_rle(const MergeEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, uint32_t sx,
                   uint32_t sy, uint32_t flip, const uint32_t *cmap);

typedef struct PlainEnv {
    const uint32_t *shadow; /* 256-entry shadow table */
    const uint8_t *hilite;  /* 256-byte highlight table */
    uint8_t spot;           /* spot color (SpriteSetSpotColor) */
} PlainEnv;
void dib_merge_plain(const PlainEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx, int sy,
                     uint32_t flip, const uint32_t *cmap);

/* engine state the effect merge reads */
typedef struct FxEnv {
    MergeEnv m;            /* shadow table and its dither setting */
    const uint8_t *blend;  /* effect 1: 256 x 256 blend table, [sprite][screen] (HSL+0xb70) */
    const uint32_t *table; /* effects 2 and 3: 256 entries per row (HSL+0xb6c) */
    int dither;            /* effect 1 as a screen door instead (HSL+0xb78) */
    uint32_t shadow_row;   /* the original passes uninitialized values for these two */
    uint32_t fast_row;     /* (screen-door row of unclipped sprites) */
} FxEnv;
void dib_merge_fx_rle(const FxEnv *e, void *dst, void *src, uint32_t w, uint32_t h, int dx, int dy, uint32_t sx,
                      uint32_t sy, uint32_t flip, uint32_t fx, int32_t arg, const uint32_t *cmap);
void dib_fx_run(const FxEnv *e, uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, uint32_t row,
                const uint32_t *cmap);
void dib_fx_run_mirrored(const FxEnv *e, uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg,
                         const uint32_t *cmap);
void line_point(uint32_t step, uint32_t steps, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *out);
uint32_t curve_point(uint32_t step, uint32_t steps, int32_t x0, int32_t y0, int32_t cx, int32_t cy, int32_t x1,
                     int32_t y1, int32_t *out);
uint32_t dib_pixel_plain(const void *bm, int x, int y, uint32_t flip);
uint32_t dib_pixel(const void *bm, int x, int y, uint32_t flip);
uint32_t __stdcall dib_overlap(void *a, void *b, uint32_t w, uint32_t h, int ax, int ay, int bx, int by, uint32_t flipa,
                               uint32_t flipb);
void dib_shadow(uint8_t *p, uint32_t n, const uint32_t *table); /* 0x10064e23 */
void dib_shadow_dither(uint8_t *p, uint32_t n, uint32_t row);    /* 0x10065000 */
void fx_blend(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend);
void fx_blend_cmap(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend, const uint32_t *cmap);
void fx_dither(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t row);

void dib_setup(void *src, void *dst, int sx, int sy, int dx, int dy, int w, int h, uint32_t flip, uint8_t **ps,
               uint8_t **pd, int32_t *rows, int32_t *sgap, int32_t *dgap, int bpp);
void __stdcall dib_fill8_wide(void *bm, int w, int h, int x, int y, uint32_t color);
void __stdcall dib_fill16(void *bm, int w, int h, int x, int y, uint32_t color);
/* pal: NULL for an 8-bit destination, else the palette for a 16-bit one */
void dib_copy_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip, const uint32_t *pal,
                int is565);
void dib_copy_key_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                    const uint32_t *pal, int is565);
void dib_copy_both_c(void *dst, void *src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                     const uint32_t *pal, int is565);

void __stdcall dib_copy_out(uint8_t *dst, int32_t pitch, void *src, uint32_t w, uint32_t h, int dx, int dy, int sx,
                            int sy);

/* 0x1006a588: expands a run-length coded bitmap (header, then the pixels) */
void __stdcall res_expand_rle(uint8_t *dst, const void *src);

#endif /* DOMINION_RTLIB_BLIT_H */
