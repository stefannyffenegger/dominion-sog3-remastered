/* The sprite engine state and sprite display (spritelib.c) */
#ifndef DOMINION_RTLIB_SPRITELIB_H
#define DOMINION_RTLIB_SPRITELIB_H

#include <stdint.h>
#include <windows.h>

void __stdcall SpriteExitLibrary(uint8_t *hsl);
uint32_t __stdcall hsl_free(uint8_t *hsl);
uint8_t *__cdecl hsl_alloc(void);
uint8_t *__stdcall SpriteInitLibrary(HWND hwnd, HDC dc, int depth);
void __stdcall SpriteResetLibrary(uint8_t *hsl);
void __stdcall set_screen_mode(uint8_t *hsl, uint32_t mode, uint32_t top_down, uint32_t v9f0, int a, int b, int c);
void __stdcall screen_size_from_desktop(uint8_t *hsl);
uint32_t __stdcall SpriteSetComposeFlags(uint8_t *hsl, uint32_t top_down, uint32_t resized, uint32_t dib_section);
void __stdcall set_8c0(uint8_t *hsl, int a, int b, int c);
void __stdcall set_b60(uint8_t *hsl, uint32_t v);
void __stdcall SpriteRegisterOverlaySprite(uint8_t *hsl, uint32_t s);
int __stdcall screen_dib_fit(int w, int h);
void __stdcall background_draw(int x, int y, uint8_t *bm);
uint32_t __stdcall cmd_cels(uint8_t *s);
void __stdcall sprite_reposition(uint8_t *s);
int __stdcall sprite_redraw_both(uint8_t *s);
void __stdcall sprite_erase(uint8_t *s, int clip);
void __stdcall sprite_move(uint8_t *s, int x, int y, int relative, int redraw);
void __stdcall SpriteHide(uint8_t *hsl, uint8_t *s, int erase);
void __stdcall SpriteReplaceBitmap(uint8_t *hsl, uint8_t *s, uint32_t old, uint32_t new);
void __stdcall SpriteShow(uint8_t *hsl, uint8_t *s, uint32_t cel, int redraw);
void __stdcall SpriteShowOpaque(uint8_t *hsl, uint8_t *s);

#endif
