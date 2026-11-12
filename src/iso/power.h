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

#endif
