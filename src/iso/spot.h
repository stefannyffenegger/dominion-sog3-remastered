/* Game logic: finding a free map cell for a unit near a point (spot.c) */
#ifndef DOMINION_ISO_SPOT_H
#define DOMINION_ISO_SPOT_H

#include <stdint.h>

int32_t __cdecl formation_spot(uint32_t unit, uint32_t how, int32_t a3, uint32_t dir, int32_t *x, int32_t *y,
                               int32_t *cell, int32_t rings, int32_t moved);

#endif
