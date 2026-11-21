/* Game logic: IsoAddUnitType (unittype.c) */
#ifndef DOMINION_ISO_UNITTYPE_H
#define DOMINION_ISO_UNITTYPE_H

#include <stdint.h>

uint32_t __stdcall IsoAddUnitTypeC(uint8_t *iso, int32_t fresh, uint32_t id, uint32_t player, uint32_t kind,
                                   uint32_t a6, int32_t x, int32_t y, uint32_t look, uint32_t a10, uint32_t level,
                                   uint32_t a12, uint32_t flags);

#endif
