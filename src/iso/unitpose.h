/* Game logic: the animation a unit plays for its action (unitpose.c) */
#ifndef DOMINION_ISO_UNITPOSE_H
#define DOMINION_ISO_UNITPOSE_H

#include <stdint.h>

uint32_t __cdecl unit_pose(uint32_t id, uint32_t action, uint32_t node, uint32_t dir, uint32_t next, uint8_t *out,
                           int32_t standing, uint8_t *flip, int32_t *moving);

#endif
