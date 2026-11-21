/* Game logic: small engine exports (engineexp.c) */
#ifndef DOMINION_ISO_ENGINEEXP_H
#define DOMINION_ISO_ENGINEEXP_H

#include <stdint.h>
#include <windows.h>

void __stdcall IsoInitReconnectAndPauseWindowC(uint8_t *iso, uint32_t a, uint32_t b, uint32_t c);
int32_t __stdcall IsoFindUnitC(uint8_t *iso, LONG x, LONG y);
uint32_t __stdcall IsoSetPlayerIdC(uint8_t *iso, uint32_t player);
int32_t __stdcall IsoSetDebugModeC(uint8_t *iso, int32_t mode);
void __stdcall IsoEnableAmbientsC(uint8_t *iso, uint32_t unused, uint32_t on);
uint32_t __stdcall IsoAddUnitC(uint8_t *iso, uint32_t player, uint32_t kind, uint32_t a4, int32_t a5, uint32_t a6,
                               uint32_t a7, int32_t a8, uint32_t a9, uint32_t flags);

#endif
