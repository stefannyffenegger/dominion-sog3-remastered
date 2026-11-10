/* The sprite engine's assembly routines in portable C (blit.c) */
#ifndef DOMINION_RTLIB_BLIT_H
#define DOMINION_RTLIB_BLIT_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
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

#endif

int __stdcall rect_intersect(int32_t *out, const int32_t *a, const int32_t *b);
int __stdcall rect_union(int32_t *out, const int32_t *a, const int32_t *b);
void __stdcall palette_fade(uint8_t *dst, const uint8_t *src, uint32_t step, uint32_t steps, uint32_t n);
void __stdcall dib_fill8(void *bm, int x, int y, int w, int h, uint32_t color);
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

#endif
