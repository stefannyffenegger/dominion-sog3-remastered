/* The game logic, 0x100dafc3-0x100db772: a unit going for its target: stopping, turning and
 * opening fire when in range, moving closer when not. */
#include "iso_core.h"
#include "patrol.h"
#include "sector.h"
#include "spot.h"
#include "unitattack.h"
#include "unitface.h"
#include "unitfacing.h"
#include "unitmove.h"
#include "unitorder.h"
#include "unitrange.h"
#include "waypoints.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define F(u) P(uint32_t, u, 0x10e)
#define S193(u) P(uint16_t, u, 0x193)
#define B195(u) P(uint8_t, u, 0x195)
#define INFO(u) P(uint8_t *, u, 0x32)
#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define ACT(a) unit_set_action(id, (a), 1, 0)

#define fn_100c6bd6 ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x100c6bd6)
#define fn_100d9ead ORIG(int32_t(__cdecl *)(uint32_t, int32_t *, int32_t), 0x100d9ead)
#define fn_100dcabd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define fn_100dd9d1                                                                          \
    ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t *, int32_t *, int32_t, int32_t), 0x100dd9d1)

static void suspend_path(uint32_t id, uint8_t *u, int32_t *result)
{
    if (P(uint8_t, u, 0x10e) & 1)
        return;
    int32_t r = unit_path_suspend(id, 1, 0);
    if (result)
        *result = r;
    F(u) |= 1;
    P(uint8_t, u, 0x177) = 0xff;
}

/* in range and stopped: turn to the target, then aim (state 0x14) */
static void turn_and_aim(uint32_t id, uint8_t *u)
{
    int32_t dir;
    B195(u) &= 0xfb;
    P(int16_t, u, 0x70) = P(int16_t, u, 0x52);
    P(int16_t, u, 0x72) = P(int16_t, u, 0x54);
    if (P(int32_t, INFO(u), 0xb4)) {
        if (!fn_100d9ead(id, &dir, 0))
            unit_face(id, dir, 0, 0);
        else if (P(int32_t, u, 0xc) >= 0x2c && P(int32_t, u, 0xc) <= 0x2f) {
            if (fn_100c6bd6(id, 2) == 1)
                ACT(0x14);
        } else {
            ACT(0x14);
        }
    } else if (!unit_faces_target(id, &dir)) {
        uint8_t pose = P(uint8_t, u, 0x147);
        if (!pose || pose == 0xb || pose == 0x22 || pose == 0x4b)
            unit_face(id, dir, 0, 0);
    } else {
        ACT(0x14);
    }
}

/* out of range: a new path towards the target */
static void move_closer(uint32_t id, uint8_t *u, uint32_t tid)
{
    if (!(P(uint16_t, u, 0x1cd) >> 3 & 1) && !P(uint8_t, u, 0x2a) && P(int16_t, u, 0x119) <= 0) {
        B195(u) &= 0xfd;
        B195(u) &= 0xfb;
    }
    if (B195(u) & 2) {
        int32_t d = P(int32_t, u, 0xc2);
        if (P(int32_t, u, 0x1ad) >= d || d - P(int32_t, u, 0x1ad) <= 0x100 ||
            P(int16_t, UNIT(tid & 0xffff), 0x113) <= 0)
            return;
    }
    if (!P(int32_t, INFO(u), 0x78) || (B195(u) & 4))
        return;
    int32_t suspended = 0;
    suspend_path(id, u, &suspended);
    if (!suspended && (B195(u) & 2))
        return;
    int32_t dir = 0, cell, moved;
    unit_faces_target(id, &dir);
    dir = G(int32_t, 0x10157350 + dir * 4);
    int32_t x = P(int16_t, UNIT(tid & 0xffff), 0x52), y = P(int16_t, UNIT(tid & 0xffff), 0x54);
    if (P(uint32_t, u, 0x3e) >> 23 & 1) {
        formation_spot(id, 3, 1, dir, &x, &y, &cell, 0, 1);
        P(int32_t, u, 0x1ad) = P(int32_t, u, 0xc2);
        moved = unit_move_order(id, (int16_t)x, (int16_t)y, 2, 0, 0, 1, 0, 0, S193(u) >> 10 & 1);
    } else {
        int32_t reach = P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),
                          P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + 4) / 64;
        fn_100dd9d1(id & 0xffff, P(int16_t, u, 0x52), P(int16_t, u, 0x54), &x, &y, reach, 3);
        P(int32_t, u, 0x1ad) = P(int32_t, u, 0xc2);
        moved = unit_move_order(id, (int16_t)x, (int16_t)y, 2, 0, 0, 1, 0, 0, !(S193(u) >> 10 & 1));
    }
    if (moved > 0)
        B195(u) |= 2;
}

/* 0x100dafc3: unit `id` goes for target `tid`. A unit with flag bit 12 drops its target and
 * goes idle. 0 when it does nothing this tick (no unit, not ours, a delay at +0x149), else 1 */
int32_t __cdecl unit_attack_move(uint32_t id, uint32_t tid)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (!u || (NET && !unit_is_local(id)))
        return 0;
    if (F(u) >> 12 & 1) {
        fn_100dcabd(id, 0);
        P(uint16_t, u, 0x18b) = 0;
        ACT(0x1e);
        return 0;
    }
    if (P(int32_t, u, 0x149))
        return 0;
    S193(u) &= 0xffef;
    P(uint16_t, u, 0x9a) = 0;
    if (!unit_in_range(id, tid) || (F(u) >> 19 & 1)) {
        move_closer(id, u, tid);
        return 1;
    }
    suspend_path(id, u, 0);
    B195(u) &= 0xfd;
    int flying = P(uint32_t, u, 0x3e) >> 23 & 1;
    if (!flying && P(int16_t, u, 0x113) > 0) {
        if (!(B195(u) & 1) && !(B195(u) & 4)) {
            B195(u) |= 4;
            patrol_order(id, (uint32_t)-1, 1, 1, 0, 0, 0, 1);
            if (P(int16_t, u, 0x119) > 0) {
                P(int16_t, u, 0x70) = P(int16_t, VEC(P(int16_t, u, 0x119)), 4);
                P(int16_t, u, 0x72) = P(int16_t, VEC(P(int16_t, u, 0x119)), 6);
            }
        }
        return 1;
    }
    if (flying) {
        if (P(int16_t, u, 0x119) <= 0)
            ACT(0x14);
        return 1;
    }
    if (!(B195(u) & 1) && !(F(u) >> 14 & 1))
        turn_and_aim(id, u);
    return 1;
}
