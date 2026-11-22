/* The game logic, 0x100eeb92-0x100ef91a: a structure put on the map. */
#include "iso_core.h"
#include "events.h"
#include "combat.h"
#include "healthbar.h"
#include "playerinfo.h"
#include "power.h"
#include "radarmap.h"
#include "screen.h"
#include "sound.h"
#include "structplace.h"
#include "tilepos.h"
#include "unit.h"
#include "unitorder.h"
#include "unitsettarget.h"
#include "vision.h"
#include "wells.h"

#define HSL I(uint32_t, 0x181e)
#define NOW I(uint32_t, 0x17dd)
#define VISIBLE I(int32_t, 0x154)
#define LOADING (I(int32_t, 0x17f5) == 1)
#define INFO(u) P(uint8_t *, u, 0x32)
#define TYPE(u) P(int32_t, u, 0xc)
#define CLASS(u) P(int32_t, u, 0x10)
#define FLAGS3E(u) P(uint32_t, u, 0x3e)

#define update_footprint ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, int, int32_t), 0x100ef91a)
#define health_badly_hurt ORIG(int32_t(__cdecl *)(uint8_t *, int32_t), 0x100a2550)
#define replace_cells ORIG(void(__stdcall *)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t), 0x100f495e)
#define sprite_level ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t), 0x100f48d4)
#define sprite_position ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100f496a)
#define sprite_bounds ORIG(void(__stdcall *)(uint32_t, uint32_t, void *), 0x100f4952)
#define sprite_flip ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t), 0x100f497c)
#define sprite_process ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100f48da)
#define fn_100f3c43 ORIG(void(__cdecl *)(uint8_t *, int32_t), 0x100f3c43)
#define fn_1006c6f6 ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x1006c6f6)

/* the frame and health for the starting pose */
static void starting_pose(uint8_t *u)
{
    switch (P(uint8_t, u, 0x147)) {
    case 0:
        P(uint16_t, u, 0x145) = (uint16_t)P(uint32_t, INFO(u), 0x8c);
        P(uint16_t, u, 0xd6) = (uint16_t)unit_max_health(u, 1);
        break;
    case 1:
        P(uint16_t, u, 0x145) = (uint16_t)P(uint32_t, INFO(u), 0x90);
        P(uint16_t, u, 0xd6) = (uint16_t)unit_max_health(u, 1);
        break;
    case 2:
        P(uint16_t, u, 0x145) = (uint16_t)P(uint32_t, INFO(u), 0x9c);
        P(uint16_t, u, 0xd6) = (uint16_t)unit_health_share(u, 1);
        FLAGS3E(u) |= 0x1000000;
        break;
    case 3:
        P(uint16_t, u, 0x145) = (uint16_t)P(uint32_t, INFO(u), 0xa0);
        P(uint16_t, u, 0xd6) = (uint16_t)health_badly_hurt(u, 1);
        FLAGS3E(u) |= 0x1000000;
        break;
    }
}

/* the combat values from the type's record */
static void copy_stats(uint8_t *u)
{
    uint8_t *in = INFO(u);
    P(uint16_t, u, 0xe4) = (uint16_t)P(uint32_t, in, 0x8);
    P(uint16_t, u, 0xd8) = (uint16_t)P(uint32_t, in, 0x18);
    P(uint16_t, u, 0xda) = (uint16_t)P(uint32_t, in, 0x1c);
    P(uint16_t, u, 0xe6) = (uint16_t)P(uint32_t, in, 0x20);
    P(uint16_t, u, 0xe8) = (uint16_t)P(uint32_t, in, 0x24);
    P(uint16_t, u, 0xdc) = (uint16_t)P(uint32_t, in, 0x28);
    P(uint16_t, u, 0xe0) = (uint16_t)P(uint32_t, in, 0x30);
    P(uint8_t, u, 0xef) = P(uint8_t, in, 0xfc);
    P(uint8_t, u, 0xed) = 1;
    if (VISIBLE && P(int16_t, u, 0xe6) >= 0) {
        uint8_t *table = I(uint8_t *, 0x1990);
        uint8_t *w = P(uint8_t *, table, 0) + P(int32_t, table, 0x2c) * P(int16_t, u, 0xe6) * 4;
        P(uint16_t, u, 0xea) = (uint16_t)P(uint32_t, w, 0x58);
    }
}

/* the sprite: cells, level, position, bounds and mirroring */
static void setup_sprite(uint32_t id, uint8_t *u)
{
    uint32_t hs = P(uint32_t, u, 8), sp = P(uint32_t, u, 4);
    replace_cells(HSL, hs, sp, P(uint16_t, u, 0x145), 0, 1);
    replace_cells(HSL, hs, sp, P(uint32_t, INFO(u), 0x84), 1, 0xa);
    replace_cells(HSL, hs, sp, P(uint32_t, INFO(u), 0x80), 0xb, 1);
    if (VISIBLE)
        unit_health_bar_init(id);
    if (P(uint32_t, INFO(u), 0xb4))
        replace_cells(HSL, P(uint32_t, u, 8), P(uint32_t, u, 4), 0xc, 0xe, 1);
    int flat = CLASS(u) == 0x64 || (CLASS(u) >= 0x88 && CLASS(u) <= 0x9c);
    P(int16_t, u, 0x66) = (int16_t)(P(int16_t, u, 0x54) + (flat ? 2 : 0x2710));
    sprite_level(HSL, P(uint32_t, u, 4), P(int16_t, u, 0x66));
    sprite_position(HSL, P(uint32_t, u, 4), P(int16_t, u, 0x52), P(int16_t, u, 0x54), 0);
    sprite_bounds(HSL, P(uint32_t, u, 4), u + 0xc6);
    P(uint16_t, u, 0xb8) = (uint16_t)(P(uint32_t, u, 0xce) - P(uint32_t, u, 0xc6));
    P(uint16_t, u, 0xba) = (uint16_t)(P(uint32_t, u, 0xd2) - P(uint32_t, u, 0xca));
    if (CLASS(u) >= 0x94 && CLASS(u) <= 0x9c) {
        sprite_flip(HSL, P(uint32_t, u, 4), 1, 0);
        P(uint8_t, u, 0x159) = 1;
    }
}

/* the remaining fields to their empty values */
static void clear_links(uint32_t id, uint8_t *u)
{
    P(uint16_t, u, 0x1cb) = (uint16_t)id;
    P(uint16_t, u, 0x1cd) = (uint16_t)((P(uint16_t, u, 0x1cd) & 0xfe14) | 0x10);
    P(uint32_t, u, 0x1d3) = 0;
    P(uint16_t, u, 0x1e7) = 0;
    P(uint16_t, u, 0x360) = 0x8300;
    P(uint16_t, u, 0x362) = 0x8300;
    P(uint32_t, u, 0x364) = 0;
    P(uint16_t, u, 0x36c) = 0x8300;
    P(uint16_t, u, 0x36e) = 0x8300;
    P(uint16_t, u, 0x370) = 0xffff;
    for (int k = 0; k < 8; k++)
        P(uint16_t, u, 0x372 + k * 2) = 0xffff;
}

/* a wall clears what stands on it: at once while a map loads, else by a timer */
static void wall_clears_ground(uint32_t id, uint8_t *u)
{
    if (CLASS(u) < 0x88 || CLASS(u) > 0x9c)
        return;
    if (LOADING) {
        kill_units_on(u, 1, 1);
        return;
    }
    int32_t args[3] = {(int32_t)(id & 0xffff), 1, 0};
    timer_set(0xfa, (int32_t)NOW + 0x10, 0, ORIG_PTR(0x1000fe89), args);
}

/* a refinery takes the energy well under it */
static void take_well(uint8_t *u, uint8_t player)
{
    if (CLASS(u) != 0x5c)
        return;
    uint32_t state;
    int32_t w = IsoFindActiveEnergyWellC(g_iso, P(int16_t, u, 0x52), P(int16_t, u, 0x54), &state);
    if (w == -1)
        return;
    P(int32_t, u, 0xfa) = w;
    I(uint32_t, w * 43 + 0x10be) = player;
    radar_plot_object(g_iso + 0x1097 + w * 43, 0, 1);
}

/* 0x100eeb92: puts structure `id` of `player` on the map at (x, y) in `pose`; `quiet` keeps
 * it from revealing the map around it. Always 1 */
int32_t __cdecl structure_place(uint32_t id, uint32_t unused, uint8_t player, int16_t x_in, int16_t y_in, uint8_t pose,
                                uint8_t level, int32_t quiet)
{
    (void)unused;
    int x = x_in, sx, sy;
    uint32_t y = (uint32_t)(int32_t)y_in;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    P(uint8_t, u, 0x2a) = 0;
    P(int32_t, u, 0x56) = IsoSnapTilePosC(g_iso, &x, &y, &sx, &sy);
    if (I(uint32_t, 0x1a9c) <= (uint32_t)x || I(uint32_t, 0x1a98) <= y)
        P(int32_t, u, 0x56) = 0;
    P(uint16_t, u, 0x52) = (uint16_t)sx;
    P(uint16_t, u, 0x54) = (uint16_t)sy;
    P(uint16_t, u, 0x6c) = (uint16_t)sx;
    P(uint16_t, u, 0x6e) = (uint16_t)sy;
    P(uint16_t, u, 0x70) = (uint16_t)sx;
    P(uint16_t, u, 0x72) = (uint16_t)sy;
    P(uint8_t, u, 0x79) = pose;
    P(uint8_t, u, 0x7a) = pose;
    P(uint8_t, u, 0x7b) = pose;
    P(uint8_t, u, 0x7c) = 0;
    P(uint32_t, u, 0x7e) = 0;
    P(uint16_t, u, 0x82) = 0;
    P(uint32_t, u, 0x36) = 0;
    P(uint8_t, u, 0x16) = (uint8_t)(TYPE(u) % 4);
    P(uint32_t, u, 0x3a) = 0;
    P(uint16_t, u, 0x28) = (uint16_t)P(uint32_t, INFO(u), 0x74);
    P(uint8_t, u, 0x14) = player;
    FLAGS3E(u) |= 0x40;
    FLAGS3E(u) |= 0x400;
    FLAGS3E(u) |= 8;
    P(uint8_t, u, 0x2d) = 2;
    P(uint32_t, u, 0x10e) |= 0x8000;
    P(uint8_t, u, 0x159) = 0;
    P(uint32_t, u, 0x10e) &= 0xfffffeff;
    P(uint16_t, u, 0x8c) = 0xffff;
    P(uint16_t, u, 0x8e) = 0xffff;
    P(uint8_t, u, 0x30) = level;
    FLAGS3E(u) &= 0xffff7fff;
    P(uint8_t, u, 0x112) &= 0xfe;
    FLAGS3E(u) &= 0xfffffffe;
    if (I(int32_t, 0x175) && I(int32_t, P(uint8_t, u, 0x14) * ISO_PLAYER_SIZE + 0x1b69) == 1) {
        if (TYPE(u) < 0x88 || TYPE(u) > 0x9f)
            FLAGS3E(u) |= 2;
    } else {
        FLAGS3E(u) &= 0xfffffffd;
    }
    P(uint8_t, u, 0x147) = pose;
    P(uint8_t, u, 0x148) = pose;
    P(uint8_t, u, 0x106) = 0;
    P(uint8_t, u, 0x107) = 9;
    for (int k = 0; k < 4; k++)
        P(uint16_t, u, 0x1b5 + k * 2) = 0xffff;
    starting_pose(u);
    copy_stats(u);
    P(uint8_t, u, 0x197) = 0x1e;
    P(uint32_t, u, 0x15f) = 0;
    P(uint32_t, u, 0x163) = 0;
    unit_set_target(id, 0);
    P(uint16_t, u, 0x18b) = 0;
    P(uint8_t, u, 0x196) = 0x1e;
    unit_set_state(id, 0x1e);
    P(uint16_t, u, 0x1a1) = 0xffff;
    P(uint16_t, u, 0x4a) = 0xffff;
    P(uint16_t, u, 0x4c) = 0xffff;
    P(uint16_t, u, 0x4e) = 0xffff;
    P(uint16_t, u, 0x50) = 0xffff;
    if ((TYPE(u) < 0x84 || TYPE(u) > 0x87) && !quiet)
        iso_reveal(P(int16_t, u, 0x52), (uint32_t)(int32_t)P(int16_t, u, 0x54),
                   ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14), 8, 0xffffffff, (uint16_t)id, 0);
    unit_sector_update(id, 1, 0);
    update_footprint(g_iso, id & 0xffff, 1, 0);
    setup_sprite(id, u);
    P(uint32_t, u, 0x14f) = NOW;
    P(uint32_t, u, 0x1a3) = NOW;
    P(uint32_t, u, 0x1a7) = NOW;
    P(uint16_t, u, 0x1b3) = 0xffff;
    P(uint16_t, u, 0x1c9) = 0xffff;
    power_structure_placed((uint16_t)id);
    FLAGS3E(u) &= 0xfffffdff;
    P(int32_t, u, 0x1c) = -1;
    P(int32_t, u, 0x20) = -1;
    P(int32_t, u, 0x24) = -1;
    uint8_t *costs = I(uint8_t *, 0x18ec);
    P(uint32_t, u, 0xa0) = P(uint32_t, P(uint8_t *, costs, 0), P(int32_t, costs, 0x2c) * TYPE(u) * 4 + 0x30) + NOW;
    if (!VISIBLE || NOW > P(uint32_t, u, 0x149)) {
        fn_100f3c43(u, 0);
        if (VISIBLE && !LOADING) {
            unit_voice(u, 7, 0, 0);
            P(uint32_t, u, 0x10e) &= 0xffff7fff;
        }
    }
    if (CLASS(u) == 0x54 && (uint32_t)P(uint8_t, u, 0x14) == I(uint32_t, 0x1b65) && !I(int32_t, 0x12c))
        radar_show(1);
    clear_links(id, u);
    wall_clears_ground(id, u);
    P(uint16_t, u, 0x1bf) = 0xffff;
    P(uint16_t, u, 0x1c1) = 0xffff;
    P(uint16_t, u, 0x1c3) = 0xffff;
    take_well(u, player);
    if (VISIBLE)
        sprite_process(HSL, P(uint32_t, u, 4), 0);
    P(uint16_t, u, 0x191) = 0xffff;
    P(uint16_t, u, 0x18f) = 0xffff;
    if (P(uint8_t, u, 0x147) == 2) {
        unit_set_report_kind((uint16_t)id);
        if (VISIBLE) {
            fn_1006c6f6(id, 0, 0);
            fn_1006c6f6(id, 1, 0);
        }
        FLAGS3E(u) &= 0xfeffffff;
    } else if (P(uint8_t, u, 0x147) == 3) {
        unit_set_report_kind((uint16_t)id);
        if (VISIBLE) {
            fn_1006c6f6(id, 0, 1);
            fn_1006c6f6(id, 1, 1);
            fn_1006c6f6(id, 2, 0);
            fn_1006c6f6(id, 3, 0);
        }
        FLAGS3E(u) &= 0xfeffffff;
    }
    FLAGS3E(u) &= 0xfbffffff;
    FLAGS3E(u) &= 0xdfffffff;
    FLAGS3E(u) &= 0x7fffffff;
    P(uint8_t, u, 0x112) &= 0xfd;
    return 1;
}
