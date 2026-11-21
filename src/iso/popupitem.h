/* Game logic: the pointer over the popup menu, hiding it, its items (popupitem.c) */
#ifndef DOMINION_ISO_POPUPITEM_H
#define DOMINION_ISO_POPUPITEM_H

#include <stdint.h>

void __cdecl popup_item_draw(uint32_t i, uint8_t *dib, int32_t y, int32_t usable, int32_t checked, int32_t hi,
                             const char *text);
int32_t __stdcall IsoBrowsePopupMenuC(uint8_t *iso, int32_t x, int32_t y, int32_t click);
void __stdcall IsoHidePopupMenuC(uint8_t *iso);

#endif
