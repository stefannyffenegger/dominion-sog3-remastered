/* Game logic: players, alliances, ground marks (players.c) */
#ifndef DOMINION_ISO_PLAYERS_H
#define DOMINION_ISO_PLAYERS_H

#include <stdint.h>

void __cdecl IsoSetSpriteMerge(uint8_t *iso, int32_t merge);
void __cdecl IsoSetMissionEndCallback(uint8_t *iso, int32_t handler);
void __cdecl kill_cb(uint32_t id, uint32_t anim, int32_t sync);
void __cdecl kill_cb2(uint32_t id, uint32_t anim, int32_t sync);
void __cdecl game_over_timer(int32_t a, uint8_t b);
void __cdecl ground_mark(int32_t x, int32_t y, uint32_t seed, int32_t big);
void __cdecl IsoSetDifficulty(uint8_t *iso, uint32_t d);
void __cdecl IsoSetNumPlayers(uint8_t *iso, int32_t n);
int32_t __cdecl IsoGetNumPlayers(uint8_t *iso);
void __cdecl IsoEnableAlliances(uint8_t *iso, int32_t on, int32_t value);

#endif
