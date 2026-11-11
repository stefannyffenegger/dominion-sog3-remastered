/* Engine core: run one command, launch a program, kiosk mode (rt_launch.c) */
#ifndef DOMINION_RTLIB_RT_LAUNCH_H
#define DOMINION_RTLIB_RT_LAUNCH_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __stdcall handler_run_at(U id, int index);
U __stdcall watch_add(HWND hwnd, U existing);
BOOL __stdcall watch_enum(HWND hwnd, LPARAM first);
U __cdecl watch_end(void);
U __cdecl watch_poll(void);
U __cdecl input_hook_2(void);
U __stdcall watch_start(U handler);
U __stdcall program_launch(LPCSTR cmdline, LPCSTR dir, U handler);
LRESULT __stdcall kiosk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
