/* Engine core: showing and hiding the scene with a fade or wipe (rt_fade.c) */
#ifndef DOMINION_RTLIB_RT_FADE_H
#define DOMINION_RTLIB_RT_FADE_H

#include <stdint.h>

void __cdecl fade_step(void);
void __stdcall scene_show(uint32_t unused);
void __stdcall scene_hide(uint32_t unused);
void __stdcall scene_cmd(const int16_t *c);
void __stdcall scene_show_fn(uint32_t unused);
void __stdcall scene_hide_fn(uint32_t unused);

#endif
