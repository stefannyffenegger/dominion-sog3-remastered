/* Game logic: sounds on the map, unit voices (sound.c) */
#ifndef DOMINION_ISO_SOUND_H
#define DOMINION_ISO_SOUND_H

#include <stdint.h>

int __cdecl sound_at(const char *name, int16_t x, int16_t y, int a4, int a5, int a6, int a7);
int __cdecl unit_voice(uint8_t *u, int kind, int unused, int centered);
int __cdecl unit_sound(uint8_t *u, int kind, int unused, int centered);
int __cdecl projectile_sound(uint8_t *p, int kind, int unused, int centered);
int __cdecl weapon_sound(uint32_t weapon, int kind, int16_t x, int16_t y, int unused, int centered);
int __cdecl general_sound(int kind, int16_t x, int16_t y, int unused, int centered);
void __stdcall IsoGetSectorCoordinates(uint8_t *iso, int s, int *x, int *y);

#endif
