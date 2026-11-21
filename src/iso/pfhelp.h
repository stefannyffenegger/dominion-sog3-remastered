/* Game logic: the path finder's helpers (pfhelp.c) */
#ifndef DOMINION_ISO_PFHELP_H
#define DOMINION_ISO_PFHELP_H

#include <stdint.h>

void __cdecl pf_step(void);
int32_t __cdecl pf_cost(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
int32_t __cdecl pf_rotate(int32_t col, int32_t row);
int32_t __cdecl pf_blocked(int32_t cell, int32_t col, int32_t row, uint32_t unit);

#endif
