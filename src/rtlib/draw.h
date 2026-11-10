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

uint32_t __cdecl dib_merge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                           uint32_t cmap);

uint32_t __stdcall dib_merge_plain_cm(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                      uint32_t flip, uint32_t cmap);
uint32_t __stdcall dib_merge_plain9(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy,
                                    uint32_t flip);
void __cdecl dib_fx_merge(uint32_t dst, uint32_t src, int w, int h, int dx, int dy, int sx, int sy, uint32_t flip,
                          uint32_t fx, uint32_t arg, uint32_t cmap);
void __cdecl fx_run(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, uint32_t row,
                    const uint32_t *cmap);
void __cdecl fx_run_mirrored(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t fx, int32_t arg, const uint32_t *cmap);
void __stdcall fx_blend_asm(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend);
void __stdcall fx_blend_cmap_asm(uint8_t *d, const uint8_t *s, uint32_t n, const uint8_t *blend, const uint32_t *cmap);
void __stdcall fx_dither_asm(uint8_t *d, const uint8_t *s, uint32_t n, uint32_t row);
void __cdecl copy_mirrored(uint8_t *d, const uint8_t *s, uint32_t n);
void __cdecl shadow_run(int type, uint8_t *p, uint32_t n, uint32_t row);

#endif
