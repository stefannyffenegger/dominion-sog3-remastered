/* Game logic: debug sprites, straight lines, message lines, display timing (display.c) */
#ifndef DOMINION_ISO_DISPLAY_H
#define DOMINION_ISO_DISPLAY_H

#include <stdint.h>

void __cdecl debug_sprite_show(int i);
void __cdecl debug_sprite_hide(int i);
void __cdecl debug_sprite_number(int n, int i);
int16_t __cdecl unit_direction(uint32_t a, uint32_t b);
int16_t __cdecl unit_line_clear(uint32_t a, uint32_t b);
void __cdecl messages_draw(void);
int32_t __cdecl table_get(uint32_t *t, int x, int y);
void __stdcall IsoSetDisplayDelay(uint8_t *iso, uint32_t delay);
int __stdcall IsoInitDisplayProcess(uint8_t *iso);
void __stdcall IsoStopDisplayProcess(uint8_t *iso);

#endif
