/* Game logic: races, tab sets, graphics quality, copy protection (prefs.c) */
#ifndef DOMINION_ISO_PREFS_H
#define DOMINION_ISO_PREFS_H

#include <stdint.h>

void __cdecl IsoResetPlayerRace(uint8_t *iso, uint32_t player, int32_t offset);
void __cdecl IsoSetTextEntryFlag(uint8_t *iso, int32_t on);
int32_t __cdecl IsoGetTabsetParms(uint8_t *iso, int32_t set, int32_t *out);
void __cdecl IsoSetTabsetParms(uint8_t *iso, int32_t set, int32_t value, int32_t bank, int32_t *params);
void __cdecl IsoSetGraphicsQuality(uint8_t *iso, uint32_t q);
int32_t __cdecl IsoCopyProtection(uint8_t *iso, int32_t check);
long double __cdecl fabs_x87(float v);
long double __cdecl fabs_x87_wrap(float v);

#endif
