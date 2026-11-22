/* Game logic: where a unit will be after a number of frames (unitwalk.c) */
#ifndef DOMINION_ISO_UNITWALK_H
#define DOMINION_ISO_UNITWALK_H

#include <stdint.h>

/* the entry stub (notes two stack slots) and the function behind it */
int32_t __cdecl unit_walk(uint32_t id, int16_t *pos, uint8_t *dirp, int32_t frames, int16_t *nodes, int32_t *still,
                          int32_t *cell);
int32_t __cdecl unit_walk_c(uint32_t id, int16_t *pos, uint8_t *dirp, int32_t frames, int16_t *nodes, int32_t *still,
                            int32_t *cell);

#endif
