/* Game logic: a structure put on the map (structplace.c) */
#ifndef DOMINION_ISO_STRUCTPLACE_H
#define DOMINION_ISO_STRUCTPLACE_H

#include <stdint.h>

int32_t __cdecl structure_place(uint32_t id, uint32_t unused, uint8_t player, int16_t x_in, int16_t y_in, uint8_t pose,
                                uint8_t level, int32_t quiet);

#endif
