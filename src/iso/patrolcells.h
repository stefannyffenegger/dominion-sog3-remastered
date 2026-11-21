/* Game logic: how many cells a straight leg crosses (patrolcells.c) */
#ifndef DOMINION_ISO_PATROLCELLS_H
#define DOMINION_ISO_PATROLCELLS_H

#include <stdint.h>

int32_t __cdecl cells_between(uint32_t a, uint32_t b, uint32_t dir);

#endif
