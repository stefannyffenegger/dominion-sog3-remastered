/* Game logic: finding a free cell on a ring around a point (ringfind.c) */
#ifndef DOMINION_ISO_RINGFIND_H
#define DOMINION_ISO_RINGFIND_H

#include <stdint.h>

int32_t __cdecl ring_find_cell(int32_t k, int32_t ux, int32_t uy, int32_t *x, int32_t *y, int32_t n, int32_t mode);

#endif
