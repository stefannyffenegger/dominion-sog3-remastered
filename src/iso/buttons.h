/* Game logic: the action buttons (buttons.c) */
#ifndef DOMINION_ISO_BUTTONS_H
#define DOMINION_ISO_BUTTONS_H

#include <stdint.h>

uint32_t __cdecl iso_check_state(uint32_t action, int apply);
int __cdecl unit_show_range(uint16_t id);
void __stdcall IsoProcessAction(uint8_t *iso, int action);

#endif
