/* Game logic: small exports for the scripts (exports.c) */
#ifndef DOMINION_ISO_EXPORTS_H
#define DOMINION_ISO_EXPORTS_H

#include <stdint.h>

int32_t __stdcall IsoGetUnitString(uint8_t *iso, int32_t type, int32_t player, uint32_t var);
int32_t __stdcall IsoCalcSliderPosition(uint8_t *iso, uint32_t pos, uint32_t count, uint32_t size);
int32_t __stdcall IsoCalcSliderStartItem(uint8_t *iso, uint32_t size, int32_t a, int32_t b, uint32_t count);
int32_t __stdcall IsoGetDibDimensions(uint8_t *iso, uint32_t res, int32_t *w, int32_t *h);
void __stdcall IsoSetRace(uint8_t *iso, int32_t player, int32_t race, int32_t colour);
uint8_t __stdcall IsoGetRace(uint8_t *iso, int32_t player);
int32_t __stdcall IsoCampaignInfo(uint8_t *iso, int32_t set, int32_t i, int32_t value);

#endif
