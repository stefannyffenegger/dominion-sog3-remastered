/* Game logic: a computer unit's patrol fields, a stuck soldier's way (soldierway.c) */
#ifndef DOMINION_ISO_SOLDIERWAY_H
#define DOMINION_ISO_SOLDIERWAY_H

#include <stdint.h>

void __cdecl ai_patrol_clear(uint8_t *u);
int32_t __cdecl patrols_collide(uint8_t *u);

#endif
