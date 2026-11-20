/* The game logic, 0x1008c755-0x1008cdf0: turret aiming: the nearest target unit, the angle
 * between two vectors in degrees (x87, as the original computes it) and wrappers around the
 * C library's acos, sin, cos, floor and ceil. */
#include "iso_core.h"
#include "vecscore.h"
#include "prefs.h"
#include "aim.h"

#define crt_acos ORIG(long double(__cdecl *)(double), 0x100f749a)
#define crt_cos ORIG(long double(__cdecl *)(double), 0x100f74ea)
#define crt_sin ORIG(long double(__cdecl *)(double), 0x100f74e0)
#define crt_ceil ORIG(long double(__cdecl *)(double), 0x100f7510)
#define crt_floor ORIG(long double(__cdecl *)(double), 0x100f7680)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)
#define turret_set_target ORIG(void(__cdecl *)(uint32_t, int, uint32_t), 0x1008c129)
#define turret_aim_at ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x1008c25f)
#define turret_set_a ORIG(void(__cdecl *)(void *, int), 0x100864f7)
#define turret_set_b ORIG(void(__cdecl *)(void *, int), 0x10086513)
#define turret_set_c ORIG(void(__cdecl *)(void *, int), 0x1008652f)
#define turret_set_d ORIG(void(__cdecl *)(void *, int), 0x10088827)
#define turret_set_state ORIG(void(__cdecl *)(void *, int), 0x10086567)

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* the C library wrappers: each takes a float and returns on the x87 stack */
long double __cdecl aim_acos(float v) { return crt_acos((double)v); }   /* 0x1008cac0 */
long double __cdecl aim_acos2(float v) { return aim_acos(v); }          /* 0x1008caa0 */
long double __cdecl aim_cos(float v) { return crt_cos((double)v); }     /* 0x1008cc00 */
long double __cdecl aim_cos2(float v) { return aim_cos(v); }            /* 0x1008cbe0 */
long double __cdecl aim_sin(float v) { return crt_sin((double)v); }     /* 0x1008cc50 */
long double __cdecl aim_sin2(float v) { return aim_sin(v); }            /* 0x1008cc30 */
long double __cdecl aim_ceil(float v) { return crt_ceil((double)v); }   /* 0x1008cd00 */
long double __cdecl aim_ceil2(float v) { return aim_ceil(v); }          /* 0x1008cce0 */
long double __cdecl aim_floor(float v) { return crt_floor((double)v); } /* 0x1008cd50 */
long double __cdecl aim_floor2(float v) { return aim_floor(v); }        /* 0x1008cd30 */
long double __cdecl aim_fabs(float v) { return fabs_x87(v); }           /* 0x1008cdd0 */

/* 0x1008caf0, 0x1008cb20: the dot and cross products */
long double __cdecl vec_dot(const float *a, const float *b)
{
    return (long double)b[1] * a[1] + (long double)b[0] * a[0];
}
long double __cdecl vec_cross(const float *a, const float *b)
{
    return (long double)b[1] * a[0] - (long double)a[1] * b[0];
}

/* 0x1008cc80: rounds to the nearest int (halves up) */
int32_t __cdecl aim_round(float v)
{
    long double frac = (long double)v - (long double)ftol(v);
    if (frac < 0.5L)
        return ftol(aim_floor2(v));
    return ftol(aim_ceil2(v));
}

/* 0x1008c990: the angle from a to b in degrees, 0-360 (counterclockwise) */
long double __cdecl vec_angle(const float *a, const float *b)
{
    float la = (float)vec_sqrt2((float)((long double)a[1] * a[1] + (long double)a[0] * a[0]));
    float lb = (float)vec_sqrt2((float)((long double)b[0] * b[0] + (long double)b[1] * b[1]));
    long double q = vec_dot(a, b) / ((long double)lb * la);
    float c = (float)q, r;
    if (!(q > -1.0L))
        r = 180.0f;
    else if (!(c < 1.0f))
        r = 0.0f;
    else
        r = (float)(aim_acos2(c) * 180.0f / G(float, 0x1011310c));
    if (vec_cross(a, b) < 0.0f)
        r = (float)(360.0f - (long double)r);
    return r;
}

/* 0x1008c879: the nearest unit of types 0x60-0x63 to (x, y), or 0 */
int32_t __cdecl aim_nearest_target(int32_t x, int32_t y)
{
    float best = 1e+20f;
    int32_t found = 0;
    int32_t n = I(int32_t, ISO_UNIT_COUNT);
    for (int32_t k = 0; k < n; k++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[k];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (!u || P(int32_t, u, 0xc) < 0x60 || P(int32_t, u, 0xc) > 0x63)
            continue;
        int32_t dx = x - P(int16_t, u, 0x52), dy = y - P(int16_t, u, 0x54);
        long double d = crt_sqrt((double)(dx * dx + dy * dy));
        float df = (float)d;
        if (d < best) {
            best = df;
            found = id;
        }
    }
    return found;
}

/* 0x1008c755: points every turret (types 0x30-0x37) at its nearest target and resets its
 * aiming state */
void __cdecl aim_turrets(void)
{
    int32_t n = I(int32_t, ISO_UNIT_COUNT);
    for (int32_t k = 0; k < n; k++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[k];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (!u || P(int32_t, u, 0xc) < 0x30 || P(int32_t, u, 0xc) > 0x37)
            continue;
        uint32_t t = (uint16_t)aim_nearest_target(P(int16_t, u, 0x52), P(int16_t, u, 0x54));
        turret_set_target((uint32_t)id, 6, t);
        turret_aim_at((uint32_t)id, t, 1);
        void *tur = P(void *, u, 0x39d);
        turret_set_a(tur, 0);
        turret_set_b(tur, 0);
        turret_set_c(tur, 0);
        turret_set_d(tur, 0);
        turret_set_state(tur, 9);
    }
}
