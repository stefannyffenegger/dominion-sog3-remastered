/* Engine core: device bitmaps into sprite bitmaps, icons, printing (rt_print.c) */
#ifndef DOMINION_RTLIB_RT_PRINT_H
#define DOMINION_RTLIB_RT_PRINT_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
void __stdcall dib_histogram(uint8_t *dib);
int __stdcall color_find(U rgb);
int __stdcall color_add(U entry);
U __stdcall color_nearest(U rgb);
U __stdcall color_index(U rgb);
void __stdcall colors_map(const RGBQUAD *colors, U transparent, U key);
int __stdcall dib_remap(uint8_t *dib, const RGBQUAD *colors, U transparent, U key);
uint8_t *__stdcall dib_from_bitmap(HBITMAP bm, U remap, U transparent, U key);
HBITMAP __stdcall icon_bitmap(HICON icon, HDC dc, int w, int h);
uint8_t *__stdcall icon_dib(HICON icon, int w, int h);
HICON __stdcall icon_load(char *path, UINT index);
U __stdcall dib_draw_icon(uint8_t *dib, char *path, UINT index);
BOOL __stdcall print_abort(HDC dc, int error);
HDC __stdcall printer_dc(U orientation);
BITMAPINFO *__stdcall print_bmi(uint8_t *dib);
U __stdcall print_dib(HDC dc, uint8_t *dib);
U __stdcall print_bitmap_dc(HDC dc, U id);
U __stdcall print_bitmap(U id, U landscape);

#endif
