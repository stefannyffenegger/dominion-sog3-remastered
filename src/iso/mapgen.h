/* Game logic: the modifier keys and the random map generator (mapgen.c) */
#ifndef DOMINION_ISO_MAPGEN_H
#define DOMINION_ISO_MAPGEN_H

#include <stdint.h>

uint8_t __stdcall IsoGetKeyModifiers(uint8_t *iso);
void __stdcall mapgen_clear_groups(int w, int h);
int __stdcall mapgen_generate(int w, int h, int seed, int terrain, int density, int ring, int unused);
int __stdcall mapgen_ring(int w, int h, int bw, int bh, int count, void *grid);
void __stdcall mapgen_mark(int bw, int bh, int x, int y, int group, void *grid);
int __stdcall mapgen_fill(int w, int h, int bw, int bh, int terrain, int density, void *grid);
int __stdcall mapgen_fill_block(int terrain, int kind, uint32_t group, int x0, int y0, int w, int h);
int __cdecl mapgen_cell(int x, int y, int *col, int *row, int w, int h);
int __stdcall mapgen_add_clusters(int w, int h, int terrain, int density);
int __stdcall mapgen_clusters(int terrain, int g1, int g2, int n_min, int n_max, int w_min, int w_max, int h_min,
                              int h_max, int d_min, int d_max);
void __cdecl mapgen_cell_info(int x, int y, uint32_t *taken, uint32_t *group);
void __cdecl IsoGenerateMap(uint8_t *iso, int w, int h, int seed, int terrain, int density, int ring, int unused);
void __cdecl IsoAPIGetRandomMapInfo(uint8_t *iso);

#endif
