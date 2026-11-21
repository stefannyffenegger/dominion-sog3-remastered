/* The game logic, 0x100d979f-0x100d99ca: distances to a point or a unit. Each stores the
 * distance in the unit's +0xc2. */
#include "iso_core.h"
#include "unitrange.h"

#define UNIT(id) (I(uint8_t **, ISO_UNITS)[(id) & 0xffff])
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)
#define WEAPON_RANGE(u)                                                                      \
    P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),                                        \
      P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + 4)

/* the root of dx*dx + dy*dy, truncated (the original's _ftol), kept in +0xc2 */
static int32_t store_distance(uint8_t *u, int32_t dx, int32_t dy)
{
    int32_t sum = dy * dy + dx * dx;
    int32_t d = (int32_t)crt_sqrt((double)sum);
    P(int32_t, u, 0xc2) = d;
    return d;
}

/* 0x100d979f: whether unit `id` is within `range` times `times` of (x, y) */
int32_t __cdecl unit_near(uint32_t id, int32_t range, int32_t x, int32_t y, uint32_t times)
{
    uint8_t *u = UNIT(id);
    if (!u)
        return 0;
    int32_t d = store_distance(u, P(int16_t, u, 0x52) - x, P(int16_t, u, 0x54) - y);
    return (int32_t)(times & 0xffff) * range >= d;
}

/* 0x100d9855: whether unit `tid` is in range of unit `id`'s weapon (500 without a range, 24
 * more for attack type 1), with y counting double on the isometric map. With +0x3e bit 23
 * the unit only reaches 150 */
int32_t __cdecl unit_in_range(uint32_t id, uint32_t tid)
{
    uint8_t *u = UNIT(id), *t = UNIT(tid);
    if (!u || !t)
        return 0;
    int32_t range = WEAPON_RANGE(u);
    if (range <= 0)
        range = 0x1f4;
    if (P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 1)
        range += 0x18;
    int32_t d = store_distance(u, P(int16_t, u, 0x52) - P(int16_t, t, 0x52),
                               (P(int16_t, u, 0x54) - P(int16_t, t, 0x54)) * 2);
    if (P(uint32_t, u, 0x3e) >> 23 & 1)
        return d < 0x96;
    return d <= range;
}
