/* Game logic: the drag box exports, paint hooks, keyboard hooks (rubber.c) */
#ifndef DOMINION_ISO_RUBBER_H
#define DOMINION_ISO_RUBBER_H

#include <stdint.h>
#include <windows.h>

void __stdcall RubberBand(uint8_t *iso, uint32_t handler);
void __stdcall RubberSetActiveBox(uint8_t *iso, int32_t left, int32_t top, int32_t right, int32_t bottom);
void __stdcall rubber_hook_off(uint8_t *iso, int on);
void __stdcall rubber_paint_hook(uint8_t *iso, HDC dc, RECT *area, int pass);
void __stdcall editpath_paint_hook(uint8_t *iso, uint32_t a, uint32_t b, int pass);
void __stdcall IsoRubberBand(uint8_t *iso, uint32_t handler, int on);
int __stdcall IsoEditTextHook(uint8_t *iso, uint32_t handler);
int __stdcall IsoKeyboardHook(uint8_t *iso, uint32_t handler);
int __stdcall IsoExitGameHook(uint8_t *iso, uint32_t handler);
uint32_t __stdcall IsoCtrlPressed(uint8_t *iso);
uint32_t __stdcall IsoShiftPressed(uint8_t *iso);
void __stdcall IsoSetIgnoreInputFlag(uint8_t *iso, uint32_t flag);
void __stdcall editpath_message(UINT msg, WPARAM wp, LPARAM lp);

#endif
