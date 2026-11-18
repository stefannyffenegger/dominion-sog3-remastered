/* Game logic: the game window's message handler and the drag box (wndproc.c) */
#ifndef DOMINION_ISO_WNDPROC_H
#define DOMINION_ISO_WNDPROC_H

#include <stdint.h>
#include <windows.h>

void __stdcall drag_box_draw(HDC dc);
int __stdcall drag_start(LPARAM pos);
int __stdcall drag_end(int cancel);
int __stdcall drag_move(LPARAM pos);
LRESULT __stdcall iso_window_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
