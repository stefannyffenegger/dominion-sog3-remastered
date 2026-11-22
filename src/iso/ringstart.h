/* Game logic: a unit's mark on the map, where to start on a ring (ringstart.c) */
#ifndef DOMINION_ISO_RINGSTART_H
#define DOMINION_ISO_RINGSTART_H

#include <stdint.h>

void __cdecl unit_free_links(int32_t k);
int32_t __cdecl ring_start(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t n);

#endif
