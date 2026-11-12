/* Game logic: screen refresh, radar clicks, display mode, popups, countdown (screen.c) */
#ifndef DOMINION_ISO_SCREEN_H
#define DOMINION_ISO_SCREEN_H

#include <stdint.h>

uint32_t __stdcall IsoGetProfilerValue(uint8_t *iso, uint32_t unused);
void __stdcall IsoRefreshScreen(uint8_t *iso);
void __stdcall IsoRefreshRect(uint8_t *iso, int32_t left, int32_t top, int32_t right, int32_t bottom);
void __stdcall IsoRadarClickPos(uint8_t *iso, int x, int y, int jump);
int __cdecl IsoSetDirectDrawMode(uint8_t *iso, int full);
void __cdecl IsoHideUnitNamePopup(uint8_t *iso);
void __cdecl radar_show(int on);
void __cdecl unit_update_bounds(uint32_t id);
void __cdecl countdown_draw(void);

#endif
