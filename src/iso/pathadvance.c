/* The game logic, 0x100ea41f-0x100ea9f5: a unit moving on to the next node of its path. */
#include "iso_core.h"
#include "pathadvance.h"
#include "pathnodes.h"
#include "sector.h"
#include "unitorder.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, NODE(k), 0xe)
#define occ_lower ORIG(void(__cdecl *)(int32_t, int32_t), 0x10067214)
#define net_path_step ORIG(void(__cdecl *)(uint32_t), 0x10032a32)

/* the animation that ends pose `pose` */
static uint32_t stop_pose(uint8_t pose)
{
    switch (pose) {
    case 4:
    case 6:
        return 0x11;
    case 3:
    case 5:
        return 0x12;
    case 0x17:
        return 0x1a;
    case 0x18:
        return 0x19;
    case 8:
    case 0x22:
        return 0x22;
    case 0x20:
    case 0x21:
    case 0x4c:
        return 0x4c;
    default:
        return 0;
    }
}

/* 0x100ea41f: unit `id` has reached its current node (+0x113), whose cell gives up a mark
 * while +0xe113 is set. At the end of the path (-3) it plays the stopping animation and sets
 * flag bit 27 (0). Otherwise the next node becomes current; with `full` the leg's ends, the
 * direction and the time are taken from it too. 1 when it moved on */
int32_t __cdecl unit_next_node(uint32_t id, int32_t full)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    int32_t k = P(int16_t, u, 0x113);
    if (k > 0 && I(int32_t, 0xe113)) {
        int32_t x = P(int16_t, NODE(k), 4), y = P(int16_t, NODE(k), 6), a, b;
        int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
        if (!(P(uint16_t, NODE(k), 0xa) >> 9 & 1))
            occ_lower(cell, 2);
    }
    if (k <= 0 || NEXT(k) == -3) {
        if (k > 0) {
            vector_free((uint16_t)k);
            P(int16_t, u, 0x113) = 0;
        }
        unit_order(id, stop_pose(P(uint8_t, u, 0x147)), 1, 0);
        P(uint32_t, u, 0x10e) |= 0x8000000;
        return 0;
    }
    int32_t n = NEXT(k);
    if (n <= 0)
        return 0;
    if (full) {
        P(int16_t, u, 0x68) = P(int16_t, NODE(k), 4);
        P(int16_t, u, 0x6a) = P(int16_t, NODE(k), 6);
    }
    P(int16_t, u, 0x113) = (int16_t)n;
    vector_free((uint16_t)k);
    if (full) {
        P(int16_t, u, 0x6c) = P(int16_t, NODE(n), 4);
        P(int16_t, u, 0x6e) = P(int16_t, NODE(n), 6);
        P(uint8_t, u, 0x7a) = P(uint8_t, NODE(n), 9);
        P(int32_t, u, 0x149) = P(int32_t, NODE(n), 0);
    }
    if (I(int32_t, 0x175) && unit_is_local(id))
        net_path_step(id & 0xffff);
    return 1;
}
