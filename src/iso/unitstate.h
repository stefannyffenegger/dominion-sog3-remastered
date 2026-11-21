/* Game logic: one step of a unit's action state machine (unitstate.c) */
#ifndef DOMINION_ISO_UNITSTATE_H
#define DOMINION_ISO_UNITSTATE_H

#include <stdint.h>

void __cdecl unit_ai_state(uint32_t id);
void unit_guard_step(uint32_t id, uint8_t *u);

#endif
