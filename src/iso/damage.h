/* Game logic: damage (damage.c) */
#ifndef DOMINION_ISO_DAMAGE_H
#define DOMINION_ISO_DAMAGE_H

#include <stdint.h>

int __cdecl effect_damage(uint32_t id, uint32_t e, int16_t distance);
void __cdecl unit_take_damage(uint8_t *u, uint32_t attacker, int16_t percent);
int __cdecl weapon_pierces(uint32_t a, uint32_t b);
int __cdecl weapon_pierces_carried(uint32_t a, uint32_t b);

#endif
