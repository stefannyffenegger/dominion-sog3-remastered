/* Game logic: distances to a point or a unit (unitrange.c) */
#ifndef DOMINION_ISO_UNITRANGE_H
#define DOMINION_ISO_UNITRANGE_H

#include <stdint.h>

int32_t __cdecl unit_near(uint32_t id, int32_t range, int32_t x, int32_t y, uint32_t times);
int32_t __cdecl unit_in_range(uint32_t id, uint32_t tid);

#endif
