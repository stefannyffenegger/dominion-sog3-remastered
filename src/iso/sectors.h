/* Game logic: the flip timer and the sector tables (sectors.c) */
#ifndef DOMINION_ISO_SECTORS_H
#define DOMINION_ISO_SECTORS_H

#include <stdint.h>

int32_t __stdcall IsoInitFlipProcessC(uint8_t *iso);
int32_t __cdecl sectors_init(void);

#endif
