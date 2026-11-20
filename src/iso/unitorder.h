/* Game logic: a unit's order and state (unitorder.c) */
#ifndef DOMINION_ISO_UNITORDER_H
#define DOMINION_ISO_UNITORDER_H

#include <stdint.h>

void __cdecl unit_set_order_lock(uint32_t unit, uint8_t lock);
void __cdecl unit_order(uint32_t unit, uint32_t order, int32_t send, int32_t delay);
void __cdecl unit_set_state(uint32_t unit, uint8_t state);
void __cdecl unit_set_action(uint32_t unit, uint32_t action, int32_t send, int32_t delay);

#endif
