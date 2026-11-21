/* Game logic: building and showing the popup menu (popupshow.c) */
#ifndef DOMINION_ISO_POPUPSHOW_H
#define DOMINION_ISO_POPUPSHOW_H

#include <stdint.h>

void __stdcall IsoShowPopupMenuC(uint8_t *iso, uint64_t mask, uint64_t enabled, uint64_t checked, int32_t px,
                                 int32_t py, uint32_t id);

#endif
