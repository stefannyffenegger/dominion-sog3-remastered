/* The game logic, 0x100e9260-0x100e97ca: a unit setting off along a new path. */
#include "iso_core.h"
#include "pathlink.h"
#include "unitfollow.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, NODE(k), 0xe)
#define NODE_FLAGS(k) P(uint16_t, NODE(k), 0xa)
#define F(u) P(uint32_t, u, 0x10e)

/* the unit's formation slot (+0x10e bits 10-11) into a node's flags (bits 2-3) */
static void node_take_slot(const uint8_t *u, int32_t k)
{
    NODE_FLAGS(k) = (uint16_t)((NODE_FLAGS(k) & 0xfff3) | ((F(u) >> 10 & 3) << 2));
}

/* 0x100e9260: unit `id` takes the node list from `node` as its path (+0x113): the first leg
 * runs from where it stands to that node. Every node gets a use count (+8), the unit's slot
 * and `flag` (bit 8); the list ends in -3. A destination path (+0x11b) moves to the last node */
void __cdecl unit_path_follow(uint32_t id, int32_t node, uint32_t flag)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    P(uint8_t, NODE(node), 8)++;
    P(int16_t, u, 0x68) = P(int16_t, u, 0x52);
    P(int16_t, u, 0x6a) = P(int16_t, u, 0x54);
    P(int16_t, u, 0x6c) = P(int16_t, NODE(node), 4);
    P(int16_t, u, 0x6e) = P(int16_t, NODE(node), 6);
    P(uint8_t, u, 0x7a) = P(uint8_t, NODE(node), 9);
    P(uint8_t, u, 0x7b) = G(uint8_t, 0x1015b5e0 + P(uint8_t, u, 0x79) + P(uint8_t, u, 0x7a) * 8);
    P(int32_t, u, 0x149) = P(int32_t, NODE(node), 0);
    P(int16_t, u, 0x113) = (int16_t)node;
    P(uint8_t, u, 0x7d) = 8;
    F(u) &= 0xfeffffff;
    F(u) &= 0x7fffffff;
    P(uint8_t, u, 0x15a) = 0;
    int32_t k = node;
    node_take_slot(u, k);
    for (;;) {
        NODE_FLAGS(k) = (uint16_t)((NODE_FLAGS(k) & 0xfeff) | ((flag & 1) << 8));
        if (NEXT(k) <= 0)
            break;
        k = NEXT(k);
        P(uint8_t, NODE(k), 8)++;
        node_take_slot(u, k);
    }
    vector_link(k, -3, 1);
    P(int32_t, u, 0x149) = P(int32_t, NODE(P(int16_t, u, 0x113)), 0);
    if (P(int16_t, u, 0x11b) > 0) {
        P(int16_t, NODE(P(int16_t, u, 0x11b)), 4) = P(int16_t, NODE(k), 4);
        P(int16_t, NODE(P(int16_t, u, 0x11b)), 6) = P(int16_t, NODE(k), 6);
    }
    F(u) &= 0xffff7fff;
}
