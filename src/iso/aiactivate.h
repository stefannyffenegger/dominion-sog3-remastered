/* Game logic: turning a player over to the computer (aiactivate.c) */
#ifndef DOMINION_ISO_AIACTIVATE_H
#define DOMINION_ISO_AIACTIVATE_H

#include <stdint.h>

int32_t __cdecl ai_player_reset(int32_t k, uint32_t race);
int32_t __cdecl ai_player_race(int32_t k, uint32_t race);
int32_t __cdecl ai_player_computer(int32_t k, uint32_t race, int32_t level, int32_t value);
int32_t __cdecl ai_player_active(int32_t k);

#endif
