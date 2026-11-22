/* Game logic: one tick of a structure (structtick.c) */
#ifndef DOMINION_ISO_STRUCTTICK_H
#define DOMINION_ISO_STRUCTTICK_H

#include <stdint.h>

void __cdecl structure_tick(uint32_t id);
void repair_step(uint32_t id, uint8_t *u);

#endif
