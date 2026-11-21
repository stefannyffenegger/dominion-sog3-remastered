/* Game logic: a patrolling vehicle's held cell, and stopping a patrol (patrolcell.c) */
#ifndef DOMINION_ISO_PATROLCELL_H
#define DOMINION_ISO_PATROLCELL_H

#include <stdint.h>

void __cdecl patrol_cell_taken(uint8_t *u, uint32_t *p);
void __cdecl patrol_stop(uint8_t *u);

#endif
