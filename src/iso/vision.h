/* Game logic: a unit's sight and the fog (vision.c) */
#ifndef DOMINION_ISO_VISION_H
#define DOMINION_ISO_VISION_H

#include <stdint.h>

void __cdecl iso_reveal(int32_t x, uint32_t y, uint32_t player, int16_t radius, uint32_t dir, uint16_t unit,
                        int32_t force);

#endif
