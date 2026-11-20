/* Game logic: mining energy wells (mining.c) */
#ifndef DOMINION_ISO_MINING_H
#define DOMINION_ISO_MINING_H

#include <stdint.h>

uint32_t __cdecl building_output(uint32_t unit, uint32_t max);
uint32_t __cdecl well_fill_frame(uint8_t *w, int32_t force, int32_t query);
int32_t __cdecl refinery_mine(uint32_t unit);

#endif
