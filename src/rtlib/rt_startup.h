/* Engine core: startup and shutdown (rt_startup.c) */
#ifndef DOMINION_RTLIB_RT_STARTUP_H
#define DOMINION_RTLIB_RT_STARTUP_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
uint8_t *__cdecl rt_alloc(void);
U __stdcall rt_block_free(uint8_t *rt);
void __stdcall joystick_calibrate(int n);
U __stdcall window_create(U arg);
U __cdecl libraries_init(void);
uint8_t *__cdecl RtInit(U unused, U arg);
U __cdecl RtFree(uint8_t *rt);
U __cdecl colors_check(void);
U __cdecl input_hook_3(void);
U __cdecl input_hook_1(void);
U __cdecl input_hook(void);
U __cdecl RtStartup(HMODULE module, U arg, LPCSTR cmdline);

#endif
