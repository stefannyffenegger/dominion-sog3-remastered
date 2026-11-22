/* The game logic, 0x100dcabd-0x100dcda0: changing a unit's target. */
#include "iso_core.h"
#include "sector.h"
#include "unitsettarget.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define net_429be ORIG(void(__cdecl *)(uint32_t), 0x100429be)
#define net_40706 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x10040706)
#define fn_10071766 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10071766)

/* whether the new target is the local player's own carrier (+0xe0 seats) */
static int own_carrier(uint32_t target)
{
    uint8_t *n = (target & 0xffff) ? UNIT(target & 0xffff) : 0;
    return n && (int32_t)P(uint8_t, n, 0x14) == I(int32_t, 0x1b65) && P(int32_t, P(uint8_t *, n, 0x32), 0xe0);
}

/* 0x100dcabd: unit `id` changes its target (+0x17d). Leaving a friendly unit with seats
 * (+0xfa) gives up the place there (0x10071766, sent in a network game unless the new
 * target is the local player's carrier); otherwise the old target forgets this unit
 * (+0x9a). Nothing in state 0x10 or when the target stays. */
void __cdecl unit_set_target(uint32_t id, uint32_t target)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (!u || P(uint16_t, u, 0x17d) == (target & 0xffff) || P(uint8_t, u, 0x196) == 0x10)
        return;
    if (P(uint16_t, u, 0x17d)) {
        debug_print(0, (const char *)ORIG_PTR(0x10159428), I(int32_t, 0x17dd), id & 0xffff,
                    (uint32_t)P(uint16_t, u, 0x17d), target & 0xffff);
        if (NET && unit_is_local_u(u) && !(target & 0xffff))
            net_429be(id);
        uint8_t *t = UNIT(P(uint16_t, u, 0x17d));
        if (!t) {
            P(uint16_t, u, 0x17d) = 0;
            return;
        }
        if (!(P(uint8_t, u, 0x10e) & 1) && P(int32_t, P(uint8_t *, u, 0x32), 0x74) != 5) {
            uint32_t mine = P(uint8_t, u, 0x14), its = P(uint8_t, t, 0x14);
            int friendly = its == mine || (I(uint8_t, 0x1ba0 + mine * 0x1461) & (1u << (its & 0x1f)));
            if (friendly && P(uint16_t, t, 0xfa)) {
                if (NET && unit_is_local_u(u) && !own_carrier(target))
                    net_40706(id, HW(u, 0x17d), 4);
                fn_10071766(HW(u, 0x17d), id);
            } else if (P(uint16_t, t, 0x9a) == (id & 0xffff)) {
                P(uint16_t, t, 0x9a) = 0;
            }
        }
    }
    P(uint16_t, u, 0x17d) = (uint16_t)target;
}
