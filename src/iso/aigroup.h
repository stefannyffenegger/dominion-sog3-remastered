/* Game logic: a computer player's current squad (aigroup.c) */
#ifndef DOMINION_ISO_AIGROUP_H
#define DOMINION_ISO_AIGROUP_H

#include <stdint.h>

int32_t __cdecl ai_group_take(int32_t k, int32_t s);
int32_t __cdecl ai_adding(int32_t k, int32_t v);
int32_t __cdecl ai_player_hold(int32_t k, int32_t v);
int32_t __cdecl ai_group_done(int32_t k);
void __cdecl ai_unit_noted(uint32_t unit, int32_t v);

#endif
