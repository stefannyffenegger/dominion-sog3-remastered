/* Game logic: sector paths (paths.c) */
#ifndef DOMINION_ISO_PATHS_H
#define DOMINION_ISO_PATHS_H

#include <stdint.h>

void __cdecl path_free(int n);
int __cdecl sector_run(int s, uint32_t dir);
int __cdecl sector_path_length(int from, int to);
int __cdecl point_path_length(int x, uint32_t y, int to, uint32_t flags);
int __cdecl sector_path_exists(int from, int to);
int __cdecl point_path_exists(int x, uint32_t y, int to, uint32_t flags);
uint32_t __cdecl sector_cell_flag(int s, uint8_t bit);

#endif
