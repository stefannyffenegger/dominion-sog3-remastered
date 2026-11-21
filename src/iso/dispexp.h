/* Game logic: scrolling, scenario and display exports (dispexp.c) */
#ifndef DOMINION_ISO_DISPEXP_H
#define DOMINION_ISO_DISPEXP_H

#include <stdint.h>

uint32_t __stdcall IsoSetScrollRateC(uint8_t *iso, uint32_t dx, uint32_t dy, uint32_t delay);
int32_t __stdcall IsoGetScenarioInfoC(uint8_t *iso, uint32_t *out);
void __stdcall IsoGetDisplayModeC(uint8_t *iso, uint32_t *w, uint32_t *h, uint32_t *bpp, uint32_t *refresh);
int32_t __stdcall IsoSetScreenResC(uint8_t *iso, int32_t w, int32_t h, uint32_t bpp, int32_t ox, int32_t oy,
                                   int32_t set, uint32_t v128);
int32_t __stdcall IsoSetDisplayModeC(uint8_t *iso, int32_t w, int32_t h, uint32_t bpp, int32_t ox, int32_t oy,
                                     int32_t set, int32_t flags, uint32_t unused);
void __stdcall IsoGetScreenPosC(uint8_t *iso, int32_t *x, int32_t *y);

#endif
