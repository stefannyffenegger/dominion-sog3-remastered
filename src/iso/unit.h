/* Game logic: placing a unit, upgrades, sectors (unit.c) */
#ifndef DOMINION_ISO_UNIT_H
#define DOMINION_ISO_UNIT_H

#include <stdint.h>

int __cdecl unit_apply_upgrade(uint32_t id);
int __cdecl unit_place(uint32_t id, uint8_t a16, uint8_t player, int16_t x, int16_t y, uint8_t dir,
                       uint8_t a30, int loaded);
void __cdecl unit_sector_update(uint32_t id, int16_t mode, int reveal);

#endif
