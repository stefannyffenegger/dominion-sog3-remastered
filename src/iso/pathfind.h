/* Game logic: the path finder (pathfind.c) */
#ifndef DOMINION_ISO_PATHFIND_H
#define DOMINION_ISO_PATHFIND_H

#include <stdint.h>

int32_t __cdecl path_find(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int16_t *head, int32_t a6, uint32_t unit,
                          int32_t a8);

#endif
