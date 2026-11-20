/* Game logic: shade levels, the default ini, directions (shade.c) */
#ifndef DOMINION_ISO_SHADE_H
#define DOMINION_ISO_SHADE_H

#include <stdint.h>

int32_t __stdcall IsoSetShadeLevel(uint8_t *iso, uint32_t unused, int32_t which, int32_t level);
void __cdecl ini_write_default(void);
void __cdecl table_10150d68_get(uint32_t unused, int16_t k, void *out);
int32_t __cdecl direction_between(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

#endif
