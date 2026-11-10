/* Engine core: backgrounds, palette commands, growable arrays, screen capture and
 * background transitions (rt_bg.c) */
#ifndef DOMINION_RTLIB_RT_BG_H
#define DOMINION_RTLIB_RT_BG_H

#include <stdint.h>
#include <windows.h>

typedef struct RtArray {
    uint8_t *data;
    uint32_t count, capacity, elem, grow;
} RtArray;

void __stdcall bg_cmd(uint8_t *c);
void __stdcall bg_set(uint8_t *c);
void __cdecl bg_clear(void);
void __stdcall bg_load(uint8_t *p);
void __stdcall bg_size(uint8_t *bm);
uint8_t *__stdcall bg_bitmap(uint32_t id);
void __stdcall bg_repaint(RECT *r);
void __stdcall bg_apply(RECT *out);
void __stdcall bg_color(uint32_t color);
void __stdcall bg_fill(uint32_t color);
void __stdcall bg_tiles(uint32_t map, int repaint);
int __stdcall bg_transition(uint8_t *c);
void __stdcall palette_cmd(uint8_t *c);
void __stdcall palette_load_cmd(uint8_t *c);
void __cdecl RtGetScrollBox(uint8_t *rt, int32_t *r);
uint32_t __cdecl RtGetBackground(uint8_t *rt);
void __cdecl RtSetBackground(uint8_t *rt, uint32_t bm, int repaint);
void __cdecl RtSetComposeBackground(uint8_t *rt, int repaint);
void __cdecl RtSetScrollBox(uint8_t *rt, const int32_t *r);
void __cdecl RtSetTileBackground(uint8_t *rt, uint32_t map, int repaint);

void *__stdcall array_add(RtArray *a);
uint32_t __stdcall array_remove(RtArray *a, uint32_t i);
void __stdcall array_free(RtArray *a);
void *__stdcall array_at(RtArray *a, uint32_t i);
int __stdcall array_init(RtArray *a, uint32_t elem, uint32_t n, uint32_t grow);

void __cdecl rt_error(int code, ...);
void __cdecl rt_debug_str(LPCSTR s);

BITMAPINFO *__stdcall screen_grab(int32_t *r);
BITMAPINFO *__stdcall bitmap_to_dib(HBITMAP bm, uint32_t unused);
HBITMAP __stdcall screen_grab_bitmap(int32_t *r);
HPALETTE __cdecl palette_copy(void);
int __stdcall bmi_colors_size(const uint8_t *bi);
int __stdcall dib_save_bmp(uint8_t *bm, LPSTR path);
int __stdcall bmi_save_bmp(int32_t *bi, LPSTR path);

void __stdcall bg_effect(uint32_t dst, uint32_t src, const uint32_t *params);
void __stdcall fx_blinds(uint32_t n);
void __stdcall fx_close(uint32_t steps, int sides);
void __stdcall fx_open(int steps, int sides);
void __stdcall fx_show(const int32_t *r);
void __stdcall fx_sides(const int32_t *a, const int32_t *b);
void __stdcall fx_bands(const int32_t *a, const int32_t *b);
void __stdcall fx_rect(int step, int32_t *r, int steps, int sides);
void __stdcall fx_roll(uint32_t n, uint32_t ms);

#endif
