/* Game logic: the helpers of the ring builder (ringcell.c) */
#ifndef DOMINION_ISO_RINGCELL_H
#define DOMINION_ISO_RINGCELL_H

#include <stdint.h>

int32_t __cdecl ring_add(int32_t *x, int32_t *y, int32_t count);
int32_t __cdecl ring_clear(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t r);
int32_t __cdecl ring_fix(int32_t *x, int32_t *y, int32_t cx, int32_t cy, int32_t r);

#endif
