/* The game logic, 0x100e98de-0x100e9c85: the free list of path nodes. */
#include "iso_core.h"
#include "pathnodes.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define FREE_HEAD I(int32_t, 0x1ad9)
#define FREE_COUNT I(int32_t, 0xd697)

/* 0x100e98de: takes a node off the free list for `owner` and `action`, cleared, with the
 * owner in the top bits of +0x10 and +0xa bit 9 set for actions 0-7 and 0xc-0x17. Returns
 * the node, -1 when it was the last one (the last is never handed out). */
int32_t __cdecl vector_new(uint32_t owner, uint32_t action)
{
    uint32_t k = (uint16_t)FREE_HEAD;
    uint8_t *n = NODE(k);
    if (P(int16_t, n, 0xe) <= 0)
        return -1;
    FREE_HEAD = P(int16_t, n, 0xe);
    P(int16_t, n, 0xe) = -1;
    P(int32_t, n, 0) = 0;
    P(uint8_t, n, 8) = 0;
    P(uint16_t, n, 0xc) = 0;
    P(uint16_t, n, 0xa) &= 0xff00;
    P(uint16_t, n, 0x10) = (uint16_t)(((owner & 0xffff) << 13) + k);
    uint8_t a = (uint8_t)action;
    if (a < 8 || (a >= 0xc && a <= 0x17))
        P(uint16_t, n, 0xa) |= 0x200;
    else
        P(uint16_t, n, 0xa) &= 0xfdff;
    FREE_COUNT--;
    return (int32_t)k;
}

/* 0x100e9bbc: puts node `k` back on the free list, if it is in use. 0 for a bad number */
int32_t __cdecl vector_free(uint32_t k)
{
    k &= 0xffff;
    if (!k || k >= 0x1000)
        return 0;
    uint8_t *n = NODE(k);
    if (P(int8_t, n, 8) > 0) {
        P(uint16_t, n, 0xe) = (uint16_t)FREE_HEAD;
        P(uint8_t, n, 8) = 0;
        FREE_HEAD = (int32_t)k;
        FREE_COUNT++;
    }
    return 1;
}
