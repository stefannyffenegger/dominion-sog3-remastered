/* The game logic, 0x100d2319-0x100d2552: whether a unit that has just got a path has to
 * wait before setting off. */
#include "iso_core.h"
#include "unitblocked.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define S(u) P(uint16_t, u, 0x1cd)
#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)
#define patrol_reset ORIG(void(__cdecl *)(uint8_t *), 0x100cb37a)
#define ai_patrol_clear ORIG(void(__cdecl *)(uint8_t *), 0x100d01d7)
#define patrol_blocked ORIG(int32_t(__cdecl *)(uint8_t *), 0x100ce277)
#define soldier_patrol_check ORIG(int32_t(__cdecl *)(uint8_t *), 0x100d0e81)
#define patrols_collide ORIG(int32_t(__cdecl *)(uint8_t *), 0x100d0270)
#define unit_wait ORIG(void(__cdecl *)(uint32_t), 0x100e97ca)

/* 0x100d2319: unit `unit` plans its legs along its new path. 0 when it is already at the
 * path's only point. A soldier is held back when an enemy wall post stands in its way
 * (unless +0x1cd bit 9) or its way ends where it cannot stand; a vehicle when its patrol
 * meets another's or a post stands in its way. A held unit stops at its place (+0x1cd bit 3,
 * +0x360/+0x362) and waits (0x100e97ca); 1 then. */
int32_t __cdecl unit_blocked(uint32_t unit)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[(int16_t)unit];
    if (!u)
        return 0;
    uint8_t rec[0x12];
    memcpy(rec, VEC(P(int16_t, u, 0x113)), 0x12);
    if (P(int16_t, rec, 0xe) <= 0 && crt_abs(P(int16_t, u, 0x52) - P(int16_t, rec, 4)) <= 2 &&
        crt_abs(P(int16_t, u, 0x54) - P(int16_t, rec, 6)) <= 2)
        return 0;
    S(u) |= 1;
    patrol_reset(u);
    ai_patrol_clear(u);
    int32_t held = 0;
    uint32_t kind = P(uint16_t, u, 0x28);
    if (kind == 1) {
        if ((!(S(u) >> 9 & 1) && patrol_blocked(u) == 1) || soldier_patrol_check(u))
            held = 1;
    } else if (kind == 3) {
        S(u) &= 0xfffe;
        S(u) |= 8;
        if (patrols_collide(u) == 1 || (!(S(u) >> 9 & 1) && patrol_blocked(u) == 1))
            held = 1;
    }
    if (held == 1) {
        S(u) |= 8;
        S(u) &= 0xfffe;
        S(u) &= 0xffef;
        P(int16_t, u, 0x360) = P(int16_t, u, 0x52);
        P(int16_t, u, 0x362) = P(int16_t, u, 0x54);
        unit_wait(unit);
    }
    return held;
}
