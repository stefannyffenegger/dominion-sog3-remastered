/* Engine core: text objects (rt_text.c) */
#ifndef DOMINION_RTLIB_RT_TEXT_H
#define DOMINION_RTLIB_RT_TEXT_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __cdecl var_value(U n);
HFONT __stdcall font_get(LPCSTR name, int height);
U __stdcall font_release(HFONT font);
void __stdcall text_font_update(uint8_t *t);
HBITMAP __stdcall text_draw_bitmap(uint8_t *t, HDC dc);
U *__stdcall text_bitmap(uint8_t *t);
void __stdcall text_place(const uint8_t *t, uint8_t *bits);
void __stdcall text_replace(const uint8_t *t, void *old, U new_bits);
U __stdcall text_render(uint8_t *t);
U __stdcall text_free(uint8_t *t);
uint8_t *__stdcall text_create(const U *res, U id);
void __stdcall text_detach(uint8_t *t, U sprite);
U __stdcall text_attach(uint8_t *t, U sprite);
U __stdcall text_set(const uint8_t *c);
U __stdcall text_set_color(const uint8_t *c);

#endif
