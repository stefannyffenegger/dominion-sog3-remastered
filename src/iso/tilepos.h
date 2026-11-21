/* Game logic: screen points to map cells (tilepos.c) */
#ifndef DOMINION_ISO_TILEPOS_H
#define DOMINION_ISO_TILEPOS_H

#include <stdint.h>

int32_t __stdcall IsoVTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy);
int32_t __stdcall IsoSnapTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy);
int32_t __stdcall IsoTilePosC(uint8_t *iso, int *x, uint32_t *y, int *sx, int *sy);

#endif
