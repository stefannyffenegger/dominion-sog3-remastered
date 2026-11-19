/* Game logic: wall segments (walls.c) */
#ifndef DOMINION_ISO_WALLS_H
#define DOMINION_ISO_WALLS_H

#include <stdint.h>

int32_t __cdecl unit_is_post(int16_t unit);
int16_t __cdecl wall_segment_alloc(void);
int16_t __cdecl wall_segment_build(uint32_t a, uint32_t b);
void __cdecl wall_crush_between(uint8_t *a, uint8_t *b);
int32_t __cdecl walls_connect(uint32_t a, uint32_t b, uint32_t unused, int check);

#endif
