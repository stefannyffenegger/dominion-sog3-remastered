/* The tile map: tables, clipping and drawing of the map's tiles (tiles.c) */
#ifndef DOMINION_RTLIB_TILES_H
#define DOMINION_RTLIB_TILES_H

#include <stdint.h>
#include <windows.h>

void __cdecl tile_outline_build(void);
void __cdecl tile_tables_start(void);
void __cdecl tile_tables_adv(void);
void __cdecl tile_tables_mirror(int pitch);
void __cdecl tile_tables_make(int pitch);

void __cdecl tile_full_c(uint32_t dst, uint32_t tile, int x, int y);
void __cdecl tile_rect_c(uint32_t dst, uint32_t tile, int x, int y, uint32_t x0, uint32_t x1, uint32_t y0,
                         uint32_t y1);
void __cdecl tile_clip_bottom_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y);
void __cdecl tile_clip_top_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                             uint32_t cy);
void __cdecl tile_clip_right_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                               uint32_t cy);
void __cdecl tile_clip_left_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                              uint32_t cy);
void __cdecl tile_clip_left_top_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                                  uint32_t cy);
void __cdecl tile_clip_right_top_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                                   uint32_t cy);
void __cdecl tile_clip_left_bottom_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                                     uint32_t cy);
void __cdecl tile_clip_right_bottom_c(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y,
                                      uint32_t cx, uint32_t cy);
void __cdecl tile_frame_c(uint32_t dst, int x, uint32_t y, uint32_t w, int h);
void __cdecl tile_fill_c(uint32_t dst, int x, int y);

void __cdecl tile_draw_clipped(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                               uint32_t cy);
void __cdecl tile_draw(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx, uint32_t cy,
                       uint32_t unused, uint32_t clip);
void __cdecl SpriteTileMerge(uint32_t dst, uint32_t tile, uint32_t w, uint32_t h, int x, int y, uint32_t cx,
                             uint32_t cy);

uint8_t *__stdcall tiles_create(uint8_t *map);
void __stdcall tiles_draw(const RECT *r, uint32_t dst, uint32_t clip);
void __stdcall tiles_draw_iso(uint32_t dst, uint32_t mode, uint32_t clip);
void __stdcall tiles_row_layers(uint8_t *map, int idx, int col, int y, int cx, int cy, int w, int h, int odd,
                                uint32_t mode, uint32_t clip);
void __cdecl tile_shroud(uint32_t dst, uint32_t bm, int w, uint32_t h, int x, int y, uint32_t cx, uint32_t cy);
void __stdcall tiles_row(uint8_t *map, int idx, int col, int y, int cx, int cy, int w, int h, int odd);
void __stdcall tiles_draw_grid(uint32_t dst);
void __stdcall tiles_overlay(const RECT *r, uint32_t dst);

#endif
