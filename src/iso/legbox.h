/* Game logic: finishing a patrol plan (legbox.c) */
#ifndef DOMINION_ISO_LEGBOX_H
#define DOMINION_ISO_LEGBOX_H

#include <stdint.h>

long double __cdecl leg_step(uint32_t dir);
void __cdecl legs_start(uint8_t *u);

#endif
