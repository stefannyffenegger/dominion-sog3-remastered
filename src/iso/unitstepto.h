/* Game logic: the animation steps to a unit's next path node (unitstepto.c) */
#ifndef DOMINION_ISO_UNITSTEPTO_H
#define DOMINION_ISO_UNITSTEPTO_H

#include <stdint.h>

int32_t __cdecl path_steps_to(uint32_t id, uint32_t x, uint32_t y, uint32_t dir, uint32_t next, uint32_t action,
                              uint32_t node, uint32_t odd, int32_t cycle, const uint8_t *data, int32_t unused,
                              int16_t *out, int32_t *frames);

#endif
