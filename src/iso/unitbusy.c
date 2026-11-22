/* The game logic, 0x100e8836-0x100e8d86: a unit being replaced by another in place. */
#include "iso_core.h"
#include "combat.h"
#include "healthbar.h"
#include "playerinfo.h"
#include "sector.h"
#include "unitbusy.h"
#include "unitorder.h"
#include "vecscore.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define OCC I(uint32_t *, 0x1934)
#define sprite_message ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48bc)
#define order_lock ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1009cc60)
#define cell_clear_unit ORIG(void(__cdecl *)(uint32_t *), 0x10070a64)
#define net_health ORIG(void(__cdecl *)(uint32_t), 0x1003d00d)
#define unit_show ORIG(void(__cdecl *)(uint8_t *, int32_t), 0x100f3e10)
#define net_order ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t), 0x10033842)

/* order 0x4d: the unit hides, leaves its cell and hands over to its partner (+0x9a) */
static int start_swap(uint32_t id, uint8_t *u)
{
    if (!UNIT(P(uint16_t, u, 0x9a)))
        return 0;
    P(uint8_t, u, 0x2d) = 1;
    P(uint32_t, u, 0x10e) |= 0x8000;
    sprite_message(I(uint32_t, 0x181e), P(uint32_t, u, 4), 1, ORIG_PTR(0x10157324));
    order_lock(id, 0x4d);
    int32_t c = P(int32_t, u, 0x56);
    OCC[c] &= 0xfffffff7;
    if ((OCC[c] >> 16 & 0xfff) == (id & 0xffff) && !(OCC[c] >> 12 & 1) && !(OCC[c] >> 14 & 1))
        cell_clear_unit(&OCC[c]);
    uint16_t old = P(uint16_t, u, 0x9a);
    if (!(P(uint32_t, UNIT(old), 0x3e) >> 26 & 1))
        P(uint16_t, u, 0x9a) = I(uint16_t, 0xd6f9 + P(uint8_t, UNIT(old), 0x14) * 2);
    else
        P(uint16_t, u, 0x9a) = old;
    if (P(uint32_t, UNIT(old), 0x3e) >> 26 & 1) {
        P(uint16_t, u, 0xd6) = (uint16_t)unit_max_health(u, 0);
        unit_health_bar(id, 0);
        if (NET && unit_is_local_u(u))
            net_health(id);
        unit_show(UNIT(P(uint16_t, u, 0x9a)), 1);
    }
    P(uint16_t, UNIT(old), 0x9a) = 0;
    uint8_t *partner = UNIT(P(uint16_t, u, 0x9a));
    if (partner) {
        P(uint16_t, partner, 0x9a) = (uint16_t)id;
        P(int16_t, u, 0x52) = P(int16_t, partner, 0x52);
        P(int16_t, u, 0x54) = P(int16_t, partner, 0x54);
        if (!NET || unit_is_local_u(u)) {
            unit_order(id, 0x26, P(int32_t, u, 0x1a3), 0);
            if (NET)
                net_order(id, 0x26, 0, 0);
        }
    } else {
        P(uint16_t, u, 0x9a) = 0;
        P(int32_t, u, 0x1a3) = 0;
    }
    return 1;
}

/* 0x100e8836: runs a unit's swap with its partner. While the countdown (+0x1a3) runs it
 * returns 1, the unit being busy. When it ends the unit comes back at its partner's place,
 * or dies when the partner is gone. Otherwise 0 */
int32_t __cdecl unit_busy(uint32_t id)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (!u)
        return 0;
    if (P(uint8_t, u, 0x148) == 0x4d && !start_swap(id, u))
        return 0;
    if (P(uint8_t, u, 0x147) != 0x4d)
        return 0;
    if (P(int32_t, u, 0x1a3)) {
        P(int32_t, u, 0x1a3)--;
        return 1;
    }
    if (!P(uint16_t, u, 0x9a) || !UNIT(P(uint16_t, u, 0x9a))) {
        unit_kill(id & 0xffff, 0x20, 1, 0);
        uint8_t *v = UNIT(id & 0xffff);
        if (!(P(uint32_t, v, 0x3e) >> 30 & 1)) {
            score_unit_killed(P(uint32_t, v, 0x10), P(uint8_t, v, 0x14), P(uint8_t, v, 0x14));
            P(uint32_t, v, 0x3e) |= 0x40000000;
        }
    } else {
        P(uint32_t, u, 0x10e) &= 0x7fffffff;
        P(uint16_t, u, 0x17d) = 0;
        P(uint16_t, u, 0x18b) = 0;
        P(uint8_t, u, 0x7c) = 0;
        unit_show(UNIT(P(uint16_t, u, 0x9a)), 0);
    }
    return 0;
}
