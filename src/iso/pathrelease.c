/* The game logic, 0x100d559a-0x100d59e4: freeing the nodes of a unit's second path. */
#include "iso_core.h"
#include "pathrelease.h"
#include "pathlink.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, VEC(k), 0xe)
#define HEAD(u) P(int16_t, u, 0x119)
#define CURSOR(u) P(int16_t, u, 0x11b)
#define occ_lower ORIG(void(__cdecl *)(int32_t, int32_t), 0x10067214)
#define node_free ORIG(void(__cdecl *)(int32_t), 0x100e9bbc)

/* 0x100d559a: frees unit `id`'s second path (+0x119) after node `at`, or all of it when `at`
 * is 0 or less; with `from_list` the walk starts at the head and `at` itself stays. It ends
 * at a node with +8 <= 0. Nodes with +0xa bit 1 stay, relinked, unless `all`. A node that
 * links to itself is cut off. The path's cursor (+0x11b) moves to the next kept node. While
 * +0xe113 is set a freed node's cell gives up a mark (1), unless the node's +0xa bit 9.
 * Returns 0 without a unit or a path */
int32_t __cdecl path_release(uint32_t id, int32_t at, int32_t all, int32_t from_list)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    int32_t move_cursor = 0, passed = 0, kept = -1, k;
    if (!u || HEAD(u) <= 0)
        return 0;
    if (at > 0 && !from_list) {
        k = NEXT(at);
        if (k > 0 && k == at) {
            NEXT(at) = -1;
            return 1;
        }
    } else {
        k = HEAD(u);
    }
    while (k > 0) {
        if (P(int8_t, VEC(k), 8) <= 0)
            break;
        if (from_list && k == at) {
            if (NEXT(k) == k)
                NEXT(k) = -1;
            k = NEXT(k);
            passed = 1;
            continue;
        }
        int32_t n = k;
        if (NEXT(n) == k)
            NEXT(n) = -1;
        k = NEXT(n);
        if ((P(uint16_t, VEC(n), 0xa) >> 1 & 1) && !all) {
            if (kept <= 0)
                HEAD(u) = (int16_t)n;
            else
                vector_link(kept, n, 1);
            kept = n;
            k = NEXT(n);
            if (move_cursor) {
                CURSOR(u) = (int16_t)n;
                move_cursor = 0;
            }
            continue;
        }
        if (at > 0 && (!from_list || passed))
            vector_link(at, k, 0);
        else
            HEAD(u) = (int16_t)k;
        if (CURSOR(u) == n)
            move_cursor = 1;
        if (I(int32_t, 0xe113)) {
            int32_t x = P(int16_t, VEC(n), 4), y = P(int16_t, VEC(n), 6), a, b;
            int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
            if (!(P(uint16_t, VEC(n), 0xa) >> 9 & 1))
                occ_lower(cell, 1);
        }
        node_free(n);
    }
    if (HEAD(u) > 0 && P(int8_t, VEC(HEAD(u)), 8) <= 0)
        HEAD(u) = -1;
    if (move_cursor)
        CURSOR(u) = HEAD(u);
    if (HEAD(u) <= 0)
        CURSOR(u) = -1;
    return 1;
}
