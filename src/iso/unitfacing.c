/* The game logic, 0x100d9b26-0x100d9ead: a target within weapon range, and whether a unit
 * faces its target. */
#include "iso_core.h"
#include "unitfacing.h"

#define UNIT(id) (I(uint8_t **, ISO_UNITS)[(id) & 0xffff])
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)
#define get_angle ORIG(int32_t(__stdcall *)(int32_t, int32_t), 0x100f4a90)

/* 0x100d9b26: whether unit `tid` is within unit `id`'s weapon range (500 without one), with
 * y counting double. The distance goes to +0xc2 */
int32_t __cdecl unit_weapon_reach(uint32_t id, uint32_t tid)
{
    uint8_t *u = UNIT(id);
    if (!u)
        return 0;
    int32_t range = P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),
                      P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + 4);
    uint8_t *t = UNIT(tid);
    int32_t dx = P(int16_t, u, 0x52) - P(int16_t, t, 0x52);
    int32_t dy = (P(int16_t, u, 0x54) - P(int16_t, t, 0x54)) * 2;
    int32_t sum = dx * dx + dy * dy;
    P(int32_t, u, 0xc2) = (int32_t)crt_sqrt((double)sum);
    return P(int32_t, u, 0xc2) <= (range > 0 ? range : 0x1f4);
}

/* 0x100d9cb5: the direction (0-7) from unit `id` to unit `tid` into `dir`, from the angle in
 * tenths of a degree; whether the unit already faces it (+0x79) */
int32_t __cdecl unit_faces_unit(uint32_t id, uint32_t tid, int32_t *dir)
{
    uint8_t *u = UNIT(id), *t = UNIT(tid);
    if (!u || !t)
        return 0;
    int32_t a = get_angle(P(int16_t, t, 0x52) - P(int16_t, u, 0x52), P(int16_t, t, 0x54) - P(int16_t, u, 0x54));
    if (a >= 0xd7a || a < 0x96)
        *dir = 2;
    else if (a < 0x2ee)
        *dir = 1;
    else if (a < 0x41a)
        *dir = 0;
    else if (a < 0x672)
        *dir = 7;
    else if (a < 0x79e)
        *dir = 6;
    else if (a < 0x9f6)
        *dir = 5;
    else if (a < 0xb22)
        *dir = 4;
    else
        *dir = 3;
    return P(uint8_t, u, 0x79) == *dir;
}

/* 0x100d9c57: whether unit `id` faces its target (+0x17d) */
int32_t __cdecl unit_faces_target(uint32_t id, int32_t *dir)
{
    uint8_t *u = UNIT(id);
    if (!u)
        return 0;
    return unit_faces_unit(id, ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x17d), dir);
}
