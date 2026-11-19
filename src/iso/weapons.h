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
uint32_t __stdcall IsoInitWeaponTable(uint8_t *iso, uint32_t res, uint32_t count);
void __stdcall IsoFreeWeaponTable(uint8_t *iso);
int32_t __stdcall weapon_fire_at_unit(uint8_t *iso, int32_t attacker, int32_t target, uint32_t x, uint32_t y);
int32_t __stdcall weapon_fire_at_point(uint8_t *iso, int32_t attacker, uint32_t x, uint32_t y);
void __stdcall IsoSetWeaponDelay(uint8_t *iso, uint32_t delay);
int32_t __stdcall IsoInitWeaponProcess(uint8_t *iso);
void __stdcall IsoStopWeaponProcess(uint8_t *iso);
void __stdcall IsoWeaponMovementLoop(uint8_t *iso, uint32_t unused);
int32_t __cdecl unit_attach_slot(uint16_t unit, int32_t *slot);
int32_t __cdecl unit_slot3_free(uint16_t unit);
int32_t __cdecl unit_effects_clear(uint16_t unit);
void __cdecl effect_start(void *sprite, int32_t level, int32_t x, int32_t y, uint32_t cel, int32_t frames,
                          int32_t flip_x, int32_t flip_y, int32_t delay, uint32_t speed, uint32_t blit);

#endif
