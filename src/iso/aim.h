/* Game logic: turret aiming math (aim.c) */
#ifndef DOMINION_ISO_AIM_H
#define DOMINION_ISO_AIM_H

#include <stdint.h>

long double __cdecl aim_acos(float v);
long double __cdecl aim_acos2(float v);
long double __cdecl aim_cos(float v);
long double __cdecl aim_cos2(float v);
long double __cdecl aim_sin(float v);
long double __cdecl aim_sin2(float v);
long double __cdecl aim_ceil(float v);
long double __cdecl aim_ceil2(float v);
long double __cdecl aim_floor(float v);
long double __cdecl aim_floor2(float v);
long double __cdecl aim_fabs(float v);
long double __cdecl vec_dot(const float *a, const float *b);
long double __cdecl vec_cross(const float *a, const float *b);
int32_t __cdecl aim_round(float v);
long double __cdecl vec_angle(const float *a, const float *b);
int32_t __cdecl aim_nearest_target(int32_t x, int32_t y);
void __cdecl aim_turrets(void);

#endif
