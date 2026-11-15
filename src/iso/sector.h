/* Game logic: sectors and small helpers (sector.c) */
#ifndef DOMINION_ISO_SECTOR_H
#define DOMINION_ISO_SECTOR_H

#include <stdint.h>

void __cdecl sector_leave(uint8_t *u);
void __cdecl sector_enter(uint8_t *u, int16_t *s, int16_t sector);
int __cdecl unit_is_local(uint32_t id);
int __cdecl action_counts(uint8_t a);
int __cdecl unit_is_local_u(uint8_t *u);
int16_t __cdecl player_slot_alloc(uint8_t player);
void __cdecl player_slot_free(int16_t id);

#endif
