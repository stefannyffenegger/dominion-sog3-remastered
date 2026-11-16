/* Game logic: the mouse over the map (mouse.c) */
#ifndef DOMINION_ISO_MOUSE_H
#define DOMINION_ISO_MOUSE_H

#include <stdint.h>

int __cdecl cell_free_for_cursor(int x, uint32_t y, int building);
void __cdecl mouse_state(int *selected_any, int *unused, int *over_map, uint32_t *cursor, int *fixed);
uint32_t __cdecl cursor_for_unit(uint32_t id, int unused, int selected_any, int unused2, uint32_t cursor, int mode);

int __cdecl selection_abilities(void);
void __cdecl unit_name_popup(uint32_t id);
void __cdecl cursor_scroll(int state, int dx, int dy);
void __cdecl cursor_follow(void);

#endif
