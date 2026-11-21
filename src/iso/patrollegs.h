/* Game logic: the patrol legs' range, direction and unit vectors (patrollegs.c) */
#ifndef DOMINION_ISO_PATROLLEGS_H
#define DOMINION_ISO_PATROLLEGS_H

#include <stdint.h>

long double __cdecl patrol_range(uint8_t *u);
void __cdecl leg_vector(uint32_t dir, float *out);
uint8_t __cdecl leg_direction(const float *a, const float *b);

#endif
