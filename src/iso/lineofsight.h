/* Game logic: clear lines between units (lineofsight.c) */
#ifndef DOMINION_ISO_LINEOFSIGHT_H
#define DOMINION_ISO_LINEOFSIGHT_H

#include <stdint.h>

void __cdecl line_visit(int32_t x, int32_t y);
int32_t __cdecl unit_is_wall(int16_t unit);
int32_t __cdecl map_cell_valid(int32_t x, int32_t y);
int32_t __cdecl unit_route_kind1(int16_t unit, int16_t dir);
int32_t __cdecl unit_route_kind2(int16_t unit, int16_t dir);
int32_t __cdecl line_side_blocked(int32_t x, uint32_t y, int16_t dir);
int32_t __cdecl line_squeezed(int32_t x1, uint32_t y1, int32_t x2, uint32_t y2, int16_t dir);
int32_t __cdecl unit_fire_line_clear(uint32_t unit, uint32_t target);

#endif
