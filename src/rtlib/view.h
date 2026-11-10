/* The sprite engine's view: background, scrolling, painting (view.c) */
#ifndef DOMINION_RTLIB_VIEW_H
#define DOMINION_RTLIB_VIEW_H

#include <stdint.h>
#include <windows.h>

void __stdcall SpriteSetScrollable(uint8_t *hsl, uint8_t *s, int on);
void __stdcall SpriteSetScrollBox(uint8_t *hsl, const RECT *r);
void __stdcall SpriteSetScrollPos(uint8_t *hsl, int x, int y);
void __stdcall SpriteScroll(uint8_t *hsl, int dx, int dy, int blit);
void __stdcall scroll_paint_x(int l, int t, int r, int b, int dx);
void __stdcall scroll_paint_y(int l, int t, int r, int b, int dy);
void __stdcall tiles_scroll(uint8_t *bm, int dx, int dy, const int32_t *r);
void __stdcall rows_copy_huge(uint8_t *dst, const uint8_t *src, int stride, uint32_t n, int rows);
void __stdcall rows_move(uint8_t *dst, const uint8_t *src, int stride, uint32_t n, int rows);
void __stdcall view2_update(const RECT *box);
void __cdecl view_reset(void);
int __stdcall SpriteSetBackground(uint8_t *hsl, uint8_t *bg, RECT *out);
int __stdcall SpriteSetTileBackground(uint8_t *hsl, uint32_t map, RECT *out);
void __stdcall SpriteSetBkgTransparent(uint8_t *hsl, uint32_t on);
void __stdcall SpriteSetClipBox(uint8_t *hsl, const RECT *r);
void __stdcall SpriteGetClipBox(uint8_t *hsl, RECT *r);
void __stdcall SpriteSetStretch(uint8_t *hsl, uint32_t on);
uint32_t __stdcall SpriteGetStretch(uint8_t *hsl);
int __stdcall SpriteSetZoom(uint8_t *hsl, int zoom, int w, int h);
void __stdcall SpritePaint(uint8_t *hsl, uint8_t *s, HDC dc);
void __stdcall SpritePaintRect(uint8_t *hsl, const RECT *r, int screen);
void __stdcall SpriteRefreshRect(uint8_t *hsl, const RECT *r);
void __stdcall comp_to_background(uint8_t *s, int keep);
void __stdcall comp_parts_to_background(uint8_t *s, const RECT *r);
void __stdcall comp_to_sprite(uint8_t *dst, uint8_t *src);
void __stdcall comp_parts_to_sprite(uint8_t *dst, uint8_t *src, const RECT *r);
void __stdcall SpriteCompToBackground(uint8_t *hsl, uint8_t *s, int keep);
void __stdcall SpriteCompToSprite(uint8_t *hsl, uint32_t a, uint32_t b);
uint8_t *__stdcall screen_copy(uint8_t *hsl);
uint8_t *__stdcall SpriteDibCreate(int w, int h, int zero, uint32_t bpp);
void __stdcall SpriteDibFree(void *p);
void __stdcall SpriteDibGetBox(uint8_t *p, int32_t *r);
void __stdcall SpriteInitScreenCapture(uint8_t *hsl, const char *name);
void __stdcall SpriteSaveScreenShot(uint8_t *p, char *path);

int __stdcall screen_dib_create(int w, int h);
int __cdecl dib_section_create(void);
int __cdecl dib_section_delete(void);
UINT __cdecl dib_section_colors(void);
uint32_t __stdcall view_false4(uint32_t a, uint32_t b, uint32_t c, uint32_t d);

void __stdcall wipe_rect(int step, int32_t *r, int steps, int sides);
void __stdcall wipe_apply(const int32_t *r);
void __stdcall wipe_sides(const int32_t *a, const int32_t *b);
void __stdcall wipe_bands(const int32_t *a, const int32_t *b);
void __stdcall wipe_reveal_run(int steps, int sides);
void __stdcall wipe_blank_run(uint32_t steps, int sides);
void __stdcall wipe_in(uint8_t *hsl, uint8_t *bm, uint32_t steps, int sides);
void __stdcall wipe_out(uint8_t *hsl, uint8_t *bm, uint32_t steps, int sides);

#endif
