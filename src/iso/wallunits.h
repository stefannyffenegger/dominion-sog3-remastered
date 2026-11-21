/* Game logic: the units standing on the line of a planned wall (wallunits.c) */
#ifndef DOMINION_ISO_WALLUNITS_H
#define DOMINION_ISO_WALLUNITS_H

#include <stdint.h>

int32_t __cdecl posts_too_close(uint8_t *pa, uint8_t *pb);
int32_t __cdecl units_between(uint8_t *pa, uint8_t *pb, int32_t *ids);

#endif
