/* The game logic, 0x100ebb44-0x100ed14a: one tick of a structure: being built, running,
 * upgrading, being sold or destroyed, and repairing. */
#include "iso_core.h"
#include "buildinfo.h"
#include "combat.h"
#include "intfwave.h"
#include "mining.h"
#include "objects.h"
#include "playerinfo.h"
#include "power.h"
#include "sound.h"
#include "structtick.h"
#include "unitdelete.h"
#include "vecscore.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define INFO(u) P(uint8_t *, u, 0x32)
#define TYPE(u) P(int32_t, u, 0xc)
#define CLASS(u) P(int32_t, u, 0x10)
#define FLAGS3E(u) P(uint32_t, u, 0x3e)
#define FRAME(u) P(uint16_t, u, 0x145)
#define POSE(u) P(uint8_t, u, 0x147)
#define NEXT_POSE(u) P(uint8_t, u, 0x148)
#define TIMER(u) P(uint32_t, u, 0x1a7)
#define NOW I(uint32_t, 0x17dd)
#define PL(u, off) I(uint32_t, P(uint8_t, u, 0x14) * ISO_PLAYER_SIZE + (off))
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))
#define HB(u, off) (((uint32_t)(uintptr_t)(u) & 0xffffff00) | P(uint8_t, u, off))
#define COST(u, off)                                                                         \
    P(uint32_t, P(uint8_t *, I(uint8_t *, 0x18ec), 0), P(int32_t, I(uint8_t *, 0x18ec), 0x2c) * TYPE(u) * 4 + (off))
#define IS_WALL(u) (CLASS(u) >= 0x88 && CLASS(u) <= 0x9f)

#define res_get ORIG(uint32_t *(__cdecl *)(uint32_t, uint32_t), 0x100f48a4)
#define sprite_empty ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x100f48c2)
#define sprite_hide ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t), 0x100f48c8)
#define sprite_post ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48ce)
#define sprite_process ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100f48da)
#define delete_object ORIG(void(__stdcall *)(uint8_t *, int32_t), 0x100adbde)
#define unit_after_tick ORIG(void(__cdecl *)(uint32_t), 0x100f308b)
#define fn_100f3c43 ORIG(void(__cdecl *)(uint8_t *, int32_t), 0x100f3c43)
#define fn_1008c3b1 ORIG(uint32_t(__cdecl *)(uint32_t), 0x1008c3b1)
#define fn_100ee01c ORIG(void(__cdecl *)(uint32_t), 0x100ee01c)
#define fn_100b1ce0 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100b1ce0)
#define show_stage ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100ed2ba)
#define fn_100a67c0 ORIG(void(__cdecl *)(uint8_t *, uint8_t *), 0x100a67c0)
#define fn_100ed14a ORIG(void(__cdecl *)(uint32_t), 0x100ed14a)
#define fn_1006c6f6 ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x1006c6f6)
#define fn_100eb610 ORIG(void(__cdecl *)(uint32_t), 0x100eb610)
#define fn_10085da0 ORIG(int32_t(__cdecl *)(uint32_t), 0x10085da0)
#define fn_100a5240 ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x100a5240)
#define fn_100ee5e3 ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100ee5e3)
#define fn_100ee418 ORIG(void(__cdecl *)(uint32_t), 0x100ee418)
#define fn_100ee92a ORIG(uint16_t(__cdecl *)(uint32_t), 0x100ee92a)
#define fn_100ee870 ORIG(int32_t(__cdecl *)(uint32_t), 0x100ee870)
#define fn_100f3951 ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x100f3951)
#define fn_100f365b ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t), 0x100f365b)
#define fn_100eb80a ORIG(void(__cdecl *)(uint32_t), 0x100eb80a)

/* the sprite of map object `obj` */
static uint32_t object_sprite(int32_t obj)
{
    return res_get(I(uint32_t, 0x180e), P(uint32_t, I(uint8_t *, 0x192c), obj * 0x12))[1];
}

/* a kind-9 unit carried by a plant of types 0x60-0x63 (+0x108) goes down with it */
static void kill_carried(uint8_t *u)
{
    if (TYPE(u) < 0x60 || TYPE(u) > 0x63 || !P(uint16_t, u, 0x108) || !UNIT(P(uint16_t, u, 0x108)) ||
        fn_10085da0(HW(u, 0x108)) != 9)
        return;
    unit_kill(P(uint16_t, u, 0x108), 0x20, 1, 0);
    uint8_t *c = UNIT(P(uint16_t, u, 0x108));
    if (FLAGS3E(c) >> 30 & 1)
        return;
    score_unit_killed(CLASS(c), P(uint8_t, u, 0x14), P(uint8_t, u, 0x14));
    FLAGS3E(UNIT(P(uint16_t, u, 0x108))) |= 0x40000000;
}

static void score_own_death(uint8_t *u)
{
    if (FLAGS3E(u) >> 30 & 1)
        return;
    score_unit_killed(CLASS(u), P(uint8_t, u, 0x14), P(uint8_t, u, 0x14));
    FLAGS3E(u) |= 0x40000000;
}

/* pose 1: standing and running */
static void pose_running(uint32_t id, uint8_t *u, uint32_t old_frame)
{
    FRAME(u) = (uint16_t)P(uint32_t, INFO(u), 0x90);
    if (TYPE(u) >= 0x60 && TYPE(u) <= 0x63) {
        uint32_t extra = fn_1008c3b1(id);
        FRAME(u) = (uint16_t)(extra + FRAME(u));
        if (FRAME(u) != old_frame && P(uint8_t, u, 0x2d) != 4)
            P(uint8_t, u, 0x2d) = 2;
    }
    if (P(uint8_t, u, 0x2d) == 4)
        P(uint8_t, u, 0x2d) = 2;
    if (FLAGS3E(u) >> 8 & 1)
        fn_100ee01c(id);
    int32_t top;
    if (!I(int32_t, 0x11c) || !(FLAGS3E(u) >> 9 & 1) || !(top = P(int32_t, INFO(u), 0xdc)))
        return;
    uint32_t rec = P(uint32_t, P(uint8_t *, I(uint8_t *, 0x1930), 0),
                     P(int32_t, I(uint8_t *, 0x1930), 0x2c) * top * 4 + 0xc);
    P(int32_t, u, 0x1c) = IsoAddObjectC(g_iso, top, P(int16_t, u, 0x52) - 0x20, P(int16_t, u, 0x54) - 0x10, rec, -1,
                                        P(int16_t, u, 0x66) + 1);
    FLAGS3E(u) &= 0xfffffdff;
    if (P(int32_t, u, 0x1c) >= 0) {
        uint32_t s = object_sprite(P(int32_t, u, 0x1c));
        sprite_process(I(uint32_t, 0x181e), s, 1);
        if (NET && CLASS(u) != 0x5c)
            fn_100b1ce0(s, PL(u, 0x1ba2), TYPE(u), 0, 0);
    }
    if (P(int16_t, u, 0x42) || CLASS(u) == 0x50)
        show_stage(id, 0);
    else if (CLASS(u) == 0x5c)
        show_stage(id, 2);
    else
        show_stage(id, 1);
}

/* pose 0: being built, a frame at a time */
static void pose_building(uint8_t *u)
{
    if (P(uint32_t, INFO(u), 0xac) + TIMER(u) >= NOW)
        return;
    TIMER(u) = NOW;
    FRAME(u)++;
    P(uint8_t, u, 0x2d) = 2;
    if (P(int32_t, INFO(u), 0x90) > (int32_t)FRAME(u))
        return;
    NEXT_POSE(u) = 1;
    if (P(int32_t, INFO(u), 0xdc))
        FLAGS3E(u) |= 0x200;
    else
        FLAGS3E(u) &= 0xfffffdff;
    FLAGS3E(u) |= 0x20;
    if ((uint32_t)P(uint8_t, u, 0x14) == I(uint32_t, 0x1b65) && P(int32_t, u, 0xfa) > -1)
        fn_100a67c0(u, g_iso + 0x1097 + P(int32_t, u, 0xfa) * 43);
}

/* poses 2 and 3: an upgrade stage shown */
static void pose_upgraded(uint32_t id, uint8_t *u, uint32_t info_off, int32_t level)
{
    if (P(uint32_t, INFO(u), info_off) == FRAME(u))
        return;
    FRAME(u) = (uint16_t)P(uint32_t, INFO(u), info_off);
    P(uint8_t, u, 0x2d) = 2;
    fn_100ed14a(id);
    if (CLASS(u) == 0x80)
        power_grid_sum(P(uint8_t, u, 0x14));
    fn_1006c6f6(id, 0, level);
    fn_1006c6f6(id, 1, level);
    if (level) {
        fn_1006c6f6(id, 2, 0);
        fn_1006c6f6(id, 3, 0);
    }
}

/* poses 0x4c and 0x4f: gone; after the countdown its objects and the unit are deleted */
static void pose_gone(uint32_t id, uint8_t *u)
{
    if (TIMER(u)) {
        TIMER(u)--;
        return;
    }
    kill_carried(u);
    if (P(uint8_t, u, 0x30) > 1 || (FLAGS3E(u) >> 8 & 1)) {
        if (P(int32_t, u, 0x24) != -1)
            delete_object(g_iso, P(int32_t, u, 0x24));
        if (P(int32_t, u, 0x20) != -1)
            delete_object(g_iso, P(int32_t, u, 0x20));
    }
    if (P(int32_t, u, 0x1c) != -1) {
        delete_object(g_iso, P(int32_t, u, 0x1c));
        P(int32_t, u, 0x1c) = -1;
        FLAGS3E(u) &= 0xfffffdff;
    }
    if (IS_WALL(u))
        kill_units_on(u, 0, 0);
    int32_t c = CLASS(u);
    if (POSE(u) == 0x4f) {
        if (c != 0x88 && c != 0x94 && c != 0x90 && c != 0x9c)
            player_announce(P(uint8_t, u, 0x14), 0x21, 1);
        IsoDeleteUnitC(g_iso, id & 0xffff, 0);
    } else if (c == 0x88 || c == 0x94) {
        IsoDeleteUnitC(g_iso, id & 0xffff, 0);
    } else if (c == 0x90 || c == 0x9c) {
        IsoDeleteUnitC(g_iso, id & 0xffff, 1);
    } else {
        player_announce(P(uint8_t, u, 0x14), 0x2d, 1);
        IsoDeleteUnitC(g_iso, id & 0xffff, 1);
    }
    power_mark_all(1);
}

/* poses 0x14 and 0x16: dying or being sold, a frame at a time. Returns 1 at the last frame */
static int pose_going(uint32_t id, uint8_t *u, int starting, uint32_t old_pose)
{
    if (starting) {
        TIMER(u) = NOW;
        int32_t full = P(int32_t, INFO(u), 0x90);
        FRAME(u) = (uint16_t)(old_pose == 2 ? full - 1 : old_pose == 3 ? full - 3 : full);
        if ((P(uint8_t, u, 0x30) > 1 || (FLAGS3E(u) >> 8 & 1)) && P(int32_t, u, 0x24) != -1) {
            uint32_t s = object_sprite(P(int32_t, u, 0x24));
            if (s) {
                sprite_empty(I(uint32_t, 0x181e), s);
                sprite_hide(I(uint32_t, 0x181e), s, 1);
            }
        }
        show_stage(id, 2);
        uint8_t *v = UNIT(id & 0xffff);
        if (P(uint32_t, v, 0x102))
            sprite_hide(I(uint32_t, 0x181e), P(uint32_t, v, 0x102), 1);
        kill_carried(u);
        P(uint8_t, u, 0x2d) = 2;
        return 0;
    }
    if (P(uint32_t, INFO(u), 0xac) + TIMER(u) >= NOW)
        return 0;
    TIMER(u) = NOW;
    FRAME(u)--;
    P(uint8_t, u, 0x2d) = 2;
    if (POSE(u) == 0x14)
        fn_100a5240(id & 0xffff, 0, HB(u, 0x14));
    if (P(uint8_t, u, 0x30) > 1 || (FLAGS3E(u) >> 8 & 1)) {
        int32_t obj = P(int32_t, u, 0x20);
        if (obj != -1) {
            uint32_t s = object_sprite(obj);
            if (s) {
                int32_t last = P(int32_t, (uint8_t *)(uintptr_t)s, 0x74) - 1;
                if (last < 0)
                    last = 0;
                G(int32_t, 0x101572c4) = last;
                G(int32_t, 0x101572c8) = last;
                G(int32_t, 0x101572cc) = 0;
                sprite_post(I(uint32_t, 0x181e), s, 4, ORIG_PTR(0x101572c0));
            }
        }
    }
    if (P(uint32_t, INFO(u), 0x8c) != FRAME(u))
        return 0;
    NEXT_POSE(u) = 0x4f;
    TIMER(u) = 0;
    if (POSE(u) == 0x14) {
        fn_100a5240(id & 0xffff, 1, HB(u, 0x14));
        score_own_death(u);
    }
    return 1;
}

/* one step of a repair: paid for in proportion, as far as the player can afford */
void repair_step(uint32_t id, uint8_t *u)
{
    if (!P(uint32_t, u, 0xa4))
        return;
    uint32_t steps = P(uint32_t, u, 0xa4), done = P(uint32_t, u, 0xa8);
    uint32_t cost_a = COST(u, 0xc) * done / steps, cost_b = COST(u, 0x10) * done / steps;
    uint32_t da = cost_a - P(uint32_t, u, 0xac), db = cost_b - P(uint32_t, u, 0xb0);
    uint32_t kind = FLAGS3E(u) >> 1 & 1;
    if (PL(u, 0x1b97) < da || PL(u, 0x1b9b) < db) {
        if (!fn_100f3951(HW(u, 0x4e), kind ? 6 : 5))
            fn_100f365b(id, kind, 1, 0);
        return;
    }
    if (!fn_100f3951(HW(u, 0x4e), kind ? 4 : 3))
        fn_100f365b(id, kind, 1, 1);
    PL(u, 0x1b97) -= da;
    PL(u, 0x1b9b) -= db;
    P(uint32_t, u, 0xac) += da;
    P(uint32_t, u, 0xb0) += db;
    int32_t max = (int16_t)unit_max_health(u, 0);
    int32_t heal = (int32_t)(P(uint32_t, u, 0xa8) * (uint32_t)max / P(uint32_t, u, 0xa4));
    int32_t left = max - P(int16_t, u, 0xd6), due = heal - P(int32_t, u, 0xb4);
    int32_t step = left < due ? left : due;
    if (P(int16_t, u, 0xd6) < (int16_t)max) {
        P(int16_t, u, 0xd6) = (int16_t)(P(int16_t, u, 0xd6) + step);
        P(int32_t, u, 0xb4) += step;
        P(uint32_t, u, 0xa8)++;
        fn_100eb80a(id);
    }
}

/* damaged and not dying: fire effects, repair of itself or of the wall it belongs to */
static void damaged(uint32_t id, uint8_t *u)
{
    FLAGS3E(u) |= 4;
    if ((FLAGS3E(u) >> 1 & 1) && !(P(uint8_t, u, 0x3e) & 1) && power_unit_counts(u)) {
        if (IS_WALL(u))
            fn_100ee5e3(id, 1);
        else
            fn_100ee418(id);
    }
    if (IS_WALL(u) && !(P(uint8_t, u, 0x3e) & 1) && power_unit_counts(u)) {
        uint32_t other = fn_100ee92a(id);
        if (fn_100ee870(other))
            fn_100ee5e3(other, 1);
    }
    if ((P(uint8_t, u, 0x3e) & 1) && power_unit_counts(u))
        repair_step(id, u);
}

/* 0x100ebb44: one tick of structure `id` */
void __cdecl structure_tick(uint32_t id)
{
    uint8_t *u = UNIT(id & 0xffff);
    int starting = 0, produce = 0;
    uint32_t old_pose = 0;
    if ((NEXT_POSE(u) == 0x14 && POSE(u) != 0x14 && P(int16_t, u, 0xd6) > 0) ||
        (NEXT_POSE(u) == 0x16 && POSE(u) != 0x16)) {
        starting = 1;
        old_pose = POSE(u);
    }
    if (TYPE(u) >= 0x64 && TYPE(u) < 0x68 && P(uint16_t, u, 0x9a)) {
        uint8_t *o = UNIT(P(uint16_t, u, 0x9a));
        if (o) {
            uint8_t st = P(uint8_t, o, 0x196), nx = P(uint8_t, o, 0x197), po = P(uint8_t, o, 0x147);
            if (!(st == 0x1e && (po == 0x4d || po == 0x27 || po == 0x26)) && st != 0x11 && nx != 0x11 && st != 0x13 &&
                nx != 0x13)
                P(uint16_t, u, 0x9a) = 0;
        }
    }
    if (POSE(u) != 0x4c && POSE(u) != 0x4f)
        POSE(u) = NEXT_POSE(u);
    if (P(uint32_t, u, 0x149)) {
        if (NOW < P(uint32_t, u, 0x149))
            return;
        P(uint32_t, u, 0x149) = 0;
        if (!NEXT_POSE(u)) {
            fn_100f3c43(u, 0);
            unit_voice(u, 7, 0, 0);
        }
        P(uint32_t, u, 0x10e) &= 0xffff7fff;
    }
    P(uint8_t, u, 0x2f) = (uint8_t)(I(uint32_t *, 0x1904)[P(int32_t, u, 0x56)] >> 24);
    unit_after_tick(id);
    uint32_t old_frame = FRAME(u);
    switch (POSE(u)) {
    case 0:
        pose_building(u);
        break;
    case 1:
        pose_running(id, u, old_frame);
        produce = 1;
        break;
    case 2:
        pose_upgraded(id, u, 0x9c, 0);
        produce = 1;
        break;
    case 3:
        pose_upgraded(id, u, 0xa0, 1);
        produce = 1;
        break;
    case 0x20:
    case 0x21:
        NEXT_POSE(u) = 0x4c;
        fn_100eb610(id);
        TIMER(u) = 0xa;
        return;
    case 0x4c:
    case 0x4f: {
        int counting = TIMER(u) != 0;
        pose_gone(id, u);
        if (!counting)
            return;
        break;
    }
    case 0x14:
    case 0x16:
        if (pose_going(id, u, starting, old_pose))
            return;
        break;
    }
    if ((int16_t)unit_max_health(u, 0) > P(int16_t, u, 0xd6) && POSE(u) != 0x14 && POSE(u) != 0x16)
        damaged(id, u);
    if (produce) {
        if (CLASS(u) == 0x7c)
            building_produce_men(id);
        else if (CLASS(u) == 0x5c)
            refinery_mine(id);
    }
}
