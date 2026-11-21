/* The game logic, 0x100aa238-0x100ab4aa: creating a unit in its slot (called by IsoAddUnit
 * and when units are built). Transports (kinds 0x88-0x9f) are made by 0x100b0837; other
 * units get their record (0x3a9 bytes) and are set up by IsoAddUnitType, then counted,
 * linked and announced. Units built by the player (flags bit 0) come out of the player's
 * main plant (iso+0xd6cb), which takes damage when it is marked (+0x3e bit 12). The result
 * is the unit's id; 0 or a negative code when it fails (the reason in 0x1017df58). */
#include "iso_core.h"
#include "sector.h"
#include "combat.h"
#include "relay.h"
#include "debugtime.h"
#include "misc.h"
#include "targets.h"
#include "walls.h"
#include "power.h"
#include "vecscore.h"
#include "unitorder.h"
#include "unitinfo.h"
#include "playerinfo.h"
#include "buildinfo.h"
#include "unitlists.h"
#include "unitcreate.h"

#define FAIL G(uint8_t, 0x1017df58)
#define IsoSnapTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, uint32_t *, int *, int *), 0x100aeebb)
#define IsoCheckFootprint ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, int32_t, uint32_t, int, uint32_t, int, int), 0x100f00bf)
#define IsoAddUnitType ORIG(uint32_t(__stdcall *)(uint8_t *, int, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, int32_t, uint32_t, int32_t, uint32_t), 0x100b2d8c)
#define transport_create ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, int, int, int, int32_t, uint32_t), 0x100b0837)
#define player_message ORIG(void(__cdecl *)(uint32_t, const char *, int), 0x1003ba11)
#define net_send_new_unit ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t), 0x100332af)
#define wall_cell_mark ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t), 0x1007a2e9)
#define refinery_setup ORIG(void(__cdecl *)(uint32_t), 0x100f33a0)
#define well_info_update ORIG(void(__cdecl *)(uint8_t *, uint8_t *), 0x100a67c0)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)
#define player_unit_built ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100b1dcb)
#define unit_upgrade_show ORIG(void(__cdecl *)(uint32_t), 0x1003ca26)
#define plant_damaged ORIG(void(__cdecl *)(uint32_t), 0x100eb80a)
#define net_send_damage ORIG(void(__cdecl *)(uint32_t, int), 0x10034aef)

static uint8_t *U(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id & 0xffff]; }
#define PLANT(p) I(uint16_t, 0xd6cb + (p) * 2)

uint32_t __cdecl unit_create(uint32_t id, uint32_t player, uint32_t kind, uint32_t a4, int32_t x, uint32_t y,
                             uint32_t a7, int32_t a8, uint32_t level, int32_t a10, uint32_t flags)
{
    int32_t net = I(int32_t, 0x148);
    DebugStr((const char *)ORIG_PTR(0x10157878), id & 0xffff, player, kind);
    int32_t base = (int32_t)(kind & 0xfffffffc);
    if (base >= 0x88 && base <= 0x9f) {
        if ((!I(int32_t, 0x175) || net) && !a8 && (flags & 1))
            a8 = I(int32_t, 0x17dd);
        if (!(flags & 1) && !a10) {
            if (base == 0x9c)
                kind -= 4;
            else if (base > 0x94)
                kind += 4;
        }
        uint32_t t = transport_create(player, kind, a4, x, y, a7, flags & 1, a8, level, flags & 2, 0, 0, 0, a10, id & 0xffff);
        if (!t)
            return 0;
        if (I(int32_t, ISO_HAVE_SPRITES) && !a10)
            target_new_transport(t);
        if (a10) {
            int32_t k = (int32_t)(kind & 0xfffffffc);
            int32_t last = I(int32_t, 0xd9de);
            if (k >= 0x88 && k <= 0x9f && (int32_t)t > 1 && last > 0 && I(uint8_t **, ISO_UNITS)[last]) {
                int32_t o = P(int32_t, I(uint8_t **, ISO_UNITS)[last], 0x10);
                if (((k >= 0x88 && k <= 0x90 && o >= 0x88 && o <= 0x90) || (k >= 0x94 && k <= 0x9c && o >= 0x94 && o <= 0x9c)) &&
                    (k == 0x88 || k == 0x94 || o == 0x88 || o == 0x94)) {
                    /* the two halves of a two-part transport are linked */
                    P(int16_t, I(uint8_t **, ISO_UNITS)[t], 0xf8) = (int16_t)I(int32_t, 0xd9de);
                    P(int16_t, I(uint8_t **, ISO_UNITS)[t], 0xf6) = 0;
                    P(int16_t, I(uint8_t **, ISO_UNITS)[I(int32_t, 0xd9de)], 0xf6) = (int16_t)t;
                }
            }
            if (k >= 0x88 && k <= 0x9c)
                I(uint32_t, 0xd9de) = t;
        }
        FAIL = 0;
        return t;
    }
    int tx = x, ty = (int)y, cx, cy;
    IsoVTilePos(g_iso, &tx, &ty, &cx, &cy);
    if (G(int16_t, 0x10118d84))
        ring_hide(G(int16_t, 0x10118d84));
    if (I(int32_t, ISO_HAVE_SPRITES) && (flags & 1) && (int32_t)kind < 0x40) {
        if (!PLANT(player) || !U(PLANT(player))) {
            FAIL = 2;
            player_message(I(uint32_t, 0x1b65), (const char *)ORIG_PTR(0x101578b0), 0);
            return 0;
        }
        uint8_t pk = (uint8_t)P(uint32_t, U(PLANT(player)), 0xc);
        iso_debug_print();
        if (pk >= 0x64 && pk < 0x68 && !P(int16_t, U(PLANT(player)), 0x42)) {
            player_replace_d6cb(PLANT(player), player);
            iso_debug_print();
            if (!PLANT(player) || !U(PLANT(player))) {
                FAIL = 2;
                return 0;
            }
        }
        x = P(int16_t, U(PLANT(player)), 0x52);
        y = (uint32_t)(int32_t)P(int16_t, U(PLANT(player)), 0x54);
        if (P(int32_t, U(PLANT(player)), 0x10) == 0x50) {
            RECT r;
            SpriteGetBounds(I(void *, 0x181e), P(void *, U(PLANT(player)), 4), &r);
            int32_t d = r.bottom - (int32_t)y;
            uint32_t ad = (uint32_t)(d < 0 ? -d : d);
            y = (ad >> 1) + y;
        }
        a7 = 0;
        int sx, sy;
        IsoSnapTilePos(g_iso, &x, &y, &sx, &sy);
        x = sx;
        y = (uint32_t)sy;
    }
    if (!(id & 0xffff) || (id & 0xffff) >= 2000) {
        FAIL = 1;
        return 0xfffffff6;
    }
    uint8_t **units = I(uint8_t **, ISO_UNITS);
    if (!units[id & 0xffff]) {
        units[id & 0xffff] = winmalloc(0x3a9, 1);
    } else {
        memset(units[id & 0xffff], 0, 0x3a9);
        unit_list_remove(id);
    }
    I(uint32_t, 0x1ad1) = unit_id_free() & 0xffff;
    if (!U(id)) {
        FAIL = 0x14;
        return 0xffffffe2;
    }
    P(uint16_t, U(id), 0x395) = (uint16_t)id;
    unit_set_1b1(id, 0xffff);
    if (I(int32_t, 0x175) && net && I(int32_t, 0x17f5) == 4 && I(uint8_t, 0xd90f) &&
        !net_send_new_unit(id, player, kind, a4, x, y, a7, a8, level, flags)) {
        /* in a network game the unit is made when the message comes back */
        if ((int32_t)kind >= 0x40 && !IsoCheckFootprint(g_iso, kind, x, y, 0, player, 1, 1)) {
            DebugStr((const char *)ORIG_PTR(0x10157910));
            winfree(U(id));
            I(uint8_t **, ISO_UNITS)[id & 0xffff] = 0;
            return 0;
        }
        DebugStr((const char *)ORIG_PTR(0x1015792c), id & 0xffff);
        FAIL = 3;
        return 0;
    }
    P(uint8_t, U(id), 0x2b) = I(uint8_t, 0xf1e + kind * 2);
    P(uint8_t, U(id), 0x2c) = I(uint8_t, 0xf1f + kind * 2);
    if ((int32_t)kind >= 0x64 && (int32_t)kind < 0x68 && P(uint8_t, U(id), 0x2c) == 0x63) {
        P(uint32_t, U(id), 0x3e) |= 0x800;
        P(uint32_t, U(id), 0x3e) |= 0x8000000;
    }
    P(int32_t, U(id), 0x149) = (a8 && (flags & 1)) ? a8 : 0;
    uint32_t r = IsoAddUnitType(g_iso, 1, id, player, kind, a4, x, y, a7, a8, level, a10, flags);
    if (r)
        return r;
    if ((int32_t)kind >= 0x40 && (int32_t)kind <= 0x4f) {
        IsoHideBeaconDirections(g_iso);
        uint32_t m = I(uint32_t *, 0x1934)[((uint32_t)ty << (I(uint8_t, 0x1aa0) & 0x1f)) + (uint32_t)tx];
        if ((m >> 14 & 1) && !(m >> 15 & 1))
            wall_cell_mark(id, tx, (uint32_t)ty);
    }
    if (P(int32_t, U(id), 0x10) == 0x5c && (uint32_t)P(uint8_t, U(id), 0x14) == I(uint32_t, 0x1b65) &&
        I(int32_t, ISO_HAVE_SPRITES)) {
        refinery_setup(id);
        if ((uint32_t)P(uint8_t, U(id), 0x14) == I(uint32_t, 0x1b65) && P(int32_t, U(id), 0xfa) > -1 &&
            I(int32_t, 0x17f5) == 1)
            well_info_update(U(id), g_iso + P(int32_t, U(id), 0xfa) * 0x2b + 0x1097);
    }
    if (!(flags & 1))
        P(uint32_t, U(id), 0x10e) &= 0xffff7fff;
    if (P(int32_t, U(id), 0x10) == 0x50 && !PLANT(player))
        PLANT(player) = (uint16_t)id;
    if (!I(int32_t, 0x175) && I(int32_t, ISO_HAVE_SPRITES) && !a10) {
        iso_debug_print();
        if (P(uint8_t, U(id), 0x14) == player)
            ai_event(player, 0, id & 0xffff, 0);
    }
    I(int32_t, player * ISO_PLAYER_SIZE + 0x1b8b)++;
    player_unit_built(player, kind);
    if (I(int32_t, ISO_HAVE_SPRITES)) {
        if (I(int32_t, 0x17f5) == 4)
            build_level_update(P(uint32_t, U(id), 0x10), (uint8_t)player, (uint8_t)level, 1);
        if ((int32_t)level > 1)
            unit_upgrade_show(id);
    }
    if (P(int32_t, U(id), 0x10) >= 0x40) {
        power_mark_all(1);
        if (!(flags & 1) && P(int32_t, P(uint8_t *, U(id), 0x32), 0xdc))
            P(uint32_t, U(id), 0x3e) |= 0x200;
    }
    if (I(int32_t, ISO_HAVE_SPRITES) && (flags & 1) && (int32_t)kind < 0x40 &&
        (P(uint32_t, U(PLANT(player)), 0x3e) >> 12 & 1) && (!I(int32_t, 0x175) || net)) {
        /* building in a damaged plant costs it a third of its health, plus 1 */
        uint16_t pid = PLANT(player);
        uint8_t *pl = U(pid);
        int16_t h = P(int16_t, pl, 0xd6);
        P(int16_t, pl, 0xd6) = (int16_t)(h - ((int16_t)unit_max_health(pl, 0) / 3 + 1));
        if (P(int16_t, pl, 0xd6) < 0)
            P(int16_t, pl, 0xd6) = 0;
        P(uint32_t, pl, 0x3e) |= 4;
        if (!I(int32_t, 0x175) || net) {
            if (P(int16_t, pl, 0xd6) < 1) {
                unit_kill(pid, 0x20, 1, 0);
                if (!(P(uint32_t, U(pid), 0x3e) >> 30 & 1)) {
                    score_unit_killed(P(uint32_t, U(pid), 0x10), P(uint8_t, U(pid), 0x14), P(uint8_t, U(pid), 0x14));
                    P(uint32_t, U(pid), 0x3e) |= 0x40000000;
                }
            } else {
                /* the original passes the id with a stack leftover in its upper half */
                plant_damaged(pid);
                if (I(int32_t, 0x175))
                    net_send_damage(pid, 0);
            }
        }
    }
    if (P(int32_t, U(id), 0xc) >= 0x40 && P(int32_t, U(id), 0xc) <= 0x4f)
        power_grid_update_x(P(uint8_t, U(id), 0x14));
    if (!I(int32_t, ISO_HAVE_SPRITES)) {
        uint8_t *e = winmalloc(10, 1);
        P(uint8_t *, U(id), 0x3a1) = e;
        e[0] = 0;
        *(uint32_t *)(e + 1) = 0;
        P(uint8_t, U(id), 0x15) = 1;
        *(uint32_t *)(P(uint8_t *, U(id), 0x3a1) + 6) = 0;
    } else {
        P(uint8_t, U(id), 0x15) = 0;
    }
    if (flags & 4) {
        P(uint32_t, U(id), 0x10e) |= 0x40000;
        unit_set_action(id, 0xc, 1, 0);
    }
    if (!I(int32_t, 0x175) || net)
        iso_debug_print();
    FAIL = 0;
    return id & 0xffff;
}
