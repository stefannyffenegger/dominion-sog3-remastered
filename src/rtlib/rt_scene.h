/* Engine core: scenes (rt_scene.c) */
#ifndef DOMINION_RTLIB_RT_SCENE_H
#define DOMINION_RTLIB_RT_SCENE_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __stdcall scene_var_set(U n, U v);
void __cdecl scene_palette_load(void);
void __cdecl scenes_trim(void);
void __cdecl input_flush(void);
void __stdcall scene_change(char *path);
U __stdcall scene_load(char *name, U progress, U cb, U arg);
void __cdecl scene_names_load(void);
U __stdcall scene_open(char *name);
U __stdcall scene_read(char *path);
U __stdcall scene_find(char *path);
U __stdcall scene_enter(char *path, U check_memory);
U __stdcall scene_enter_cb(char *path, U check_memory, U cb, U arg);
void __cdecl scene_state_reset(void);
void __cdecl scene_exit(void);

#endif
