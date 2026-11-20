/* Game logic: player count lookups, unit health, IsoBlastText (playerinfo.c) */
#ifndef DOMINION_ISO_PLAYERINFO_H
#define DOMINION_ISO_PLAYERINFO_H

#include <stdint.h>

int32_t __cdecl player_count_1b9b(int32_t p);
int32_t __cdecl player_count_1b97(int32_t p);
uint32_t __cdecl player_unseen(int32_t p);
uint32_t __cdecl game_difficulty(void);
void __cdecl unit_set_flag_2a(uint8_t *u, uint8_t v);
int32_t __cdecl IsoBlastTextC(uint8_t *iso, uint32_t font, uint32_t dib, int32_t colours, uint32_t text, int x, int y);
uint32_t __cdecl unit_max_health(uint8_t *u, uint8_t level);
int32_t __cdecl unit_health_share(uint8_t *u, uint8_t level);

#endif
