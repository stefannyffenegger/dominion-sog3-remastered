/* The game logic, 0x100dc1e7-0x100dc3ca: whether a unit goes on attacking its target. */
#include "iso_core.h"
#include "unitkeep.h"
#include "unitrange.h"

#define UNITS I(uint8_t **, ISO_UNITS)
#define S193(u) P(uint16_t, u, 0x193)
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))
#define unit_follow_near ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x100d99ca)

/* 0x100dc1e7: whether unit `id` keeps its target. Normally yes, except a hovercraft target
 * out of range when +0x193 bit 8 is set. While following (bit 2): yes with bit 5, else while
 * it stays within twice its leash of the guard point (bit 3) or of the followed unit */
int32_t __cdecl unit_keeps_target(uint32_t id)
{
    uint8_t *u = UNITS[id & 0xffff];
    uint8_t *t = UNITS[P(uint16_t, u, 0x17d)];
    if (!u || !t)
        return 0;
    if (!(S193(u) >> 2 & 1)) {
        if (P(int32_t, t, 0xc) >= 0x30 && P(int32_t, t, 0xc) <= 0x37 && (S193(u) >> 8 & 1) &&
            !unit_in_range(id, HW(u, 0x17d)))
            return 0;
        return 1;
    }
    if (S193(u) >> 5 & 1)
        return 1;
    if (S193(u) >> 3 & 1) {
        int32_t r = P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),
                      P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + 4) / 2;
        if (r <= 0xe1)
            r = 0xe1;
        return unit_near(id, r, P(int16_t, u, 0x187), P(int16_t, u, 0x189), 2) != 0;
    }
    return unit_follow_near(id, HW(u, 0x1ab), 2) != 0;
}
