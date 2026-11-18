/* Game logic: the radar's dots and scale (radarmap.c) */
#ifndef DOMINION_ISO_RADARMAP_H
#define DOMINION_ISO_RADARMAP_H

#include <stdint.h>

void __cdecl radar_plot(int32_t at, uint8_t c, int32_t which);
int32_t __cdecl radar_plot_object(uint8_t *o, int32_t erase, int32_t which);
void __cdecl radar_set_scale(void);
int32_t __stdcall IsoCreateRadarBgndBitmap(uint8_t *iso);
int32_t __stdcall IsoSetRadarBitmap(uint8_t *iso, uint32_t sprite, uint32_t fog, uint32_t radar, uint32_t bg);
int32_t __stdcall IsoUpdateRadarFogBitmap(uint8_t *iso, uint32_t x0, uint32_t y0, int32_t x1, int32_t y1,
                                          uint8_t mask);
int32_t __stdcall IsoCreateRadarScreenArea(uint8_t *iso);

#endif
