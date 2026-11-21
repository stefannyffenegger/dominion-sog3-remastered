/* Game logic: whether a vehicle's patrol still ends somewhere it can go (patrolend.c) */
#ifndef DOMINION_ISO_PATROLEND_H
#define DOMINION_ISO_PATROLEND_H

#include <stdint.h>

int32_t __cdecl vehicle_patrol_check(uint8_t *u);
int32_t __cdecl patrol_end_free(uint8_t *u, int32_t x, int32_t y, uint32_t dir);

#endif
