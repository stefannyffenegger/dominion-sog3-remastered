/* Game logic: the map cell a unit stands on (unitcell.c) */
#ifndef DOMINION_ISO_UNITCELL_H
#define DOMINION_ISO_UNITCELL_H

#include <stdint.h>

void __cdecl unit_cell_update(uint32_t unit, uint32_t how);
void __cdecl unit_run_over(uint32_t unit);
void __cdecl unit_cell_update2(uint32_t unit, uint32_t how);

#endif
