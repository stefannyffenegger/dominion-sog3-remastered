/* Game logic: text sprites, preloading, special units, random numbers (misc.c) */
#ifndef DOMINION_ISO_MISC_H
#define DOMINION_ISO_MISC_H

#include <stdint.h>

void __cdecl IsoHideTextSprites(uint8_t *iso);
int32_t __cdecl player_preload_units(int32_t player);
void __cdecl player_replace_d6cb(uint32_t id, uint32_t player);
void __cdecl player_replace_d6f9(uint32_t id, uint32_t player);
int32_t __cdecl IsoRand(uint8_t *iso, int32_t lo, int32_t hi);
void __cdecl IsoBriefingScreen(uint8_t *iso, int32_t on);
uint8_t __cdecl IsoInBriefingScreen(uint8_t *iso);
void __cdecl path_unescape(char *path);

#endif
