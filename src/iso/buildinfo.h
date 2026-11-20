/* Game logic: the counters and the build levels (buildinfo.c) */
#ifndef DOMINION_ISO_BUILDINFO_H
#define DOMINION_ISO_BUILDINFO_H

#include <stdint.h>

int32_t __cdecl counters_update(void);
void __cdecl build_level_update(uint32_t kind, uint8_t player, uint8_t level, uint8_t how);
int32_t __cdecl interface_message4(void);
int32_t __stdcall IsoUpdateIntfOptionsC(uint8_t *iso, uint32_t player);
int32_t __cdecl building_produce_men(uint32_t unit);

#endif
