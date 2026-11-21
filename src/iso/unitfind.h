/* Game logic: finding a unit's way to a point (unitfind.c) */
#ifndef DOMINION_ISO_UNITFIND_H
#define DOMINION_ISO_UNITFIND_H

#include <stdint.h>

int32_t __cdecl unit_find_path(uint32_t unit, int32_t tx, int32_t ty, int32_t patrol, int32_t reuse, int32_t a6,
                               uint32_t flag);

#endif
