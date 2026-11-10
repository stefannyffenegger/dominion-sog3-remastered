/* The isometric tile blitters in portable C (tileblit.c) */
#ifndef DOMINION_RTLIB_TILEBLIT_H
#define DOMINION_RTLIB_TILEBLIT_H

#include <stdint.h>

/* Lookup tables built for one screen pitch (0x1005c569) */
typedef struct TileTables {
    int32_t adv[32][64];   /* 0x1008a6d0: source step after row r, by its first drawn column */
    int32_t adv_r[32][64]; /* 0x1008d838: the same for right-hand parts */
    int32_t start[64];     /* 0x1008d738: source offset of the row that starts at column x */
    int32_t mirror[15];    /* 0x1008d6f8: distance from row r to row 31 - r in the screen */
} TileTables;

void tile_tables_build(TileTables *t, int pitch);
void tile_tables_start_build(TileTables *t); /* parts of it */
void tile_tables_adv_build(TileTables *t);

/* dst and tile are bitmaps (header + pixel pointer at +0x20). A tile's pixels are a
 * 4-byte header and then its 32 rows, 2, 6, ..., 58, 64, 64, 58, ..., 2 pixels wide.
 * (x, y) is where the visible part starts on the screen; cx, cy is how much of the tile is
 * clipped off on the left and top, and w, h is the visible size. */
void tile_full(const TileTables *t, void *dst, const void *tile, int x, int y);
void tile_clip_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y);
void tile_clip_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                   uint32_t cx, uint32_t cy);
void tile_clip_right(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                     uint32_t cx, uint32_t cy);
void tile_clip_left(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                    uint32_t cx, uint32_t cy);
void tile_clip_left_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                        uint32_t cx, uint32_t cy);
void tile_clip_right_top(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                         uint32_t cx, uint32_t cy);
void tile_clip_left_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                           uint32_t cx, uint32_t cy);
void tile_clip_right_bottom(const TileTables *t, void *dst, const void *tile, uint32_t w, uint32_t h, int x, int y,
                            uint32_t cx, uint32_t cy);
void tile_rect(void *dst, const void *tile, int x, int y, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1);
void tile_fill(void *dst, int x, int y);
void tile_frame(void *dst, int x, uint32_t y, uint32_t w, int h);

#endif
