/* Game logic: the blitter's colour lookup tables (clut.c) */
#ifndef DOMINION_ISO_CLUT_H
#define DOMINION_ISO_CLUT_H

#include <stdint.h>

int32_t __cdecl IsoLoadClutC(uint8_t *iso, uint32_t n);
int32_t __cdecl IsoLoadBlutC(uint8_t *iso);

#endif
