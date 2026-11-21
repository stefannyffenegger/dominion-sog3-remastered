/* Game logic: a defence squad taking up its guard (squaddefend.c) */
#ifndef DOMINION_ISO_SQUADDEFEND_H
#define DOMINION_ISO_SQUADDEFEND_H

#include <stdint.h>

int32_t __cdecl squad_setup_defense(int32_t s);
void __cdecl ai_player_review(int32_t player);
void __cdecl ai_player_plan(int32_t player);

#endif
