/* Game logic: a click or drag on the map (click.c) */
#ifndef DOMINION_ISO_CLICK_H
#define DOMINION_ISO_CLICK_H

#include <stdint.h>

int __cdecl map_click(void);
int __cdecl selection_add(uint32_t id, int *tutorial);
int __cdecl selection_foreign(uint32_t id);
int __cdecl click_move(void);
int __cdecl box_select(void);
int __cdecl structure_link(uint32_t id);
int __cdecl move_marker(int x, int y);

#endif
