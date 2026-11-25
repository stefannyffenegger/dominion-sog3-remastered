/* The game logic, 0x1007618a-0x10076f60: moving a unit to a point, patrols, a unit's
 * build-up counter (+0x84/+0x88) and a structure's animation state (+0x3a). */
#include "iso_core.h"
#include "orders2.h"

#define IsoSnapTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, uint32_t *, int *, uint32_t *), 0x100aeebb)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define path_request ORIG(int(__cdecl *)(uint32_t, int, int, int, int *, uint32_t *, int *, int, int), 0x100ba61f)
#define unit_order_go ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int, int, int, int, int), 0x100b7a66)
#define path_release ORIG(int(__cdecl *)(uint32_t, int32_t, int, int), 0x100d559a)
#define unit_add_waypoint ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int, int32_t, int, int, int, int, int), 0x100b4ba4)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)
#define unit_built ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1005c899)

static uint8_t *unit_ptr(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

/* 0x1007618a: sends a unit (not a turret, structure or unit of action 0x10, nor flagged
 * +0x3e bit 25) to the point (x, y): action 0x1f, its target tile, a new path if it is not
 * already there, and the point as a waypoint. `flag` goes to +0x193 bit 5 */
void __cdecl unit_move_to(uint32_t unit, int32_t x, uint32_t y, uint16_t flag)
{
    uint8_t *u = unit_ptr(unit);
    if (!u || P(uint8_t, u, 0x196) == 0x10 || (P(uint32_t, u, 0x3e) >> 25 & 1) || P(int32_t, u, 0xc) >= 0x40 ||
        (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37))
        return;
    unit_set_action(unit, 0x1f, 1, 0);
    P(uint16_t, u, 0x193) |= 4;
    P(uint16_t, u, 0x193) |= 8;
    P(int16_t, u, 0x1ab) = 0;
    P(uint16_t, u, 0x193) = (uint16_t)((P(uint16_t, u, 0x193) & 0xffdf) | (flag & 1) << 5);
    int px = x, tx, tmp;
    uint32_t py = y, ty;
    IsoSnapTilePos(g_iso, &px, &py, &tx, &ty);
    P(int16_t, u, 0x74) = (int16_t)tx;
    P(int16_t, u, 0x76) = (int16_t)ty;
    if (P(int16_t, u, 0x52) != tx || P(int16_t, u, 0x54) != (int32_t)ty || P(int16_t, u, 0x113) > 0) {
        path_request(unit, 3, 1, 0, &tx, &ty, &tmp, 0, 0);
        if (!unit_order_go(unit, 0xffffffff, 1, 1, 0, 0, 0, 1))
            path_release(unit, P(int16_t, u, 0x113) > 0 ? P(int16_t, u, 0x11b) : -1, 1, P(int16_t, u, 0x113) > 0);
    }
    P(int16_t, u, 0x187) = (int16_t)tx;
    P(int16_t, u, 0x189) = (int16_t)ty;
    unit_add_waypoint(unit, tx, (int32_t)ty, 3, 0, 0, 1, 0, 0, 0);
}

static void patrol_point(uint32_t unit, const int32_t *points, const int32_t *delays, int k, int *pt)
{
    pt[0] = points[k * 2];
    uint32_t py = (uint32_t)points[k * 2 + 1];
    P(int16_t, unit_ptr(unit & 0xffff), 0x74) = (int16_t)pt[0];
    P(int16_t, unit_ptr(unit & 0xffff), 0x76) = (int16_t)py;
    path_request(unit, 0, 1, 0, &pt[0], &py, &pt[1], 0, 0);
    unit_add_waypoint(unit, pt[0], (int32_t)py, 3, delays[k], 0, 1, 0, 0, 0);
    path_request(0, 2, 0, 0, 0, 0, 0, 0, 0);
}

/* 0x100764b2: a patrol over up to 6 points with a delay at each; mode 2000 ("LINEAR") goes
 * back the same way, others ("CIRCULAR") start over. The owner's AI hears of it (event 0xd) */
void __cdecl unit_patrol(uint32_t unit, const int32_t *points, const int32_t *delays, int32_t count, int32_t mode)
{
    int pt[2] = { 0, 0 }; /* the third output stays from point to point, as in the original */
    if (!unit_order_go(unit, 0xffffffff, 1, 1, 0, 0, 0, 1))
        path_release(unit, -1, 1, 0);
    if (!(unit & 0xffff) || count >= 7 || !points || !delays)
        return;
    for (int k = 0; k < count; k++) {
        patrol_point(unit, points, delays, k, pt);
        DebugStr((const char *)ORIG_PTR(0x101473c4), count,
                 (const char *)ORIG_PTR(mode == 2000 ? 0x101473b0 : 0x101473b8), points[k * 2], points[k * 2 + 1],
                 delays[k]);
    }
    if (mode == 2000)
        for (int k = count - 2; k > 0; k--)
            patrol_point(unit, points, delays, k, pt);
    ai_event(P(uint8_t, unit_ptr(unit & 0xffff), 0x14), 0xd, unit & 0xffff, 0);
}

/* 0x10076af2: a unit's build-up step (+0x84: 0 none, 1 done, 2 counting down, 3 counting up
 * to 5 for allies of the local player, 9 for others; every other tick, 0x17dd bit 1).
 * Returns the counter (+0x88), or 0 */
int32_t __cdecl unit_buildup_step(uint32_t unit)
{
    uint8_t *u = unit_ptr(unit & 0xffff);
    if (!u || !P(int32_t, u, 0x84))
        return 0;
    int16_t top = (I(uint8_t, I(int32_t, ISO_LOCAL_PLAYER) * ISO_PLAYER_SIZE + 0x1ba0) & 1 << (P(uint8_t, u, 0x14) & 0x1f))
                      ? 5
                      : 9;
    switch (P(int32_t, u, 0x84)) {
    case 2:
        if (P(int16_t, u, 0x88) < 1)
            P(int32_t, u, 0x84) = 0;
        else if (I(uint8_t, 0x17dd) & 2)
            P(int16_t, u, 0x88)--;
        break;
    case 3:
        if (P(int16_t, u, 0x88) < top) {
            if (I(uint8_t, 0x17dd) & 2) {
                P(int16_t, u, 0x88)++;
                if (P(uint8_t, u, 0x2d) != 4)
                    P(uint8_t, u, 0x2d) = 2;
            }
        } else {
            P(int32_t, u, 0x84) = 1;
            unit_built(unit & 0xffff, 0xffffffff);
        }
        break;
    }
    return P(int16_t, u, 0x88);
}

/* 0x10076ca3: a unit's build-up counter (0 when none) */
int32_t __cdecl unit_buildup(uint32_t unit)
{
    uint8_t *u = unit_ptr(unit & 0xffff);
    return (!u || !P(int32_t, u, 0x84)) ? 0 : P(int16_t, u, 0x88);
}

/* 0x10076d00: starts counting a unit's build-up down (2, if it has one) or up (3, unless
 * done). Returns the state, or 0 */
int32_t __cdecl unit_buildup_set(uint32_t unit, int32_t state)
{
    uint8_t *u = unit_ptr(unit & 0xffff);
    if (!u)
        return 0;
    if (state == 2) {
        if (P(int32_t, u, 0x84))
            P(int32_t, u, 0x84) = 2;
    } else if (state == 3) {
        if (P(int32_t, u, 0x84) != 1)
            P(int32_t, u, 0x84) = 3;
    } else {
        return 0;
    }
    return P(int32_t, u, 0x84);
}

/* 0x10076de9: sets a structure's (types 0x40-0x87) animation state +0x3a */
int32_t __cdecl structure_set_anim(uint32_t unit, int32_t state)
{
    uint8_t *u = unit_ptr(unit & 0xffff);
    if (!u || P(int32_t, u, 0xc) < 0x40 || P(int32_t, u, 0xc) > 0x87)
        return 0;
    P(int32_t, u, 0x3a) = state;
    return P(int32_t, u, 0x3a);
}

/* 0x10076e53: a structure's animation step when its sprite has nothing queued: states 1 and
 * 3 give 0, 2 gives 1; every other tick the state moves on (table 0x10146ac8). -1 when there
 * is nothing to do (no unit, state 0, or something queued): the drawing loop then draws the
 * structure plainly, while 0 and up select a shaded blit */
int32_t __cdecl structure_anim_step(uint32_t unit)
{
    uint8_t *u = unit_ptr(unit & 0xffff);
    if (!u || P(int32_t, u, 0x3a) < 1 || P(int32_t, P(uint8_t *, P(uint8_t *, u, 4), 0x34), 0x28))
        return -1;
    int32_t r = P(int32_t, u, 0x3a) == 2;
    if (P(uint8_t, u, 0x2d) == 1)
        P(uint8_t, u, 0x2d) = 2;
    if (I(uint8_t, 0x17dd) & 2)
        P(int32_t, u, 0x3a) = G(int32_t, 0x10146ac8 + P(int32_t, u, 0x3a) * 4);
    return r;
}
