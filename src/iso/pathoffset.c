/* The game logic, 0x100e4aa0-0x100e4c70: where a unit aims for at a node of its path. */
#include "iso_core.h"
#include "pathoffset.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define vector_to ORIG(void(__cdecl *)(float *, float *, float *), 0x1007e720)

/* 0x100e4aa0: the point for unit `id` at path node `node`, into (*ox, *oy). Units other than
 * types 0x28-0x3f take the node less the offset (dx, dy), returning 1. Those stop at the
 * node (0) when it is the last or there is no offset; otherwise they aim short of it along
 * their line to it, by the distance at 0x1011332c (1) */
int32_t __cdecl path_point_offset(uint32_t id, uint32_t node, uint32_t dx, uint32_t dy, int16_t *ox, int16_t *oy)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    int16_t k = (int16_t)node, ddx = (int16_t)dx, ddy = (int16_t)dy;
    if (P(int32_t, u, 0xc) < 0x28 || P(int32_t, u, 0xc) >= 0x40) {
        *ox = (int16_t)(P(int16_t, NODE(k), 4) - ddx);
        *oy = (int16_t)(P(int16_t, NODE(k), 6) - ddy);
        return 1;
    }
    if (P(int16_t, NODE(k), 0xe) <= 0 || (!ddx && !ddy)) {
        *ox = P(int16_t, NODE(k), 4);
        *oy = P(int16_t, NODE(k), 6);
        return 0;
    }
    float a[2] = {(float)P(int16_t, u, 0x52), (float)P(int16_t, u, 0x54)};
    float b[2] = {(float)P(int16_t, NODE(k), 4), (float)P(int16_t, NODE(k), 6)}, v[2];
    vector_to(a, b, v);
    *ox = (int16_t)(int32_t)(-((long double)v[0] * G(float, 0x1011332c)) + b[0]);
    *oy = (int16_t)(int32_t)(-((long double)v[1] * G(float, 0x1011332c)) + b[1]);
    return 1;
}
