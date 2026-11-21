/* Game logic: a unit setting off along its path (patrolstep.c) */
#ifndef DOMINION_ISO_PATROLSTEP_H
#define DOMINION_ISO_PATROLSTEP_H

#include <stdint.h>

void __cdecl patrol_reset(uint8_t *u);
void __cdecl unit_moved(uint32_t unit);

#endif
