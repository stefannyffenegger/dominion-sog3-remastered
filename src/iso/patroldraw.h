/* Game logic: the debug view of the patrol legs (patroldraw.c) */
#ifndef DOMINION_ISO_PATROLDRAW_H
#define DOMINION_ISO_PATROLDRAW_H

#include <stdint.h>

void __cdecl patrol_draw(uint8_t *u, uint32_t on);
void __cdecl patrols_draw(void);

#endif
