/* Game logic: enemy wall posts on a patrol way (patrolblock.c) */
#ifndef DOMINION_ISO_PATROLBLOCK_H
#define DOMINION_ISO_PATROLBLOCK_H

#include <stdint.h>

int32_t __cdecl patrol_blocked(uint8_t *u);
void __cdecl unit_set_hold(uint8_t *u, uint32_t on);
int32_t __cdecl cell_of(int32_t x, int32_t y);

#endif
