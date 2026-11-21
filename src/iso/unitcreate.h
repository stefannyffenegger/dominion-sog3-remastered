/* Game logic: creating a unit (unitcreate.c) */
#ifndef DOMINION_ISO_UNITCREATE_H
#define DOMINION_ISO_UNITCREATE_H

#include <stdint.h>

uint32_t __cdecl unit_create(uint32_t id, uint32_t player, uint32_t kind, uint32_t a4, int32_t x, uint32_t y,
                             uint32_t a7, int32_t a8, uint32_t level, int32_t a10, uint32_t flags);

#endif
