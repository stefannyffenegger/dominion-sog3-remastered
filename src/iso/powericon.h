/* Game logic: a unit's power tick and its "no power" icon (powericon.c) */
#ifndef DOMINION_ISO_POWERICON_H
#define DOMINION_ISO_POWERICON_H

#include <stdint.h>

void __cdecl power_tick_of(uint32_t id);
int32_t __cdecl unit_power_icon(uint32_t id);

#endif
