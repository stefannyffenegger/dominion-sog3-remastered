/* Engine core: mouse, keys, click regions and hover (rt_input.c) */
#ifndef DOMINION_RTLIB_RT_INPUT_H
#define DOMINION_RTLIB_RT_INPUT_H

#include <stdint.h>
#include <windows.h>

void __stdcall click_set(uint8_t *c);
void __stdcall mouse_down(uint32_t msg, uint32_t keys, uint32_t lparam);
int __stdcall mouse_handler(uint32_t what);
int __stdcall handler_run(uint32_t handler, uint32_t arg);
void __stdcall mouse_up(uint32_t msg, uint32_t lparam);
void __stdcall drag_end(int x, int y);
void __stdcall click_rects_handler(uint32_t handler);
void __stdcall click_objects_handler(uint32_t handler);
uint32_t __stdcall click_cmd(uint8_t *c);
void __stdcall click_all_handler(uint32_t handler);
void __stdcall click_all_disable(uint32_t off);
uint32_t __stdcall drag_cmd(uint8_t *c);
void __stdcall key_down(uint32_t key);
int __stdcall key_handler(uint32_t key);
void __cdecl pause_toggle(void);
void __stdcall key_up(uint32_t key);
void __stdcall mouse_to_view(POINT *pt);
void __stdcall mouse_move(uint32_t lparam);
uint32_t __cdecl hover_sprite_leave(void);
void __stdcall hover_regions(int x, int y);
uint32_t __stdcall hover_sprite(int x, int y);
void __stdcall key_cmd(uint8_t *c);
uint32_t __stdcall collide_cmd(uint8_t *c);

#endif
