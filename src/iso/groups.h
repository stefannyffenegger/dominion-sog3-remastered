/* Game logic: the players' numbered groups (groups.c) */
#ifndef DOMINION_ISO_GROUPS_H
#define DOMINION_ISO_GROUPS_H

#include <stdint.h>

int32_t __cdecl records_1017c888_alloc(int32_t n);
uint32_t __cdecl unit_group_entry(int32_t unit);
void __cdecl unit_groups_drop(int32_t unit);
int32_t __cdecl unit_group_slot(int32_t unit);
void __cdecl group_attack(uint8_t *u, uint32_t target);

#endif
