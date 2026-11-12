/* Engine core: geometry built-ins and the controller layer's mouse steering (rt_geom.c) */
#ifndef DOMINION_RTLIB_RT_GEOM_H
#define DOMINION_RTLIB_RT_GEOM_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
int __stdcall div10000(int v);
void __stdcall ray_to_rect(int angle, int x, int y, int left, int top, int right, int bottom, int *out);
void __stdcall builtin_angle(uint8_t *c);
void __stdcall builtin_intersect(uint8_t *c);
void __stdcall builtin_sin_cos(uint8_t *c);
void __stdcall builtin_ray_to_rect(uint8_t *c);
uint8_t __cdecl mouse_steer(U pos);
U __stdcall mouse_controller(U msg, U pos);
void __stdcall mouse_controller_reset(U unused, U reset);

#endif
