/* The game logic, 0x100cb9d8-0x100cbcea: the patrol legs' range, direction and unit
 * vectors. */
#include "iso_core.h"
#include "patrollegs.h"

#define fn_100e5a4d ORIG(void(__cdecl *)(uint32_t, int, int32_t *, int32_t *), 0x100e5a4d)
#define vec_sqrt2 ORIG(long double(__cdecl *)(float), 0x1007e780)
#define vec_direction ORIG(void(__cdecl *)(const float *, const float *, float *), 0x1007e720)
#define vec_angle ORIG(long double(__cdecl *)(const float *, const float *), 0x1008c990)

/* 0x100cb9d8: how far unit `u` patrols: the length of the vector 0x100e5a4d gives for its
 * patrol (+0x1cb, passed with the record pointer's upper half above it) */
long double __cdecl patrol_range(uint8_t *u)
{
    int32_t a, b;
    fn_100e5a4d(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb), 0, &a, &b);
    float sq = (float)(b * b + a * a);
    return vec_sqrt2(sq);
}

/* 0x100cba36: the unit vector of direction `dir` (0 east, then in eighths of a turn on the
 * isometric map: the diagonals are (0.894, 0.447)) */
void __cdecl leg_vector(uint32_t dir, float *out)
{
    static const uint32_t v[8][2] = {
        {0x3f800000, 0},          {0x3f64f92e, 0x3ee4f953}, {0, 0x3f800000},          {0xbf64f92e, 0x3ee4f953},
        {0xbf800000, 0},          {0xbf64f92e, 0xbee4f953}, {0, 0xbf800000},          {0x3f64f92e, 0xbee4f953},
    };
    uint8_t d = (uint8_t)dir;
    if (d > 7)
        return;
    ((uint32_t *)out)[0] = v[d][0];
    ((uint32_t *)out)[1] = v[d][1];
}

/* 0x100cbb54: the direction (1-7, else 0) from point `a` to point `b`, by the angle of the
 * vector between them against (1, 0) in the sectors of table 0x10113280. The first test
 * uses the angle as the x87 has it, the others the angle rounded to a float, as in the
 * original. */
uint8_t __cdecl leg_direction(const float *a, const float *b)
{
    float v[2];
    vec_direction(a, b, v);
    long double angle = vec_angle((const float *)ORIG_PTR(0x101590e8), v);
    volatile float s = (float)angle;
    const float *c = (const float *)ORIG_PTR(0x10113280);
    if (angle > (long double)c[0] && !(s > c[1]))
        return 1;
    for (int k = 2; k <= 7; k++)
        if (s > c[k - 1] && !(s > c[k]))
            return (uint8_t)k;
    return 0;
}
