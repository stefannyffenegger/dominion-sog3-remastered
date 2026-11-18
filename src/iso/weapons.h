/* Game logic: instant hits, area damage, attack checks, effect sprites (weapons.c) */
#ifndef DOMINION_ISO_WEAPONS_H
#define DOMINION_ISO_WEAPONS_H

#include <stdint.h>

uint32_t __cdecl effect_slot_take(uint8_t keep, uint16_t owner);
int32_t __cdecl unit_random_point(uint32_t firer, uint32_t target, int16_t *x, int16_t *y);
int32_t __cdecl weapon_hit_instant(uint32_t id, uint32_t unit, int32_t x, int32_t y, uint32_t target,
                                   int32_t weapon);
void __cdecl effect_area_hit(uint32_t id, int skip_target, void(__cdecl *fn)(uint32_t));
int32_t __cdecl unit_can_attack(uint32_t attacker, uint32_t target, int no_structures, int allies);
int32_t __cdecl selection_can_attack(uint32_t target, int no_structures, int allies);

#endif
