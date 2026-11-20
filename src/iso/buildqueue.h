/* Game logic: the build slots, the counters' setup (buildqueue.c) */
#ifndef DOMINION_ISO_BUILDQUEUE_H
#define DOMINION_ISO_BUILDQUEUE_H

#include <stdint.h>

void __cdecl build_slot_cancel(uint8_t player, int32_t kind);
void __stdcall IsoUpdateMMStringC(uint8_t *iso);
void __stdcall IsoInitMenMatStringsC(uint8_t *iso, uint32_t men_var, uint32_t mat_var, uint32_t men_sprite,
                                     uint32_t men_pos, uint32_t mat_sprite, uint32_t mat_pos, uint32_t font,
                                     uint32_t c34a, uint32_t c346, uint32_t c342);

#endif
