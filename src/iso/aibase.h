/* Game logic: AI squads, the base's building list (aibase.c) */
#ifndef DOMINION_ISO_AIBASE_H
#define DOMINION_ISO_AIBASE_H

#include <stdint.h>

int32_t __cdecl ai_squad_of_kind(int32_t player, int32_t kind);
int32_t __cdecl ai_squad_new(int32_t player, uint32_t kind, int base);
void __cdecl ai_rebuild_lost(int32_t player, int32_t task);
void __cdecl ai_build_list_step(int32_t player, int32_t task);
int32_t __cdecl ai_squad_review(int32_t squad);

#endif
