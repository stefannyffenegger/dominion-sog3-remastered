/* Game logic: the occupied-cells map, distances, unit boxes, effect frames (threatmap.c) */
#ifndef DOMINION_ISO_THREATMAP_H
#define DOMINION_ISO_THREATMAP_H

#include <stdint.h>

int32_t __cdecl occ_free(void);
int32_t __cdecl occ_new(void);
void __cdecl occ_cell_raise(int32_t cell, int16_t by);
void __cdecl occ_cell_lower(int32_t cell, int16_t by);
uint32_t __cdecl occ_get(int32_t cell);
void __cdecl occ_unit_raise(int16_t id, int16_t by);
void __cdecl occ_unit_lower(int16_t id, int16_t by);
int64_t __cdecl point_distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
int32_t __cdecl point_distance_axes(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void __cdecl unit_screen_box(uint32_t id, int32_t *x, int32_t *y, int32_t *w, int32_t *h);
void __cdecl effect_timer(uint32_t id, uint32_t arg);
void __cdecl effect_next_frame(uint32_t id);

#endif
