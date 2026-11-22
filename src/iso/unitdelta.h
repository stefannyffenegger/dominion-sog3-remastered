/* Game logic: how far a unit moves in a number of animation steps (unitdelta.c) */
#ifndef DOMINION_ISO_UNITDELTA_H
#define DOMINION_ISO_UNITDELTA_H

#include <stdint.h>

int32_t __cdecl unit_step_delta(uint32_t id, int32_t steps, const uint8_t *data, uint32_t frame, uint32_t phase,
                                uint32_t dir, int32_t *dx, int32_t *dy, uint8_t *odd);

#endif
