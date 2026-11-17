/* Game logic: the AI's targets and squads (aisquads.c) */
#ifndef DOMINION_ISO_AISQUADS_H
#define DOMINION_ISO_AISQUADS_H

#include <stdint.h>

int32_t __cdecl unit_attacked_by(uint8_t *u, uint32_t player);
int32_t __cdecl squad_add_target(int32_t squad, int32_t target);
void __cdecl squad_drop_target(int32_t player, int32_t target);
void __cdecl target_tell_players(int32_t target);
int32_t __cdecl squad_can_attack(int32_t player, uint8_t *u, int32_t squad);
int32_t __cdecl squad_find_for(uint8_t *u, int32_t player, int32_t list);
int32_t __cdecl target_assign(int32_t player, int32_t target, int32_t force);
void __cdecl ai_assign_targets(int32_t player);
int32_t __cdecl unit_tile_value(uint16_t id);
int32_t __cdecl ai_set_lethality(int32_t player, int32_t value);

#endif
