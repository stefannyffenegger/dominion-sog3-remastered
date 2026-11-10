/* The sprite engine's drawing pass (draw.c) */
#ifndef DOMINION_RTLIB_DRAW_H
#define DOMINION_RTLIB_DRAW_H

#include <stdint.h>
#include <windows.h>

int __stdcall draw_iface_rect(const int32_t *r);
uint8_t *__stdcall draw_sprites(const RECT *r, int panel, int pass, uint8_t *from);
void __stdcall draw_parts(const RECT *r, uint8_t *s, int panel);
int __stdcall draw_view_sprites(const void *r, uint32_t level);
void __stdcall draw_background(uint32_t w, uint32_t h);
void __stdcall screen_blit(uint8_t *bm);
void __cdecl draw_sprite_into(uint8_t *s, uint32_t dst);

#endif
