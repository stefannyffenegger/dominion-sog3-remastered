/* The game logic, 0x1007e720-0x1007eb13: 2D float vectors (x87, as the original computes
 * them) and the players' score counters (per player at 0x1461 bytes: counts by unit type
 * and in total). */
#include "iso_core.h"
#include "vecscore.h"

#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)

/* 0x1007e7a0, 0x1007e780: the square root of a float (the C library's, at the x87's
 * precision) */
long double __cdecl vec_sqrt(float v) { return crt_sqrt((double)v); }
long double __cdecl vec_sqrt2(float v) { return vec_sqrt(v); }

/* 0x1007e830: c = b - a */
void __cdecl vec_sub(const float *a, const float *b, float *c)
{
    c[0] = (float)((long double)b[0] - a[0]);
    c[1] = (float)((long double)b[1] - a[1]);
}

/* 0x1007e7d0: normalises v; x is divided by the length at full precision, y by the length
 * rounded to float, as the original's x87 code did */
void __cdecl vec_normalize(float *v)
{
    float sq = (float)((long double)v[1] * v[1] + (long double)v[0] * v[0]);
    long double len = vec_sqrt2(sq);
    float lenf = (float)len;
    v[0] = (float)((long double)v[0] / len);
    v[1] = (float)((long double)v[1] / lenf);
}

/* 0x1007e720: c = the unit vector from a to b (its length is computed and dropped) */
void __cdecl vec_direction(const float *a, const float *b, float *c)
{
    vec_sub(a, b, c);
    vec_normalize(c);
    float sq = (float)((long double)c[1] * c[1] + (long double)c[0] * c[0]);
    volatile float len = (float)vec_sqrt2(sq);
    (void)len;
}

/* 0x1007e870: a unit of `type` destroyed: counted as a loss of `owner` and, if someone
 * else's doing, a kill of `killer` (per type at +0x2e59/+0x2ead, totals +0x2eab/+0x2eff) */
void __cdecl score_unit_killed(uint32_t type, uint8_t owner, uint8_t killer)
{
    uint32_t t = (type & 0xfffffffd) >> 1;
    if (killer != owner) {
        I(int16_t, killer * ISO_PLAYER_SIZE + t + 0x2ead)++;
        I(int16_t, killer * ISO_PLAYER_SIZE + 0x2eff)++;
    }
    I(int16_t, owner * ISO_PLAYER_SIZE + t + 0x2e59)++;
    I(int16_t, owner * ISO_PLAYER_SIZE + 0x2eab)++;
}

/* 0x1007e943: a unit of `type` built by `player` (per type +0x2e05, total +0x2e57) */
void __cdecl score_unit_built(int32_t player, uint32_t type)
{
    int32_t t = (int32_t)(type & 0xfffffffd) >> 1;
    I(int16_t, t + player * ISO_PLAYER_SIZE + 0x2e05)++;
    I(int16_t, player * ISO_PLAYER_SIZE + 0x2e57)++;
}

/* 0x1007e9ff, 0x1007eadf: adds to a player's men (+0x2fb1) or materials (+0x2fb5) total */
void __cdecl score_add_men(int32_t player, int32_t n) { I(int32_t, player * ISO_PLAYER_SIZE + 0x2fb1) += n; }
void __cdecl score_add_materials(int32_t player, int32_t n) { I(int32_t, player * ISO_PLAYER_SIZE + 0x2fb5) += n; }
