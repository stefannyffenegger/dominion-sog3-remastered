/* The game logic, 0x1001bd92-0x1001c7c0: sector-to-sector paths on top of the path
 * finder (0x100c70bc): freeing a found path, its length, whether one exists, how far
 * a straight run of sectors goes, and a sector's cell flags. */
#include "iso_core.h"
#include "sound.h"
#include "paths.h"

/* not rewritten yet */
/* finds a path from (x0, y0) to (x1, y1); *head gets its first node */
#define path_find ORIG(int(__cdecl *)(int, uint32_t, int, uint32_t, int16_t *, int, uint32_t), 0x100c70bc)
#define IsoSnapTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, uint32_t *, int *, int *), 0x100aeebb)
#define g_hypot ORIG(long double(__cdecl *)(double, double), 0x100f4fc0)

#define ISO_SECTOR_MAX 0xc2f1 /* the last sector's number */
#define ISO_PATH_NODES 0x1984 /* 0x12 bytes each: +4 x, +6 y (shorts), +8 users, +0xe next */
#define NODE(n, off) P(int16_t, I(uint8_t *, ISO_PATH_NODES) + (n) * 0x12, off)

static int valid_sector(int s) { return s >= 0 && s <= I(int32_t, ISO_SECTOR_MAX); }
static int32_t abs32(int32_t v) { return v < 0 ? -v : v; }

/* 0x1001bd92: releases the path from node `n` on: nodes no one else uses go back to
 * the free list */
void __cdecl path_free(int n)
{
    while (n >= 1) {
        uint8_t *node = I(uint8_t *, ISO_PATH_NODES) + n * 0x12;
        P(int8_t, node, 8)--;
        int next = P(int16_t, node, 0xe);
        if (P(int8_t, node, 8) < 1) {
            P(int16_t, node, 0xe) = (int16_t)I(int32_t, 0x1ad9);
            I(int32_t, 0x1ad9) = n;
            I(int32_t, 0xd697)++;
        }
        n = next;
        if (next == -1)
            break;
    }
}

/* 0x1001be66: from sector `s`, the farthest sector in direction `dir` (0-3) reached by
 * a straight path (0: within 32 pixels up or down, 2: 64 pixels sideways, 1 and 3: at
 * most two nodes); -1 when not even the neighbour is */
int __cdecl sector_run(int s, uint32_t dir)
{
    int x0 = 0, x1 = 0, count = 0;
    uint32_t y0 = 0, y1 = 0;
    int16_t head = 0;
    if (!valid_sector(s))
        return -1;
    int next = P(int16_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 6 + (dir & 0xff) * 2);
    if (!valid_sector(next))
        return -1;
    IsoGetSectorCoordinates(g_iso, s, &x0, (int *)&y0);
    IsoGetSectorCoordinates(g_iso, next, &x1, (int *)&y1);
    if (path_find(x0, y0, x1, y1, &head, 1, 0) < 0)
        return -1;
    /* the original also works out the slope here (as a float) and never uses it */
    for (int n = head; n > 0; n = NODE(n, 0xe)) {
        count++;
        switch (dir & 0xff) {
        case 0:
            if (abs32((int32_t)y0 - NODE(n, 6)) > 0x20)
                goto fail;
            break;
        case 1:
        case 3:
            if (count > 2)
                goto fail;
            break;
        case 2:
            if (abs32(x0 - NODE(n, 4)) > 0x40)
                goto fail;
            break;
        default:
            goto fail;
        }
    }
    path_free(head);
    {
        int r = sector_run(next, dir);
        return r == -1 ? next : r;
    }
fail:
    path_free(head);
    return -1;
}

/* the length of the path from (x, y) along the nodes from `head`, with the original's
 * hypot and truncation */
static int path_length_from(int x, int y, int head)
{
    int len = 0;
    for (int n = head; n > 0; n = NODE(n, 0xe)) {
        int dx = NODE(n, 4) - x, dy = NODE(n, 6) - y;
        len += (int32_t)(int64_t)g_hypot((double)dx, (double)dy);
        x = NODE(n, 4);
        y = NODE(n, 6);
    }
    return len;
}

/* 0x1001c158: the length of the path between two sectors' middles, or -1 */
int __cdecl sector_path_length(int from, int to)
{
    int x0 = 0, x1 = 0;
    uint32_t y0 = 0, y1 = 0;
    int16_t head = 0;
    if (!valid_sector(from) || !valid_sector(to))
        return -1;
    IsoGetSectorCoordinates(g_iso, from, &x0, (int *)&y0);
    IsoGetSectorCoordinates(g_iso, to, &x1, (int *)&y1);
    if (path_find(x0, y0, x1, y1, &head, 1, 0) < 0)
        return -1;
    int len = path_length_from(x0, (int)y0, head);
    path_free(head);
    return len;
}

/* 0x1001c354: the length of the path from (x, y) to sector `to`'s middle, or -1 */
int __cdecl point_path_length(int x, uint32_t y, int to, uint32_t flags)
{
    int x1 = 0;
    uint32_t y1 = 0;
    int16_t head = 0;
    if (!valid_sector(to))
        return -1;
    IsoGetSectorCoordinates(g_iso, to, &x1, (int *)&y1);
    if (path_find(x, y, x1, y1, &head, 1, flags) < 0)
        return -1;
    int len = path_length_from(x, (int)y, head);
    path_free(head);
    return len;
}

/* 0x1001c505: whether a path leads from sector `from` to sector `to` */
int __cdecl sector_path_exists(int from, int to)
{
    int x0 = 0, x1 = 0;
    uint32_t y0 = 0, y1 = 0;
    int16_t head = 0;
    if (!valid_sector(from)) {
        DebugStr((const char *)ORIG_PTR(0x10130eb0));
        return 0;
    }
    if (!valid_sector(to)) {
        DebugStr((const char *)ORIG_PTR(0x10130ec4));
        return 0;
    }
    IsoGetSectorCoordinates(g_iso, from, &x0, (int *)&y0);
    IsoGetSectorCoordinates(g_iso, to, &x1, (int *)&y1);
    int r = path_find(x0, y0, x1, y1, &head, 1, 0);
    if (r < 0) {
        DebugStr((const char *)ORIG_PTR(0x10130ed8), r);
        return 0;
    }
    path_free(head);
    return 1;
}

/* 0x1001c62c: whether a path leads from (x, y) to sector `to` */
int __cdecl point_path_exists(int x, uint32_t y, int to, uint32_t flags)
{
    int x1 = 0;
    uint32_t y1 = 0;
    int16_t head = 0;
    if (!valid_sector(to)) {
        DebugStr((const char *)ORIG_PTR(0x10130ef0));
        return 0;
    }
    IsoGetSectorCoordinates(g_iso, to, &x1, (int *)&y1);
    int r = path_find(x, y, x1, y1, &head, 1, flags);
    if (r < 0) {
        DebugStr((const char *)ORIG_PTR(0x10130f04), r);
        return 0;
    }
    path_free(head);
    return 1;
}

/* 0x1001c6fe: bit `bit` of the top byte of the map cell in sector `s`'s middle */
uint32_t __cdecl sector_cell_flag(int s, uint8_t bit)
{
    int x = -99999, a, b;
    uint32_t y = 0xfffe7961; /* -99999 */
    if (s <= 0)
        return 0;
    IsoGetSectorCoordinates(g_iso, s, &x, (int *)&y);
    if (x == -99999 || y == 0xfffe7961)
        return 0;
    int32_t cell = IsoSnapTilePos(g_iso, &x, &y, &a, &b);
    if (!cell)
        return 0;
    return (I(uint32_t *, ISO_MAP)[cell] >> 24) & (1u << (bit & 0x1f));
}
