/* Game logic: unit counts, defeat, objects in the sector table (playerstate.c) */
#ifndef DOMINION_ISO_PLAYERSTATE_H
#define DOMINION_ISO_PLAYERSTATE_H

#include <stdint.h>

void __cdecl player_unit_built(uint32_t p, uint8_t kind);
void __cdecl player_unit_lost(uint32_t p, uint8_t kind);
void __cdecl player_check_lost(uint8_t p);
void __cdecl object_link(uint8_t *o, uint32_t sector);
void __cdecl object_sector(uint32_t k, int16_t how);

#endif
