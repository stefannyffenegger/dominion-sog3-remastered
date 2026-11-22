/* Game logic: the tables of rings of cells around a cell (ringinit.c) */
#ifndef DOMINION_ISO_RINGINIT_H
#define DOMINION_ISO_RINGINIT_H

#include <stdint.h>

int32_t __cdecl tile_corner(int32_t *x, int32_t *y, int32_t *row);
void __cdecl rings_init(void);

#endif
