/* The game logic, 0x100f223a-0x100f306f: one tick of a wall post: its poses, its gun and
 * its repair. */
#include "iso_core.h"
#include "combat.h"
#include "intfwave.h"
#include "objects.h"
#include "playerinfo.h"
#include "posttick.h"
#include "power.h"
#include "sound.h"
#include "structshow.h"
#include "structtick.h"
#include "unitdelete.h"
#include "unitorder.h"
#include "unitsettarget.h"
#include "vecscore.h"
#include "walls.h"

#define INFO(u) P(uint8_t *, u, 0x32)
#define TYPE(u) P(int32_t, u, 0xc)
#define FLAGS3E(u) P(uint32_t, u, 0x3e)
#define FRAME(u) P(uint16_t, u, 0x145)
#define POSE(u) P(uint8_t, u, 0x147)
#define NEXT_POSE(u) P(uint8_t, u, 0x148)
#define TIMER(u) P(uint32_t, u, 0x1a7)
#define NOW I(uint32_t, 0x17dd)
#define HB14(u) (((uint32_t)(uintptr_t)(u) & 0xffffff00) | P(uint8_t, u, 0x14))
#define OBJ_FLAGS(t) \
    P(uint32_t, P(uint8_t *, I(uint8_t *, 0x1930), 0), P(int32_t, I(uint8_t *, 0x1930), 0x2c) * (t) * 4 + 0xc)

#define res_get ORIG(uint32_t *(__cdecl *)(uint32_t, uint32_t), 0x100f48a4)
#define sprite_process ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100f48da)
#define delete_object ORIG(void(__stdcall *)(uint8_t *, int32_t), 0x100adbde)
#define fn_1006a5c1 ORIG(void(__cdecl *)(uint32_t), 0x1006a5c1)
#define fn_100f308b ORIG(void(__cdecl *)(uint32_t), 0x100f308b)
#define fn_100b1ce0 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100b1ce0)
#define fn_1006c6f6 ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x1006c6f6)
#define fn_100eb610 ORIG(void(__cdecl *)(uint32_t), 0x100eb610)
#define fn_100a5240 ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x100a5240)
#define fn_100d9ead ORIG(int32_t(__cdecl *)(uint32_t, int32_t *, int32_t), 0x100d9ead)
#define fn_100d9fa9 ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t), 0x100d9fa9)
#define fn_100ee418 ORIG(void(__cdecl *)(uint32_t), 0x100ee418)

/* pose 1: standing, with its top object added when one is due */
static void pose_running(uint32_t id, uint8_t *u)
{
    if (P(uint8_t, u, 0x2d) == 4)
        P(uint8_t, u, 0x2d) = 2;
    FRAME(u) = (uint16_t)P(uint32_t, INFO(u), 0x90);
    int32_t top;
    if (!I(int32_t, 0x11c) || !(FLAGS3E(u) >> 9 & 1) || !(top = P(int32_t, INFO(u), 0xdc)))
        return;
    P(int32_t, u, 0x1c) = IsoAddObjectC(g_iso, top, P(int16_t, u, 0x52) - 0x20, P(int16_t, u, 0x54) - 0x10,
                                        OBJ_FLAGS(top), -1, P(int16_t, u, 0x66) + 1);
    FLAGS3E(u) &= 0xfffffdff;
    uint32_t s = res_get(I(uint32_t, 0x180e), P(uint32_t, I(uint8_t *, 0x192c), P(int32_t, u, 0x1c) * 0x12))[1];
    if (s)
        sprite_process(I(uint32_t, 0x181e), s, 1);
    if (I(int32_t, 0x175))
        fn_100b1ce0(s, I(uint32_t, P(uint8_t, u, 0x14) * ISO_PLAYER_SIZE + 0x1ba2), TYPE(u), 0, 0);
    structure_show(id, P(int16_t, u, 0x42) ? 0 : 1);
}

/* pose 0: being built */
static void pose_building(uint8_t *u)
{
    if (P(uint32_t, INFO(u), 0xac) + TIMER(u) >= NOW)
        return;
    TIMER(u) = NOW;
    FRAME(u)++;
    P(uint8_t, u, 0x2d) = 2;
    if (P(int32_t, INFO(u), 0x90) > (int32_t)FRAME(u))
        return;
    P(uint16_t, u, 0xdc) = (uint16_t)P(uint32_t, INFO(u), 0x28);
    NEXT_POSE(u) = 1;
    P(uint8_t, u, 0x2d) = 4;
    if (P(int32_t, INFO(u), 0xdc))
        FLAGS3E(u) |= 0x200;
    else
        FLAGS3E(u) &= 0xfffffdff;
}

/* poses 2 and 3: the frame of a damage stage */
static void pose_stage(uint32_t id, uint8_t *u, uint32_t off, int32_t which)
{
    if (P(uint32_t, INFO(u), off) == FRAME(u))
        return;
    FRAME(u) = (uint16_t)P(uint32_t, INFO(u), off);
    P(uint8_t, u, 0x2d) = 2;
    structure_show(id, 2);
    fn_1006c6f6(id, which, 0);
}

/* poses 0x4c and 0x4f: the countdown, then the post is deleted. Returns 1 when it is */
static int pose_gone(uint32_t id, uint8_t *u)
{
    if (TIMER(u)) {
        TIMER(u)--;
        return 0;
    }
    if (P(int32_t, u, 0x1c) != -1) {
        delete_object(g_iso, P(int32_t, u, 0x1c));
        P(int32_t, u, 0x1c) = -1;
        FLAGS3E(u) &= 0xfffffdff;
    }
    if (POSE(u) == 0x4f) {
        player_announce(P(uint8_t, u, 0x14), 0x21, 1);
        IsoDeleteUnitC(g_iso, id & 0xffff, 0);
    } else {
        player_announce(P(uint8_t, u, 0x14), 0x2d, 1);
        IsoDeleteUnitC(g_iso, id & 0xffff, 1);
    }
    power_mark_all(1);
    return 1;
}

/* poses 0x14 and 0x16: dying or sold, frame by frame. Returns 1 at the last frame */
static int pose_going(uint32_t id, uint8_t *u, int starting, uint32_t old_pose)
{
    if (starting) {
        TIMER(u) = NOW;
        int32_t full = P(int32_t, INFO(u), 0x90);
        FRAME(u) = (uint16_t)(old_pose == 2 ? full - 1 : old_pose == 3 ? full - 3 : full);
        structure_show(id, 2);
        P(uint8_t, u, 0x2d) = 2;
        return 0;
    }
    if (P(uint32_t, INFO(u), 0xac) + TIMER(u) >= NOW)
        return 0;
    TIMER(u) = NOW;
    FRAME(u)--;
    P(uint8_t, u, 0x2d) = 2;
    if (POSE(u) == 0x14)
        fn_100a5240(id & 0xffff, 0, HB14(u));
    if (P(uint32_t, INFO(u), 0x8c) != FRAME(u))
        return 0;
    NEXT_POSE(u) = 0x4f;
    TIMER(u) = 0;
    if (POSE(u) == 0x14) {
        fn_100a5240(id & 0xffff, 1, HB14(u));
        if (!(FLAGS3E(u) >> 30 & 1)) {
            score_unit_killed(P(uint32_t, u, 0x10), P(uint8_t, u, 0x14), P(uint8_t, u, 0x14));
            FLAGS3E(u) |= 0x40000000;
        }
    }
    return 1;
}

/* a post with a gun (+0xb4) turns it towards its target, wrapping at 48 directions; types
 * 0x44-0x47 aiming at types 0x30-0x37 pass a flag to the turn */
static void turn_gun(uint32_t id, uint8_t *u)
{
    if (!P(int32_t, INFO(u), 0xb4))
        return;
    uint8_t *target = I(uint8_t **, ISO_UNITS)[P(uint16_t, u, 0x17d)];
    if (!target)
        return;
    int32_t want = 0;
    if (fn_100d9ead(id, &want, 0))
        return;
    int32_t flag = TYPE(u) >= 0x44 && TYPE(u) <= 0x47 && TYPE(target) >= 0x30 && TYPE(target) <= 0x37;
    int32_t step = fn_100d9fa9(P(uint8_t, u, 0x7c), want, flag);
    P(uint8_t, u, 0x7c) = (uint8_t)(step + P(uint8_t, u, 0x7c));
    P(uint8_t, u, 0x7c) = (uint8_t)(P(uint8_t, u, 0x7c) + 0x30);
    P(uint8_t, u, 0x7c) = (uint8_t)(P(uint8_t, u, 0x7c) % 0x30);
    P(uint8_t, u, 0x2d) = 2;
}

/* 0x100f223a: one tick of wall post `id` */
void __cdecl post_tick(uint32_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return;
    int starting = 0;
    uint32_t old_pose = 0;
    if (P(int16_t, u, 0x1c9) >= 0) {
        if (!P(int16_t, u, 0x1c9))
            fn_1006a5c1(id);
        else
            P(uint16_t, u, 0x1c9)--;
    }
    post_update_walls(id);
    if ((NEXT_POSE(u) == 0x14 && POSE(u) != 0x14 && P(int16_t, u, 0xd6) > 0) ||
        (NEXT_POSE(u) == 0x16 && POSE(u) != 0x16)) {
        starting = 1;
        old_pose = POSE(u);
    }
    if (POSE(u) != 0x4c && POSE(u) != 0x4f)
        POSE(u) = NEXT_POSE(u);
    if (P(uint32_t, u, 0x149)) {
        if (NOW < P(uint32_t, u, 0x149))
            return;
        P(uint32_t, u, 0x149) = 0;
        if (!NEXT_POSE(u)) {
            int32_t obj = P(int32_t, INFO(u), 0xc0);
            if (!obj)
                P(int32_t, u, 0x18) = -1;
            else if (P(int32_t, u, 0x18) == -1)
                P(int32_t, u, 0x18) = IsoAddObjectC(g_iso, obj, P(int16_t, u, 0x52), P(int16_t, u, 0x54),
                                                    OBJ_FLAGS(obj), -1, -1);
            unit_voice(u, 7, 0, 0);
            P(uint32_t, u, 0x10e) &= 0xffff7fff;
        }
    }
    P(uint8_t, u, 0x2f) = (uint8_t)(I(uint32_t *, 0x1904)[P(int32_t, u, 0x56)] >> 24);
    fn_100f308b(id);
    switch (POSE(u)) {
    case 0:
        pose_building(u);
        break;
    case 1:
        pose_running(id, u);
        break;
    case 2:
        pose_stage(id, u, 0x9c, 0);
        break;
    case 3:
        pose_stage(id, u, 0xa0, 1);
        break;
    case 0x20:
    case 0x21:
        NEXT_POSE(u) = 0x4c;
        fn_100eb610(id);
        TIMER(u) = 0xa;
        return;
    case 0x4c:
    case 0x4f:
        if (pose_gone(id, u))
            return;
        break;
    case 0x14:
    case 0x16:
        if (pose_going(id, u, starting, old_pose))
            return;
        break;
    }
    if (P(uint32_t, u, 0x10e) >> 12 & 1) {
        unit_set_target(id, 0);
        P(uint16_t, u, 0x18b) = 0;
        unit_set_action(id, 0x1e, 1, 0);
    }
    turn_gun(id, u);
    if ((int16_t)unit_max_health(u, 0) <= P(int16_t, u, 0xd6) || POSE(u) == 0x14 || POSE(u) == 0x16)
        return;
    FLAGS3E(u) |= 4;
    if ((FLAGS3E(u) >> 1 & 1) && !(P(uint8_t, u, 0x3e) & 1) && power_unit_counts(u))
        fn_100ee418(id);
    if ((P(uint8_t, u, 0x3e) & 1) && power_unit_counts(u))
        repair_step(id, u);
}
