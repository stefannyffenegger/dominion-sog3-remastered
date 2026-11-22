/* Game logic: a structure's footprint on the map cells (footprint.c) */
#ifndef DOMINION_ISO_FOOTPRINT_H
#define DOMINION_ISO_FOOTPRINT_H

#include <stdint.h>

int32_t __stdcall IsoUpdateFootprintC(uint8_t *iso, uint32_t id, int32_t place, int32_t burn);

#endif
