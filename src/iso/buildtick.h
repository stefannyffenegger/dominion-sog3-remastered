/* Game logic: the build slots' progress (buildtick.c) */
#ifndef DOMINION_ISO_BUILDTICK_H
#define DOMINION_ISO_BUILDTICK_H

#include <stdint.h>

int32_t __cdecl build_slots_tick(void);
void __stdcall IsoSetTabsetC(uint8_t *iso, uint8_t tab);
int32_t __stdcall IsoInitIntfOptionsC(uint8_t *iso, uint32_t table, uint32_t states, uint32_t script);
void __stdcall IsoRecalcBuildInfoC(uint8_t *iso, uint32_t player);
int32_t __stdcall IsoInitEconomyTableC(uint8_t *iso, uint32_t table);
int32_t __stdcall IsoInitTechDependTablesC(uint8_t *iso, uint32_t sides, uint32_t needs, uint32_t third);
int32_t __cdecl IsoUnitIsBuiltC(uint8_t *iso, uint32_t kind, int32_t player);

#endif
