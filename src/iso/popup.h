/* Game logic: the popup menu, the mouse over a unit, click or drag (popup.c) */
#ifndef DOMINION_ISO_POPUP_H
#define DOMINION_ISO_POPUP_H

#include <stdint.h>

int __cdecl menu_count(uint32_t lo, int32_t hi);
void __cdecl menu_for_unit(uint16_t id, uint64_t *items, uint64_t *ticked);
void __cdecl popup_menu_open(int x, int y, uint32_t id, int selection);
int __cdecl mouse_over_unit(uint16_t id, uint16_t *hit);
int __cdecl mouse_click_kind(uint8_t *kind, uint16_t *id);

#endif
