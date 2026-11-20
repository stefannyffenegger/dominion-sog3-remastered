/* Game logic: the path table (pathtable.c) */
#ifndef DOMINION_ISO_PATHTABLE_H
#define DOMINION_ISO_PATHTABLE_H

#include <stdint.h>

void __cdecl path_table_fill(uint8_t *cells);
int32_t __cdecl path_table_init(void);
int32_t __cdecl point_on_map(int32_t x, int32_t y);
uint32_t __cdecl path_cell_at(int32_t x, uint32_t y);
void __cdecl path_slots_walk(void *path);
void __cdecl path_sectors_draw(void);

#endif
