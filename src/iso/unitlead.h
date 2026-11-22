/* Game logic: how far a unit gets during the network delay (unitlead.c) */
#ifndef DOMINION_ISO_UNITLEAD_H
#define DOMINION_ISO_UNITLEAD_H

#include <stdint.h>

int32_t __cdecl unit_step_reach(uint32_t id, uint32_t dir, int32_t *x, int32_t *y);

#endif
