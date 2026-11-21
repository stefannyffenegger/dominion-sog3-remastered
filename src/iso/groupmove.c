/* The game logic: the group move (0x100b39aa-0x100b4683). The local player's selection is
 * sent to a point: units keep their places relative to a leader, are sorted by distance and
 * each gets its move (0x100b4ba4) with a little random spread. Units that were following or
 * carrying something drop it first; turrets turn instead. The selection is taken from the
 * local player (iso+0x1b65), but its count and the sorting from the player passed, as in the
 * original. */
#include "iso_core.h"
#include "sector.h"
#include "sound.h"
#include "netflags.h"
#include "unitorder.h"
#include "tilepos.h"
#include "unitai.h"
#include "groupmove.h"

#define SEL(p) I(uint16_t *, 0x193c + (p) * 4)
#define NSEL(p) I(int32_t, 0x1b23 + (p) * 4)
#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define orig_qsort ORIG(void(__cdecl *)(void *, size_t, size_t, void *), 0x100f57d0)
#define net_send_drop ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x10040706)
#define unit_drop_cargo ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10071766)
#define unit_stop_targeting ORIG(void(__cdecl *)(uint32_t, int), 0x100dcabd)
#define unit_hold ORIG(void(__cdecl *)(uint32_t, int), 0x100db98e)
#define net_send_stop ORIG(void(__cdecl *)(uint32_t), 0x1003da6d)
#define turret_aim ORIG(int32_t(__cdecl *)(uint32_t, int32_t *, int, uint32_t), 0x1008a000)
#define net_send_aim ORIG(void(__cdecl *)(uint32_t, int32_t *, int, uint32_t), 0x100401fa)
#define path_group ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int, int32_t *, int32_t *, int32_t *, int, int), 0x100ba61f)
#define unit_move ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, uint32_t, uint32_t, int, int32_t, int, int, uint32_t), 0x100b4ba4)

static uint8_t *U(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id & 0xffff]; }

/* the units that may move: able to (+0x32 +0x78), not busy or held, turrets only onto open
 * ground, kind 0x22 only where it can stand, not under special orders ('F', 'G', state ';') */
static int movable(uint8_t *u, int open, int stand)
{
    return P(int32_t, P(uint8_t *, u, 0x32), 0x78) && !(P(uint32_t, u, 0x10e) >> 12 & 1) && !(P(uint32_t, u, 0x3e) >> 16 & 1) &&
           !(P(uint32_t, u, 0x3e) >> 25 & 1) && !(P(uint32_t, u, 0x10e) >> 4 & 1) && !(P(uint32_t, u, 0x10e) >> 5 & 1) &&
           (open || P(int32_t, u, 0x10) < 0x30 || P(int32_t, u, 0x10) > 0x37) && (stand || P(int32_t, u, 0xc) != 0x22) &&
           P(uint8_t, u, 0x147) != 'F' && P(uint8_t, u, 0x147) != 'G' && P(uint8_t, u, 0x196) != ';';
}

void __cdecl group_move(int32_t player, int32_t x, uint32_t y, uint32_t a4, uint32_t action, int32_t no_leader,
                        uint32_t a7)
{
    uint16_t leader = 0;
    int vx = x, cx, cy;
    uint32_t vy = y;
    int32_t count = 0;
    int voiced = 0;
    uint16_t spread = 0;
    int32_t cell = IsoVTilePosC(g_iso, &vx, &vy, &cx, &cy);
    uint32_t m = I(uint32_t *, 0x1934)[cell];
    int open = ((uint8_t)(I(uint32_t *, 0x1904)[cell] >> 24) & (uint8_t)(1 << (I(uint8_t, 0x1b65) & 0x1f))) && !(m & 1) &&
               !(m >> 14 & 1);
    int stand = open;
    vx = x;
    vy = y;
    cell = IsoSnapTilePosC(g_iso, &vx, &vy, &x, (int *)&y);
    if (!no_leader)
        for (uint16_t k = 0; (int32_t)k < NSEL(player); k++) {
            uint16_t id = SEL(player)[k];
            if (!id || !U(id))
                continue;
            uint8_t *u = U(id);
            if (P(int32_t, u, 0x10) < 0x40 && (P(int32_t, u, 0x10) < 0x30 || P(int32_t, u, 0x10) > 0x37 || open) &&
                (P(int32_t, u, 0xc) != 0x22 || stand) && !(P(uint32_t, u, 0x3e) >> 16 & 1)) {
                leader = id;
                break;
            }
        }
    for (uint16_t k = 0; (int32_t)k < NSEL(player); k++) {
        uint16_t id = SEL(I(int32_t, 0x1b65))[k];
        if (!id || !U(id) || !movable(U(id), open, stand))
            continue;
        uint8_t *u = U(id);
        int32_t dx = x - P(int16_t, u, 0x52), dy = (int32_t)y - P(int16_t, u, 0x54);
        P(int32_t, u, 0xc2) = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (!leader || leader == id) {
            P(int16_t, u, 0xbe) = 0;
            P(int16_t, u, 0xc0) = 0;
        } else if (!P(int32_t, P(uint8_t *, u, 0x32), 0xb0)) {
            /* its place relative to the leader, at most 240 pixels each way */
            P(int16_t, u, 0xbe) = (int16_t)-(P(int16_t, U(leader), 0x52) - P(int16_t, u, 0x52));
            P(int16_t, u, 0xc0) = (int16_t)-(P(int16_t, U(leader), 0x54) - P(int16_t, u, 0x54));
            if ((P(int16_t, u, 0xbe) < 0 ? -P(int16_t, u, 0xbe) : P(int16_t, u, 0xbe)) > 0xf0)
                P(int16_t, u, 0xbe) = P(int16_t, u, 0xbe) < 1 ? -0xf0 : 0xf0;
            if ((P(int16_t, u, 0xc0) < 0 ? -P(int16_t, u, 0xc0) : P(int16_t, u, 0xc0)) > 0xf0)
                P(int16_t, u, 0xc0) = P(int16_t, u, 0xc0) < 1 ? -0xf0 : 0xf0;
        }
        if (!voiced)
            voiced = unit_voice(u, 3, 0, 1);
        count++;
        if (!P(uint16_t, u, 0x17d)) {
            unit_set_state(id, (uint8_t)action);
            unit_set_action(id, action, 1, 0);
        } else {
            uint16_t c = P(uint16_t, u, 0x17d);
            if (U(c) && P(uint16_t, U(c), 0x9a) == id)
                P(uint16_t, U(c), 0x9a) = 0;
            if (P(uint16_t, u, 0xfc) || P(uint16_t, u, 0xfa)) {
                if (I(int32_t, 0x175) && unit_is_local_u(u))
                    net_send_drop(id, ((uint32_t)(uintptr_t)u & 0xffff0000) | c, 4);
                unit_drop_cargo(((uint32_t)(uintptr_t)u & 0xffff0000) | c, id);
                unit_stop_targeting(id, 0);
                P(uint16_t, u, 0xf4) = 0;
            }
            P(uint16_t, u, 0x9a) = 0;
            if (!(P(uint8_t, u, 0x10e) & 1))
                unit_stop_targeting(id, 0);
            else
                unit_hold(id, 1);
            P(uint16_t, u, 0x18b) = 0;
            unit_set_state(id, 0xf);
            unit_set_action(id, 0xf, 1, 0);
            if (I(int32_t, 0x175))
                net_send_stop(id);
        }
        P(uint8_t, u, 0x195) = 0;
    }
    orig_qsort(SEL(player), (size_t)NSEL(player), 2, (void *)units_by_distance);
    int first = 1;
    int32_t minx = 0x20 - I(int32_t, 0xd709), miny = 0x10 - I(int32_t, 0xd71e);
    int32_t maxx = I(int32_t, 0xd709) - 0x20, maxy = I(int32_t, 0xd71e) - 0x10;
    for (uint16_t k = 0; (int32_t)k < NSEL(player); k++) {
        uint16_t id = SEL(I(int32_t, 0x1b65))[k];
        if (!id || !U(id))
            continue;
        uint8_t *u = U(id);
        if (P(int16_t, u, 0x74) == x && P(int16_t, u, 0x76) == (int32_t)y)
            continue;
        if (!movable(u, open, stand))
            continue;
        P(int16_t, u, 0x74) = (int16_t)x;
        P(int16_t, u, 0x76) = (int16_t)y;
        if (NSEL(player) < 2 || P(int16_t, u, 0x113) > 0)
            spread = 0;
        else
            spread = (uint16_t)(orig_rand() % 4 + spread);
        int32_t tx = P(int16_t, u, 0xbe) + x, ty = P(int16_t, u, 0xc0) + (int32_t)y;
        if (tx < minx)
            tx = minx;
        if (maxx < tx)
            tx = maxx;
        if (ty < miny)
            ty = miny;
        if (maxy < ty)
            ty = maxy;
        P(uint32_t, u, 0x10e) &= 0xfffffffb;
        P(uint32_t, u, 0x10e) &= 0xfdffffff;
        P(uint32_t, u, 0x10e) &= 0xfffbffff;
        P(uint32_t, u, 0x10e) &= 0xffffffbf;
        P(uint16_t, u, 0x193) &= 0xfff7;
        P(uint16_t, u, 0x193) &= 0xfffb;
        if (U(P(uint16_t, u, 0x1ab)))
            P(uint16_t, U(P(uint16_t, u, 0x1ab)), 0x193) &= 0xfffd;
        P(uint16_t, u, 0x1ab) = 0;
        if (P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 1) {
            if (P(uint32_t, u, 0x10e) >> 17 & 1)
                unit_order(id, 0xc, 1, 0);
            else if (P(uint32_t, u, 0x10e) >> 23 & 1)
                unit_order(id, 0x4a, 1, 0);
        }
        if (P(int32_t, u, 0x10) < 0x30 || P(int32_t, u, 0x10) > 0x37 || !P(void *, u, 0x39d)) {
            path_group(id, first == 0, count, 0, &tx, &ty, &cell, 0, 0);
            first = 0;
            unit_move(id, tx, ty, a4, spread, 0, count, 0, 0, a7);
        } else {
            int32_t at[2] = {tx, ty};
            int32_t d = net_delay();
            if (turret_aim(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb), at, 0, I(uint32_t, 0x17dd) + d) &&
                I(int32_t, 0x175) && unit_is_local_u(u)) {
                d = net_delay();
                net_send_aim(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb), at, 0, I(uint32_t, 0x17dd) + d);
            }
        }
    }
    if (!first)
        path_group(0, 2, 0, 0, 0, 0, 0, 0, 0);
}
