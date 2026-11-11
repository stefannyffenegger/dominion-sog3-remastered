/* Engine core: the engine window's procedure (rt_wndproc.c) */
#ifndef DOMINION_RTLIB_RT_WNDPROC_H
#define DOMINION_RTLIB_RT_WNDPROC_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
void __stdcall paint_black(HDC dc);
void __stdcall window_activate(HWND hwnd, U active_arg, U unused);
ATOM __stdcall window_class_register(U skip);
void __cdecl idle_rearm(void);
void __cdecl window_stopped(void);
void __cdecl window_close(void);
void __cdecl deferred_run(void);
void __cdecl window_paint(void);
void __stdcall window_alt_key(HWND hwnd, WPARAM wp, LPARAM lp);
LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
