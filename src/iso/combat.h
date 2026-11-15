/* Game logic: hits, damage reactions, help, killing units (combat.c) */
#ifndef DOMINION_ISO_COMBAT_H
#define DOMINION_ISO_COMBAT_H

#include <stdint.h>

void __cdecl unit_hit_by(uint32_t id, uint16_t attacker, uint32_t effect, int16_t distance);
void __cdecl unit_hit(uint32_t id);
void __cdecl hit_effect(uint32_t id);
void __cdecl call_for_help(uint32_t id, uint32_t attacker);
void __cdecl unit_kill(uint32_t id, uint32_t anim, int sync, uint32_t quiet);

#endif
