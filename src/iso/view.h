/* Game logic: the power bar, scrolling, the radar's view box, selections (view.c) */
#ifndef DOMINION_ISO_VIEW_H
#define DOMINION_ISO_VIEW_H

#include <stdint.h>

void __cdecl power_bar_set(int uses, uint32_t gives);
void __cdecl IsoInitIntfPowerBar(uint8_t *iso, uint32_t res, uint16_t player);
void __cdecl IsoPowerBarBrowse(uint8_t *iso);
int __cdecl view_set_scroll_box(void);
int __cdecl IsoCheckRadarScreenAreaUpdate(uint8_t *iso);
void __cdecl view_shake(int32_t amount, int32_t times);
int __cdecl view_scroll_keys(int *dx, int *dy);
int __cdecl view_scroll(int forced, int dx, int dy);
void __cdecl view_center_on(uint32_t id);
int __cdecl selection_clear(int player);
void __cdecl selection_remove(uint32_t id, uint32_t player, int notify);

#endif
