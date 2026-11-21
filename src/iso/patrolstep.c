/* The game logic, 0x100cb2b2-0x100cb9d8: a unit setting off along its patrol path, and the
 * straight legs it plans along it (up to 15 at +0x1e9, 25 bytes each: start x, y, end x, y
 * as floats, a direction byte, the length and where it starts along the way). */
#include "iso_core.h"
#include "patrolstep.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define patrol_range ORIG(long double(__cdecl *)(uint8_t *), 0x100cb9d8)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define leg_direction ORIG(uint8_t(__cdecl *)(float *, float *), 0x100cbb54)
#define legs_start ORIG(void(__cdecl *)(uint8_t *), 0x100cbcea)
#define fabs_x87_wrap ORIG(long double(__cdecl *)(float), 0x1005d0f0)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)

/* whether a leg's end lies on the map (|x| and |y| within half the map's size) */
static int leg_end_on_map(uint8_t *leg)
{
    /* (written as the x87 compares: an unordered result counts as off the map) */
    if (!((long double)(I(int32_t, 0xd6f1) / 2) >= fabs_x87_wrap(P(float, leg, 8))))
        return 0;
    if (!((long double)(I(int32_t, 0xd6f5) / 2) >= fabs_x87_wrap(P(float, leg, 0xc))))
        return 0;
    return 1;
}

/* 0x100cb37a: plans the legs of unit `u`'s patrol along its waypoints, as far as its range
 * (0x100cb9d8, kept at +0x1d3) plus 80 reaches: each waypoint further than 0 away becomes a
 * leg; the last one ends part of the way to its waypoint. Legs ending off the map stop the
 * plan. With no leg, a unit on patrol gets one straight to its first waypoint; one without
 * a patrol leaves its patrol state (+0x1cd bit 0). All float values are kept as floats in
 * memory between the steps, as the original does. */
void __cdecl patrol_reset(uint8_t *u)
{
    P(float, u, 0x1d3) = (float)patrol_range(u);
    volatile float left = (float)((long double)P(float, u, 0x1d3) + (long double)G(float, 0x10113260));
    volatile float pos[2], next[2], vec[2], dist, along = 0.0f, rest;
    pos[0] = (float)P(int16_t, u, 0x52);
    pos[1] = (float)P(int16_t, u, 0x54);
    P(int16_t, u, 0x1e7) = 0;
    int16_t k = P(int16_t, u, 0x113);
    while (k > 0 && P(int16_t, u, 0x1e7) < 0xf && left > G(float, 0x10113264)) {
        uint8_t rec[0x12];
        memcpy(rec, VEC(k), 0x12);
        next[0] = (float)P(int16_t, rec, 4);
        next[1] = (float)P(int16_t, rec, 6);
        long double dy = (long double)pos[1] - (long double)P(int16_t, rec, 6);
        long double dx = (long double)pos[0] - (long double)next[0];
        long double d = crt_sqrt((double)(dy * ((long double)pos[1] - (long double)next[1]) + dx * dx));
        dist = (float)d;
        if (!(d > (long double)G(float, 0x10113264))) {
            /* the unit is at this waypoint already */
            pos[0] = next[0];
            pos[1] = next[1];
            k = P(int16_t, rec, 0xe);
            continue;
        }
        uint8_t dir = leg_direction((float *)pos, (float *)next);
        uint8_t *leg = LEG(u, P(int16_t, u, 0x1e7));
        if (!(left >= dist)) {
            /* the range ends on the way to this waypoint */
            leg_vector(dir, (float *)vec);
            P(float, leg, 0) = pos[0];
            P(float, leg, 4) = pos[1];
            P(float, leg, 8) = (float)((long double)vec[0] * left + pos[0]);
            P(float, leg, 0xc) = (float)((long double)vec[1] * left + pos[1]);
            if (!leg_end_on_map(leg))
                break;
            P(uint8_t, leg, 0x10) = dir;
            P(float, leg, 0x11) = left;
            P(float, leg, 0x15) = along;
            P(int16_t, u, 0x1e7)++;
            along = (float)((long double)left + along);
            left = (float)((long double)left - dist);
            break;
        }
        rest = (float)((long double)left - dist);
        P(float, leg, 0) = pos[0];
        P(float, leg, 4) = pos[1];
        P(float, leg, 8) = next[0];
        P(float, leg, 0xc) = next[1];
        if (!leg_end_on_map(leg))
            break;
        P(uint8_t, leg, 0x10) = dir;
        P(float, leg, 0x11) = dist;
        P(float, leg, 0x15) = along;
        P(int16_t, u, 0x1e7)++;
        along = (float)((long double)dist + along);
        left = rest;
        pos[0] = next[0];
        pos[1] = next[1];
        k = P(int16_t, rec, 0xe);
    }
    if (P(int16_t, u, 0x1e7) > 0) {
        legs_start(u);
        return;
    }
    if (P(int16_t, u, 0x113) <= 0) {
        P(uint16_t, u, 0x1cd) &= 0xfffe;
        return;
    }
    uint8_t rec[0x12];
    memcpy(rec, VEC(P(int16_t, u, 0x113)), 0x12);
    uint8_t *leg = LEG(u, 0);
    P(float, leg, 0) = (float)P(int16_t, u, 0x52);
    P(float, leg, 4) = (float)P(int16_t, u, 0x54);
    P(float, leg, 8) = (float)P(int16_t, rec, 4);
    P(float, leg, 0xc) = (float)P(int16_t, rec, 6);
    P(uint8_t, leg, 0x10) = P(uint8_t, rec, 9);
    int32_t ddx = P(int16_t, u, 0x52) - P(int16_t, rec, 4), ddy = P(int16_t, u, 0x54) - P(int16_t, rec, 6);
    volatile float len = (float)crt_sqrt((double)(ddx * ddx + ddy * ddy));
    P(float, leg, 0x11) = len;
    P(float, leg, 0x15) = 0.0f;
    P(int16_t, u, 0x1e7) = 1;
    legs_start(u);
}


/* 0x100cb2b2: unit `unit` has set off (+0x10e bit 2 cleared); a unit on patrol (+0x113)
 * starts its patrol state over (+0x1cd: bit 0 set, bits 1, 3, 4, 7, 8 cleared; +0x360/362
 * 0x8300) and 0x100cb37a. The original indexes the unit table with the whole argument. */
void __cdecl unit_moved(uint32_t unit)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit];
    if (!u)
        return;
    P(uint32_t, u, 0x10e) &= 0xfffffffb;
    if (P(int16_t, u, 0x113) <= 0)
        return;
    P(uint16_t, u, 0x1cd) |= 1;
    P(uint16_t, u, 0x1cd) &= 0xffef;
    P(uint16_t, u, 0x1cd) &= 0xfff7;
    P(uint16_t, u, 0x1cd) &= 0xfffd;
    P(uint16_t, u, 0x360) = 0x8300;
    P(uint16_t, u, 0x362) = 0x8300;
    P(uint16_t, u, 0x1cd) &= 0xff7f;
    P(uint16_t, u, 0x1cd) &= 0xfeff;
    patrol_reset(u);
}
