/* Game logic: moving to a point, patrols, build-up, structure animation (orders2.c) */
#ifndef DOMINION_ISO_ORDERS2_H
#define DOMINION_ISO_ORDERS2_H

#include <stdint.h>

void __cdecl unit_move_to(uint32_t unit, int32_t x, uint32_t y, uint16_t flag);
void __cdecl unit_patrol(uint32_t unit, const int32_t *points, const int32_t *delays, int32_t count, int32_t mode);
int32_t __cdecl unit_buildup_step(uint32_t unit);
int32_t __cdecl unit_buildup(uint32_t unit);
int32_t __cdecl unit_buildup_set(uint32_t unit, int32_t state);
int32_t __cdecl structure_set_anim(uint32_t unit, int32_t state);
int32_t __cdecl structure_anim_step(uint32_t unit);

#endif
