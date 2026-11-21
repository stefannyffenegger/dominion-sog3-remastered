/* Game logic: map objects (objects.c) */
#ifndef DOMINION_ISO_OBJECTS_H
#define DOMINION_ISO_OBJECTS_H

#include <stdint.h>

int32_t __stdcall IsoAddObjectC(uint8_t *iso, int32_t type, int32_t x, int32_t y, uint32_t flags, int32_t kind,
                                int32_t level);

#endif
