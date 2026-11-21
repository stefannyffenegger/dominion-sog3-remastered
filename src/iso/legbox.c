/* The game logic, 0x100cbcea-0x100cc145: finishing a patrol plan: the legs are trimmed at
 * both ends, their start points along the way worked out, and the box around them kept
 * (+0x1d7-+0x1e3). Legs are 25 bytes each at +0x1e9: start x, y, end x, y (floats), the
 * direction, the length and where the leg starts along the way. All floats go through
 * memory as floats between the steps, as in the original. */
#include "iso_core.h"
#include "legbox.h"

#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)

static long double leg_length(uint8_t *leg)
{
    long double dy = (long double)P(float, leg, 4) - P(float, leg, 0xc);
    long double dy2 = (long double)P(float, leg, 4) - P(float, leg, 0xc);
    long double dx = (long double)P(float, leg, 0) - P(float, leg, 8);
    long double dx2 = (long double)P(float, leg, 0) - P(float, leg, 8);
    return crt_sqrt((double)(dy * dy2 + dx * dx2));
}

/* 0x100cc0b9: how far one step goes in direction `dir`: 64 east and west, 32 north and
 * south, 36 on the diagonals (0 for no direction) */
long double __cdecl leg_step(uint32_t dir)
{
    switch ((uint8_t)dir) {
    case 0:
    case 4:
        return 64.0f;
    case 2:
    case 6:
        return 32.0f;
    case 1:
    case 3:
    case 5:
    case 7:
        return 36.0f;
    default:
        return 0.0f;
    }
}

/* 0x100cbcea: finishes unit `u`'s patrol plan: the first leg starts a quarter step back,
 * the last ends 5 (soldiers +0x28 == 1: 3) further on, the lengths and starts along the
 * way are worked out again, and the box around the legs' ends and the first start is
 * kept: half its size (+0x1d7, +0x1db) and its middle (+0x1df, +0x1e3) */
void __cdecl legs_start(uint8_t *u)
{
    volatile float vec[2], x, y, len, step, along, minx, maxx, miny, maxy, w, h;
    leg_vector(P(uint8_t, u, 0x1f9), (float *)vec);
    x = P(float, u, 0x1e9);
    y = P(float, u, 0x1ed);
    long double r = leg_step(P(uint8_t, u, 0x1f9)) * (long double)G(float, 0x101132ac);
    step = (float)r;
    x = (float)(-(r * vec[0]) + x);
    y = (float)(-((long double)vec[1] * step) + y);
    P(float, u, 0x1e9) = x;
    P(float, u, 0x1ed) = y;
    len = (float)leg_length(LEG(u, 0));
    P(float, u, 0x1fa) = len;

    step = 5.0f;
    if (P(uint16_t, u, 0x28) == 1)
        step = 3.0f;
    uint8_t *last = LEG(u, P(int16_t, u, 0x1e7) - 1);
    leg_vector(P(uint8_t, last, 0x10), (float *)vec);
    x = P(float, last, 8);
    y = P(float, last, 0xc);
    x = (float)((long double)vec[0] * step + x);
    y = (float)((long double)vec[1] * step + y);
    P(float, last, 8) = x;
    P(float, last, 0xc) = y;
    len = (float)leg_length(last);
    P(float, last, 0x11) = len;

    along = 0.0f;
    for (int16_t i = 1; i < P(int16_t, u, 0x1e7); i++) {
        along = (float)((long double)P(float, LEG(u, i - 1), 0x11) + along);
        P(float, LEG(u, i), 0x15) = along;
    }

    minx = maxx = P(float, u, 0x1e9);
    miny = maxy = P(float, u, 0x1ed);
    for (int16_t i = 0; i < P(int16_t, u, 0x1e7); i++) {
        float ex = P(float, LEG(u, i), 8), ey = P(float, LEG(u, i), 0xc);
        /* (as the x87 compares: unordered counts as below for the minimum) */
        if (!(ex >= minx))
            minx = ex;
        if (ex > maxx)
            maxx = ex;
        if (!(ey >= miny))
            miny = ey;
        if (ey > maxy)
            maxy = ey;
    }
    w = (float)((long double)maxx - minx);
    h = (float)((long double)maxy - miny);
    P(float, u, 0x1d7) = (float)((long double)w * G(float, 0x101132b8));
    P(float, u, 0x1db) = (float)((long double)h * G(float, 0x101132b8));
    P(float, u, 0x1df) = (float)((long double)w * G(float, 0x101132b8) + minx);
    P(float, u, 0x1e3) = (float)((long double)h * G(float, 0x101132b8) + miny);
}
