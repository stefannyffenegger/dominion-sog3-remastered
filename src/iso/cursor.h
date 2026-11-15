/* Game logic: the in-game mouse cursor and font (cursor.c) */
#ifndef DOMINION_ISO_CURSOR_H
#define DOMINION_ISO_CURSOR_H

#include <stdint.h>

int __stdcall IsoLoadMouseCursor(uint8_t *iso, uint32_t table, uint32_t sprite, uint32_t sprite_ptr, uint32_t event);
void __stdcall IsoUnloadMouseCursor(uint8_t *iso);
void __stdcall IsoEnableMouseCursor(uint8_t *iso, int on);
uint32_t __stdcall IsoSetMouseCursorState(uint8_t *iso, uint32_t state, int shown);
uint32_t __stdcall IsoGetMouseCursorState(uint8_t *iso);
int __stdcall IsoSetMouseCursorOp(uint8_t *iso, uint32_t op);
uint32_t __stdcall IsoGetMouseCursorOp(uint8_t *iso);
void __cdecl IsoInitInGameFont(uint8_t *iso, uint32_t font, uint32_t font2, uint32_t popup, uint32_t popup_dib);

#endif
