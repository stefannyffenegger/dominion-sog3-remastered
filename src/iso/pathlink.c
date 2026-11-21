/* The game logic: the linked lists of path nodes (18 bytes each at iso+0x1984, the next node
 * at +0xe), 0x100d4450, 0x100d4947 and 0x100d4bfa. */
#include "iso_core.h"
#include "pathlink.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, VEC(k), 0xe)
#define UNIT(id) (I(uint8_t **, ISO_UNITS)[(id) & 0xffff])
#define occ_lower ORIG(void(__cdecl *)(int32_t, int32_t), 0x10067214)
#define node_free ORIG(void(__cdecl *)(int32_t), 0x100e9bbc)

/* 0x100d4450: sets node `k`'s next node (the third argument has no effect) */
void __cdecl vector_link(int32_t k, int32_t next, int32_t unused)
{
    (void)unused;
    NEXT(k) = (int16_t)next;
}

/* 0x100d4947: frees unit `id`'s path after node `after` (from its start, +0x113, when 0 or
 * less), up to a node with +8 <= 0. With an `after` that node then leads to -3 (or to what
 * ended the list). While +0xe113 is set each node's cell gives up a mark (2), unless the
 * node's +0xa bit 9. Returns 0 without a unit */
int32_t __cdecl path_stop(uint32_t id, int32_t after)
{
    uint8_t *u = UNIT(id);
    if (!u)
        return 0;
    int32_t k = after > 0 ? NEXT(after) : P(int16_t, u, 0x113);
    while (k > 0) {
        if (P(int8_t, VEC(k), 8) <= 0) {
            if (after > 0)
                k = -3;
            break;
        }
        if (I(int32_t, 0xe113)) {
            int32_t x = P(int16_t, VEC(k), 4), y = P(int16_t, VEC(k), 6), a, b;
            int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
            if (!(P(uint16_t, VEC(k), 0xa) >> 9 & 1))
                occ_lower(cell, 2);
        }
        int32_t old = k;
        k = NEXT(k);
        node_free(old);
    }
    if (after > 0)
        vector_link(after, k, 1);
    else
        P(int16_t, u, 0x113) = 0;
    return 1;
}

/* 0x100d4bfa: joins the list `node` into unit `id`'s second path (+0x119). With `before`,
 * `node` goes in front of node `at`; without, after it, or at the list's end when `at` is 0
 * or less. Returns 0 without a unit, or when `at` is not found */
int32_t __cdecl unit_path_join(uint32_t id, int32_t at, int32_t node, int32_t before)
{
    uint8_t *u = UNIT(id);
    int32_t tail = node;
    if (!u)
        return 0;
    while (NEXT(tail) > 0)
        tail = NEXT(tail);
    if (before) {
        vector_link(node, at, 1);
        int32_t head = P(int16_t, u, 0x119);
        if (head <= 0 || head == at || at <= 0) {
            P(int16_t, u, 0x119) = (int16_t)node;
        } else {
            int32_t p = head;
            while (p > 0 && NEXT(p) != at)
                p = NEXT(p);
            if (p < 0)
                return 0;
            vector_link(p, node, 1);
        }
    } else if (at > 0) {
        vector_link(node, NEXT(at), 1);
        vector_link(at, node, 1);
    } else {
        vector_link(node, NEXT(at), 1);
        if (P(int16_t, u, 0x119) <= 0)
            P(int16_t, u, 0x119) = (int16_t)node;
        else
            vector_link(tail, node, 1);
    }
    return 1;
}
