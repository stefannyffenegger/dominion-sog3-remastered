/* Game logic: a unit's move order (unitmove.c) */
#ifndef DOMINION_ISO_UNITMOVE_H
#define DOMINION_ISO_UNITMOVE_H

#include <stdint.h>

int32_t __cdecl unit_move_order(uint32_t unit, int16_t x, int16_t y, uint32_t how_arg, uint32_t spread_arg, uint32_t a6,
                                uint32_t a7, int32_t a8, int32_t forced, uint16_t flag);

#endif
