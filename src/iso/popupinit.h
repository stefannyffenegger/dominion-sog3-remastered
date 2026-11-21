/* Game logic: the map margin test and loading the popup menu's graphics (popupinit.c) */
#ifndef DOMINION_ISO_POPUPINIT_H
#define DOMINION_ISO_POPUPINIT_H

#include <stdint.h>

int32_t __cdecl on_map_px(int32_t x, int32_t y);
void __stdcall IsoInitPopupMenuC(uint8_t *iso, uint32_t res, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                 uint32_t r4, uint32_t r5, uint32_t r6, uint32_t value);

#endif
