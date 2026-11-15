/* Game logic: infantry idle animations (idle.c) */
#ifndef DOMINION_ISO_IDLE_H
#define DOMINION_ISO_IDLE_H

#include <stdint.h>

void __cdecl idle_wait(uint8_t *u);
int __cdecl idle_solo(uint8_t *u);
int __cdecl idle_member(uint8_t *u);
int __cdecl idle_leader(uint8_t *u);
void __cdecl idle_leader_wait(uint8_t *u);

#endif
