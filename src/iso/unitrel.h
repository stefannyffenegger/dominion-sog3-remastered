/* Game logic: two small unit tests (unitrel.c) */
#ifndef DOMINION_ISO_UNITREL_H
#define DOMINION_ISO_UNITREL_H

#include <stdint.h>

int32_t __cdecl players_allied(uint32_t a, uint32_t b);
int32_t __cdecl ignores_marks(const uint8_t *u);

#endif
