/* Game logic: a new map (createmap.c) */
#ifndef DOMINION_ISO_CREATEMAP_H
#define DOMINION_ISO_CREATEMAP_H

#include <stdint.h>

int __stdcall IsoCreateMap(uint8_t *iso, uint32_t w, uint32_t h, int scenario, int load_tiles, int from_file,
                           int autofill, uint32_t fill_first, int fill_count);

#endif
