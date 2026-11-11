/* Engine core: plugins, the game's .r32 modules (rt_plugin.c) */
#ifndef DOMINION_RTLIB_RT_PLUGIN_H
#define DOMINION_RTLIB_RT_PLUGIN_H

#include <stdint.h>
#include <windows.h>

uint32_t __stdcall rt_set_cef0(uint32_t a, uint32_t unused1, uint32_t unused2);
void __stdcall plugins_notify_18(uint32_t arg);
void __stdcall plugins_pause(uint32_t paused);
void __stdcall plugins_notify_20(uint32_t a, uint32_t b);
void __cdecl plugins_service(void);
void __cdecl plugins_palette(void);
void __cdecl plugins_unload(void);
void __stdcall plugin_free(HMODULE dll, uint32_t ctx);
void __cdecl plugins_free_dlls(void);
uint32_t __cdecl plugins_load(void);
uint32_t __stdcall plugin_add(HMODULE dll, LPCSTR name);
int __cdecl plugins_bind_functions(void);

#endif
