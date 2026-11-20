/* Game logic: energy wells appearing, finding a well (wells.c) */
#ifndef DOMINION_ISO_WELLS_H
#define DOMINION_ISO_WELLS_H

#include <stdint.h>

void __cdecl wells_appear_tick(void);
int32_t __cdecl IsoFindActiveEnergyWellC(uint8_t *iso, int32_t x, int32_t y, uint32_t *state);

#endif
