/* The game logic, 0x100c70bc-0x100c8773: the path finder. It searches the map cells in a
 * rotated grid (one step = one of 8 neighbours, 0x100c9a00 steps round them) from the
 * goal outwards (the cost of each cell in the grid 0x1017e414, the open list 0x1017e3f8
 * with its costs 0x1017e3fc, the current cell 0x1017e408/40c, the original's globals),
 * until it reaches the start; then it walks back down the costs to the goal, notes each
 * step's direction, and turns every change of direction into a waypoint vector. */
#include "iso_core.h"
#include "pathfind.h"

#define MSG ((char *)(g_iso + 0xdea))
#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define GRID G(uint16_t *, 0x1017e414)
#define OPEN G(int16_t *, 0x1017e3f8) /* 10 bytes each: +0/+2 the cell, +4/+6 rotated, +8 dir */
#define OPEN_COST G(uint16_t *, 0x1017e3fc)
#define STEPS G(uint8_t *, 0x1017e400) /* 0x12 bytes each: +4/+6 the point, +9 the direction */
#define CX G(int32_t, 0x1017e408)
#define CY G(int32_t, 0x1017e40c)
#define DIRS ((int32_t *)ORIG_PTR(0x1017e418))
#define NOPEN G(int32_t, 0x10158d68)
#define FOUND G(int32_t, 0x10158d6c)
#define BEST G(int32_t, 0x10158d70)
#define DIR G(int32_t, 0x10158d74)
#define SHIFT (I(uint8_t, 0x1aa0))
#define debug_sprite_show ORIG(void(__cdecl *)(int), 0x10005bb1)
#define debug_sprite_hide ORIG(void(__cdecl *)(int), 0x10005bf1)
#define occ_get ORIG(uint32_t(__cdecl *)(int32_t), 0x100672b3)
#define vector_new ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)
#define pf_step ORIG(void(__cdecl *)(void), 0x100c9a00)
#define pf_cost ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c9a50)
#define pf_rotate ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100c9aa0)
#define pf_blocked ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, uint32_t), 0x100c9ae0)
#define pf_fits ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t, int32_t), 0x100c8773)
#define pf_goal_near ORIG(int32_t(__cdecl *)(int32_t *, int32_t *), 0x100c883f)

static int32_t fail(int32_t code)
{
    if (I(uint8_t, 0xeeb) & 0x20)
        debug_sprite_hide(1);
    I(int32_t, 0x1b59) = code;
    return code;
}

/* a message for the debug output: the format with the unit, then a line end */
static void report(uint32_t fmt, uint32_t unit, uint32_t tail)
{
    wsprintfA(MSG, (const char *)ORIG_PTR(fmt), unit & 0xffff);
    lstrcatA(MSG, (const char *)ORIG_PTR(tail));
    OutputDebugStringA(MSG);
}

static int32_t dist2(int16_t ax, int16_t ay, int16_t bx, int16_t by)
{
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

/* 0x100c70bc: finds a way from (x0, y0) to (x1, y1) for unit `unit` (0: none) and turns it
 * into waypoint vectors, the first in *head. The number of waypoints; -10 without the
 * buffers, -0x32 when both points are on the same cell, -0x1e when the goal is blocked
 * (and no other cell near it will do) or no way exists, -0x14 when the search or the way
 * grows too long or no vector is free, -0x28 without `head`, -0x3c for a cell out of
 * range. The code is also kept at iso+0x1b59. */
int32_t __cdecl path_find(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int16_t *head, int32_t a6, uint32_t unit,
                          int32_t a8)
{
    (void)a6;
    (void)a8;
    int32_t gx = x1, gy = y1, sx = x0, sy = y0;
    int32_t n = 0, goal_cell, tx, ty, wx, wy, cell, gcx, gcy, scx, scy;
    /* the start and goal cells as {column, row} pairs: 0x100c883f takes them that way */
    int32_t st[2], gt[2];
#define stx st[0]
#define sty st[1]
#define gtx gt[0]
#define gty gt[1]
    int32_t cx, cy, i, dir = 0, last = 0;
    /* (the debug print "GameClock: ..." is compiled out) */
    if (I(uint8_t, 0xeeb) & 0x20)
        debug_sprite_show(1);
    I(int32_t, 0x1b59) = 0;
    if (!G(void *, 0x1017e414) || !G(void *, 0x1017e3f8) || !G(void *, 0x1017e400))
        return fail(-10);
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (unit & 0xffff)
        P(int16_t, u, 0x370) = -1;
    tx = gx;
    ty = gy;
    goal_cell = IsoVTilePos(g_iso, &tx, &ty, &wx, &wy);
    gtx = tx;
    gty = ty;
    tx = sx;
    ty = sy;
    IsoVTilePos(g_iso, &tx, &ty, &wx, &wy);
    stx = tx;
    sty = ty;
    gx += I(int32_t, 0xd6f1) >> 1;
    gy += I(int32_t, 0xd6f5) >> 1;
    sx += I(int32_t, 0xd6f1) >> 1;
    sy += I(int32_t, 0xd6f5) >> 1;
    (void)gx;
    (void)gy;
    if (stx == gtx && sty == gty) {
        report(0x10158ef8, unit, 0x10158f3c);
        return fail(-0x32);
    }
    if (pf_blocked(goal_cell, gtx, gty, unit)) {
        if ((unit & 0xffff) && I(uint8_t **, ISO_UNITS)[unit & 0xffff]) {
            if (!pf_goal_near(st, gt))
                return -0x1e;
        } else {
            I(int32_t, 0x1b59) = -0x1e;
            return -0x1e;
        }
    }
    gcx = pf_rotate(gtx, gty);
    gcy = (gty >> 1) - gtx;
    scx = pf_rotate(stx, sty);
    scy = (sty >> 1) - stx;
    cell = ((scy + scx) << SHIFT) + ((scx - scy) >> 1);
    memset(GRID, 0xff, I(int32_t, 0x17fa) * I(int32_t, 0x17fe) * 2);
    memset(OPEN, 0, 0x4e20);
    memset(OPEN_COST, 0xff, 0xfa0);
    NOPEN = 1;
    OPEN[0] = (int16_t)gtx;
    OPEN[1] = (int16_t)gty;
    OPEN[2] = (int16_t)gcx;
    OPEN[3] = (int16_t)gcy;
    OPEN_COST[0] = (uint16_t)pf_cost(gcx, gcy, scx, scy);
    GRID[I(int32_t, 0x17fe) * gty + gtx] = 0;

    /* the search, from the goal until the start is reached */
    int32_t stuck = 0;
    FOUND = 0;
    while (!FOUND && stuck != 1) {
        if (NOPEN >= 0x7d0) {
            report(0x10158f40, unit, 0x10158f70);
            return fail(-0x14);
        }
        BEST = 0;
        for (i = 0; i < NOPEN - 1; i++)
            if (OPEN_COST[i] < OPEN_COST[BEST])
                BEST = i;
        cx = OPEN[BEST * 5 + 2];
        cy = OPEN[BEST * 5 + 3];
        int16_t *lastrec = OPEN + (NOPEN - 1) * 5, *best = OPEN + BEST * 5;
        for (i = 0; i < 5; i++)
            best[i] = lastrec[i];
        OPEN_COST[BEST] = OPEN_COST[NOPEN - 1];
        NOPEN--;
        if ((unit & 0xffff) && (P(uint16_t, u, 0x1cd) >> 12 & 1) && !pf_fits(scx, scy, cx, cy)) {
            if (!NOPEN)
                stuck = 1;
            continue;
        }
        CX = cx;
        CY = cy;
        DIR = 5;
        CY--;
        CX--;
        for (i = 0; i < 8; i++) {
            cell = ((CY + CX) << SHIFT) + ((CX - CY) >> 1);
            if (!pf_blocked(cell, (CX - CY) >> 1, CY + CX, unit)) {
                int32_t at = ((CY + CX) << (SHIFT + 1)) + ((CX - CY) >> 1);
                if (at < 0) {
                    wsprintfA(MSG, (const char *)ORIG_PTR(0x10158f74), unit & 0xffff);
                    wsprintfA(MSG, (const char *)ORIG_PTR(0x10158fa4), at, cell, CX, CY);
                    /* (then to the compiled-out debug log) */
                    return fail(-0x3c);
                }
                if (GRID[at] == 0xffff) {
                    GRID[at] = (uint16_t)(GRID[((cy + cx) << (SHIFT + 1)) + ((cx - cy) >> 1)] +
                                          G(int32_t, 0x10158ce8 + DIR * 4));
                    int16_t *rec = OPEN + NOPEN * 5;
                    rec[2] = (int16_t)CX;
                    rec[3] = (int16_t)CY;
                    rec[0] = (int16_t)((CX - CY) >> 1);
                    rec[1] = (int16_t)(CY + CX);
                    rec[4] = (int16_t)DIR;
                    /* (the original notes a cost of 0xffff in a local it never reads) */
                    int32_t cost = pf_cost(CX, CY, scx, scy);
                    OPEN_COST[NOPEN] = (uint16_t)((int16_t)occ_get(cell) + cost);
                    NOPEN++;
                }
            }
            if (CX == scx && CY == scy) {
                FOUND = 1;
                break;
            }
            pf_step();
        }
        if (!NOPEN)
            stuck = 1;
    }
    if (FOUND != 1)
        return fail(-0x1e);

    /* back down the costs from the start to the goal, noting each step's direction */
    n = 0;
    cx = scx;
    cy = scy;
    while (!(cx == gcx && cy == gcy) && n < 0x7d0) {
        CX = cx;
        CY = cy;
        int32_t bx = cx, by = cy;
        uint16_t low = 0xffff;
        DIR = 6;
        CY--;
        for (i = 0; i < 8; i++) {
            int32_t col = (CX - CY) >> 1, row = CY + CX;
            if (col > 0 && col < I(int32_t, 0x1a9c) - 1 && row > 0 && row < I(int32_t, 0x1a98) - 1) {
                int32_t at = (row << (SHIFT + 1)) + col;
                if (GRID[at] < low) {
                    low = GRID[at];
                    bx = CX;
                    by = CY;
                    dir = DIR;
                }
            }
            pf_step();
        }
        CX = bx;
        CY = by;
        cx = CX;
        cy = CY;
        DIRS[n] = G(int32_t, 0x10158c88 + dir * 4);
        n++;
    }
    if (!head)
        return fail(-0x28);
    cx = x0 + (I(int32_t, 0xd6f1) >> 1);
    cy = y0 + (I(int32_t, 0xd6f5) >> 1);
    for (i = 0; i < n; i++) {
        int32_t d = DIRS[i] < 7 ? DIRS[i] + 1 : 0;
        cx += G(int32_t, 0x10158ca8 + d * 8);
        cy += G(int32_t, 0x10158cac + d * 8);
        P(int16_t, STEPS + i * 0x12, 4) = (int16_t)(cx - (I(int32_t, 0xd6f1) >> 1));
        P(int16_t, STEPS + i * 0x12, 6) = (int16_t)(cy - (I(int32_t, 0xd6f5) >> 1));
        P(uint8_t, STEPS + i * 0x12, 9) = (uint8_t)d;
    }
    if (n == 0x7d0) {
        report(0x10158fe4, unit, 0x10159014);
        return fail(-0x14);
    }

    /* a waypoint at every change of direction, and at the end */
    int32_t made = 0;
    for (i = 0; i < n; i++) {
        if (P(uint8_t, STEPS + (i + 1) * 0x12, 9) == P(uint8_t, STEPS + i * 0x12, 9) && i != n - 1)
            continue;
        uint8_t *v = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
        uint32_t type = v ? P(uint32_t, v, 0xc) : 0;
        uint32_t player = v ? P(uint8_t, I(uint8_t **, ISO_UNITS)[unit & 0xffff], 0x14) : 0;
        int32_t k = vector_new((type & 0xffff0000) | (player & 0xff), type);
        if (k < 0) {
            lstrcpyA(MSG, (const char *)ORIG_PTR(0x10159018));
            lstrcatA(MSG, (const char *)ORIG_PTR(0x10159038));
            OutputDebugStringA(MSG);
            return fail(-0x14);
        }
        P(int16_t, VEC(k), 4) = P(int16_t, STEPS + i * 0x12, 4);
        P(int16_t, VEC(k), 6) = P(int16_t, STEPS + i * 0x12, 6);
        P(uint8_t, VEC(k), 9) = P(uint8_t, STEPS + i * 0x12, 9);
        if (!made)
            *head = (int16_t)k;
        else
            P(int16_t, VEC(last), 0xe) = (int16_t)k;
        last = k;
        made++;
    }
    n = made;

    /* (only without a map, +0xe113) how far along the way the unit gets on its range
     * (+0x1d3, a float), and the first corner it cannot cut, into +0x370 */
    int32_t hold = u && (P(uint16_t, u, 0x1cd) >> 12 & 1);
    if (!I(int32_t, 0xe113) && !hold && (unit & 0xffff) && n > 2) {
        int16_t k = *head, prev = 0;
        float range = P(float, u, 0x1d3);
        volatile float run = 0.0f; /* kept in memory as a float, as the original does */
        int16_t ax = P(int16_t, u, 0x52), ay = P(int16_t, u, 0x54);
        int16_t bx = P(int16_t, VEC(k), 4), by = P(int16_t, VEC(k), 6);
        run = (float)(crt_sqrt((double)dist2(ax, ay, bx, by)) + (long double)run);
        k = P(int16_t, VEC(k), 0xe);
        ax = bx;
        ay = by;
        bx = P(int16_t, VEC(k), 4);
        by = P(int16_t, VEC(k), 6);
        run = (float)(crt_sqrt((double)dist2(ax, ay, bx, by)) + (long double)run);
        prev = k;
        k = P(int16_t, VEC(k), 0xe);
        while (k > 0) {
            ax = bx;
            ay = by;
            bx = P(int16_t, VEC(k), 4);
            by = P(int16_t, VEC(k), 6);
            long double sum = crt_sqrt((double)dist2(ax, ay, bx, by)) + (long double)run;
            int within = !(sum > (long double)range);
            run = (float)sum;
            if (!within)
                break;
            prev = k;
            k = P(int16_t, VEC(k), 0xe);
        }
        while (k > 0) {
            uint8_t d1 = P(uint8_t, VEC(prev), 9), d2 = P(uint8_t, VEC(k), 9), corner;
            if (!d1 || !d2) {
                if (!d1) {
                    d1 = 4;
                    corner = (uint8_t)(d2 < d1 ? d2 + 1 : d2 - 1);
                } else {
                    d2 = 4;
                    corner = (uint8_t)(d2 < d1 ? d1 - 5 : d1 + 5);
                }
            } else if (d2 < d1) {
                corner = (uint8_t)(d1 - d2 > 1 ? (d1 + 5) % 8 : (d1 + 6) % 8);
            } else {
                corner = (uint8_t)(d2 - d1 > 1 ? (d1 + 3) % 8 : (d1 + 2) % 8);
            }
            int cxx = P(int16_t, VEC(prev), 4) + G(int32_t, 0x10158ca8 + corner * 8);
            int cyy = P(int16_t, VEC(prev), 6) + G(int32_t, 0x10158cac + corner * 8);
            int e4, e8;
            int32_t c = IsoVTilePos(g_iso, &cxx, &cyy, &e4, &e8);
            uint32_t m = I(uint32_t *, 0x1934)[c];
            if ((m & 1) || (m >> 16 & 0xfff))
                break;
            prev = k;
            k = P(int16_t, VEC(k), 0xe);
        }
        if (k > 0)
            P(int16_t, u, 0x370) = prev;
    }
    if (I(uint8_t, 0xeeb) & 0x20)
        debug_sprite_hide(1);
    return n;
#undef stx
#undef sty
#undef gtx
#undef gty
}
