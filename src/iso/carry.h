/* Game logic: units carried by another (carry.c) */
#ifndef DOMINION_ISO_CARRY_H
#define DOMINION_ISO_CARRY_H

#include <stdint.h>

void __cdecl carried_follow(uint32_t unit);
int32_t __cdecl dest_claim(uint32_t unit, uint32_t x, uint32_t y, int32_t check);
int32_t __cdecl dest_release(uint32_t unit, int32_t push_off);

#endif
