/* Game logic: showing the map objects (objshow.c) */
#ifndef DOMINION_ISO_OBJSHOW_H
#define DOMINION_ISO_OBJSHOW_H

#include <stdint.h>

void __stdcall IsoInitObjectDestructionInfoC(uint8_t *iso, uint32_t res);
int32_t __stdcall IsoShowObjectsC(uint8_t *iso, int32_t show);

#endif
