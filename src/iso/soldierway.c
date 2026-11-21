/* The game logic, 0x100d01d7-0x100d0e81: a computer unit's patrol fields, and whether a
 * stuck soldier's way is clear. */
#include "iso_core.h"
#include "soldierway.h"

/* 0x100d01d7: clears a unit's computer patrol (+0x1cd bit 5, +0x382-+0x388) */
void __cdecl ai_patrol_clear(uint8_t *u)
{
    P(uint16_t, u, 0x1cd) &= 0xffdf;
    P(int16_t, u, 0x382) = 0;
    P(int16_t, u, 0x384) = 0;
    P(int16_t, u, 0x386) = -1;
    P(int16_t, u, 0x388) = -1;
}

#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define sectors_near ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *), 0x100ce197)
#define aim_fabs ORIG(long double(__cdecl *)(float), 0x1008cdd0)
#define legs_meet                                                                            \
    ORIG(int32_t(__cdecl *)(uint8_t *, float *, float *, float, int32_t, uint8_t *, float *, float *, float, int32_t), \
         0x100d0622)

/* the length of the whole patrol: where its last leg starts plus its length */
static float patrol_length(uint8_t *u)
{
    uint8_t *last = LEG(u, P(int16_t, u, 0x1e7) - 1);
    return (float)((long double)P(float, last, 0x15) + P(float, last, 0x11));
}

/* 0x100d0270: whether the patrol of unit `u` meets that of another patrolling unit (not a
 * soldier) near it whose patrol box (+0x1d7-+0x1e3, grown by 8) overlaps its own: at once
 * when that unit stands at its stop (+0x1cd bit 3), else when two of their legs meet
 * (0x100d0622) */
int32_t __cdecl patrols_collide(uint8_t *u)
{
    if (!P(int16_t, u, 0x1e7))
        return 0;
    volatile float hw, hh, ow, oh, dx, dy, ulen, olen;
    hw = (float)((long double)P(float, u, 0x1d7) + G(float, 0x101132cc));
    hh = (float)((long double)P(float, u, 0x1db) + G(float, 0x101132cc));
    int32_t r = 0;
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        uint8_t *o = I(uint8_t **, ISO_UNITS)[I(int32_t *, 0xd67a)[i]];
        if (!o || o == u || !(P(uint16_t, o, 0x1cd) & 1) || !P(int16_t, o, 0x1e7) || P(uint16_t, o, 0x28) == 1)
            continue;
        if (!sectors_near(u, o))
            continue;
        ow = (float)((long double)P(float, o, 0x1d7) + G(float, 0x101132cc));
        oh = (float)((long double)P(float, o, 0x1db) + G(float, 0x101132cc));
        dx = (float)aim_fabs((float)((long double)P(float, u, 0x1df) - P(float, o, 0x1df)));
        dy = (float)aim_fabs((float)((long double)P(float, u, 0x1e3) - P(float, o, 0x1e3)));
        /* (as the x87 compares: an unordered result skips the unit) */
        if (!((long double)ow + hw >= (long double)dx))
            continue;
        if (!((long double)oh + hh >= (long double)dy))
            continue;
        if (P(uint16_t, o, 0x1cd) >> 3 & 1)
            return 1;
        if (!P(int16_t, u, 0x1e7) || !P(int16_t, o, 0x1e7))
            return 0;
        ulen = patrol_length(u);
        olen = patrol_length(o);
        for (int32_t j = 0; j < P(int16_t, u, 0x1e7); j++) {
            float a0[2] = {P(float, LEG(u, j), 0), P(float, LEG(u, j), 4)};
            float a1[2] = {P(float, LEG(u, j), 8), P(float, LEG(u, j), 0xc)};
            for (int32_t k = 0; k < P(int16_t, o, 0x1e7); k++) {
                float b0[2] = {P(float, LEG(o, k), 0), P(float, LEG(o, k), 4)};
                float b1[2] = {P(float, LEG(o, k), 8), P(float, LEG(o, k), 0xc)};
                r = legs_meet(u, a0, a1, ulen, j, o, b0, b1, olen, k);
                if (r == 1)
                    return 1;
            }
        }
    }
    return r;
}
