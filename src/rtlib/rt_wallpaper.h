/* Engine core: Living Wall Paper mode and screen captures (rt_wallpaper.c) */
#ifndef DOMINION_RTLIB_RT_WALLPAPER_H
#define DOMINION_RTLIB_RT_WALLPAPER_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
void __stdcall capture_name(char *out, char *name, U unused, U n);
void __stdcall band_draw(const RECT *r);
void __stdcall band_start(int x, int y);
void __stdcall band_move(int x, int y);
void __stdcall wallpaper_name(char *out, U unused);
void __stdcall wallpaper_set(char *path, U tiled);
void __stdcall wallpaper_capture(int32_t *r, U tiled);
void __stdcall band_end(U cancel);
void __stdcall screen_capture_file(char *name, U n);
BOOL __stdcall wallpaper_close_other(HWND hwnd, LPARAM unused);
void __cdecl wallpaper_single(void);
LRESULT __stdcall wallpaper_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
