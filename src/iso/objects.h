/* Game logic: map objects (objects.c) */
#ifndef DOMINION_ISO_OBJECTS_H
#define DOMINION_ISO_OBJECTS_H

#include <stdint.h>

int32_t __stdcall IsoAddObjectC(uint8_t *iso, int32_t type, int32_t x, int32_t y, uint32_t flags, int32_t kind,
                                int32_t level);
void __stdcall object_cells_mark(uint8_t *iso, uint32_t k);
int32_t __stdcall IsoAddEnergyWellC(uint8_t *iso, int32_t x, int32_t y, uint32_t left, int32_t total,
                                    int32_t interval, int32_t look, int32_t quiet);

#endif
