/* Game logic: unit commands (commands.c) */
#ifndef DOMINION_ISO_COMMANDS_H
#define DOMINION_ISO_COMMANDS_H

#include <stdint.h>

int __stdcall unit_command(uint32_t id, uint32_t cmd, uint32_t value);
int __stdcall IsoSetSelectedUnitState(uint8_t *iso, uint32_t state);
int __cdecl unit_fire_button(uint32_t id);

#endif
