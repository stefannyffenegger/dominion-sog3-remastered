/* Game logic: power grids (power.c) */
#ifndef DOMINION_ISO_POWER_H
#define DOMINION_ISO_POWER_H

#include <stdint.h>

void __cdecl power_mark(int16_t id, int16_t radius, int set);
int __cdecl power_reachable(int *x, uint32_t *y, int16_t type, uint8_t player);
int16_t __cdecl power_grid_add(uint16_t id);
void __cdecl power_grid_remove(uint32_t id);
void __cdecl power_grids_clear(uint32_t player);
void __cdecl power_structure_placed(uint16_t id);
void __cdecl power_flag_near(uint32_t id);
void __cdecl power_remark_flagged(void);
void __cdecl power_structure_removed(uint32_t id);
void __cdecl IsoResetAllStructurePowerGrids(uint8_t *iso);
uint32_t __cdecl power_check(int16_t id);
void __cdecl power_icon_hide(uint16_t i);
int16_t __cdecl power_percent(int16_t id);
void __cdecl power_grid_update_x(uint16_t player);
int16_t __cdecl power_grid_fill(uint16_t player, int16_t slot);
void __cdecl power_grid_sum(uint16_t player);
int16_t __cdecl power_icon_alloc(void);
void __cdecl power_icon_free_x(int16_t i);
void __cdecl power_unit_update(uint32_t id);
void __cdecl power_unit_tick(uint32_t id);
void __cdecl power_mark_all(uint32_t on);
int __cdecl IsoDisplayPowerInfo(uint8_t *iso);
int __cdecl power_reach_x(int x, uint32_t y, uint8_t player, int type, int any, int clear);
int __cdecl power_unit_counts(uint8_t *u);
uint32_t __cdecl power_reach_others(int *x, uint32_t *y, int16_t type, uint8_t player);
uint8_t __cdecl power_player_reach(uint8_t *unit);

#endif
