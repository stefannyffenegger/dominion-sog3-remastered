/* Game logic: where a unit aims for at a node of its path (pathoffset.c) */
#ifndef DOMINION_ISO_PATHOFFSET_H
#define DOMINION_ISO_PATHOFFSET_H

#include <stdint.h>

int32_t __cdecl path_point_offset(uint32_t id, uint32_t node, uint32_t dx, uint32_t dy, int16_t *ox, int16_t *oy);

#endif
