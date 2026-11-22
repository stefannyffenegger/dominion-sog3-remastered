/* The game logic, 0x100e9c85-0x100e9e85: dropping a unit's whole path. */
#include "iso_core.h"
#include "pathclear.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define occ_lower ORIG(void(__cdecl *)(int32_t, int32_t), 0x10067214)

/* 0x100e9c85: frees all of unit `id`'s path nodes (+0x113). While +0xe113 is set a freed
 * node's cell gives up a mark (2), unless the node's +0xa bit 9. The unit keeps facing its
 * current step (+0x7a from +0x7b). 0 without a unit, else 1 */
int32_t __cdecl unit_path_clear(uint32_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    for (int32_t k = P(int16_t, u, 0x113); k > 0;) {
        int32_t next = P(int16_t, NODE(k), 0xe);
        if (P(int8_t, NODE(k), 8) > 0) {
            if (I(int32_t, 0xe113)) {
                int32_t x = P(int16_t, NODE(k), 4), y = P(int16_t, NODE(k), 6), a, b;
                int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
                if (!(P(uint16_t, NODE(k), 0xa) >> 9 & 1))
                    occ_lower(cell, 2);
            }
            P(uint16_t, NODE(k), 0xe) = (uint16_t)I(int32_t, 0x1ad9);
            P(uint8_t, NODE(k), 8) = 0;
            I(int32_t, 0x1ad9) = k;
            I(int32_t, 0xd697)++;
        }
        k = next;
    }
    P(uint16_t, u, 0x113) = 0;
    P(uint8_t, u, 0x7a) = P(uint8_t, u, 0x7b);
    return 1;
}
