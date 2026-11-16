/* The game logic, 0x100147d0-0x10016430: scouting. Where a scouting unit goes next (the
 * corner of the map, of the three other quarters, its player has seen least of, by a
 * sector on the way it can reach), and the Recon unit's double click (a sector still in
 * fog). */
#include "iso_core.h"
#include "recon.h"

/* not rewritten yet (names ours, provisional) */
#define sector_reachable ORIG(int(__cdecl *)(int, uint32_t, int, uint32_t), 0x1001c62c)
#define IsoGetSectorCoordinates ORIG(void(__stdcall *)(uint8_t *, int, int *, int *), 0x1001bce0)
#define direction_to ORIG(int(__cdecl *)(int16_t, int16_t, int16_t, int16_t), 0x1009188a)

#define U_SECTOR 0x1a1
#define SECTORS_W I(uint16_t, 0xc2fd)
#define SECTORS_H I(uint16_t, 0xc2ef)

/* 0x100147d0: the sector to scout next, or -1 (mode 0; other modes were never finished
 * and give -1) */
int __cdecl recon_next_sector(uint32_t id, int16_t mode)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int quarter = P(int16_t, u, U_X) < 1 ? (P(int16_t, u, U_Y) < 1 ? 0 : 2) : (P(int16_t, u, U_Y) < 1 ? 1 : 3);
    DebugStr((const char *)ORIG_PTR(0x10118fc4), quarter);
    if (mode)
        return -1;
    const uint8_t *others = (const uint8_t *)ORIG_PTR(0x10118fb8) + quarter * 3; /* the other three */
    uint32_t most = 0, target = 0xffffffff;
    uint8_t pick = 0;
    for (int i = 0; i < 3; i++) {
        uint32_t q = others[i];
        uint32_t left = (uint32_t)quarter_unseen(q, P(uint8_t, u, U_PLAYER)) & 0xffff;
        DebugStr((const char *)ORIG_PTR(0x10118fd4), q, left);
        if ((int)most < (int)left) {
            most = left;
            target = q;
            pick = (uint8_t)i;
            DebugStr((const char *)ORIG_PTR(0x10118ff0), q);
        }
    }
    int turned = 0;
    for (;;) {
        int s;
        switch (target) {
        case 0: s = 0; break;
        case 1: s = SECTORS_W - 1; break;
        case 2: s = (SECTORS_H - 1) * SECTORS_W; break;
        case 3: s = SECTORS_W * SECTORS_H - 1; break;
        default:
            DebugStr((const char *)ORIG_PTR(0x10119000));
            return -1;
        }
        DebugStr((const char *)ORIG_PTR(0x10119014), id & 0xffff, target, s);
        /* from the corner towards the unit until a sector it can reach */
        int found = 0, tries = 0;
        while (!found && P(int16_t, u, U_SECTOR) != s && s != -1 && tries < (SECTORS_W >> 1)) {
            found = sector_reachable(P(int16_t, u, U_X), (uint32_t)(int)P(int16_t, u, U_Y), s, id);
            if (!found) {
                int sx, sy;
                IsoGetSectorCoordinates(g_iso, s, &sx, &sy);
                int d = direction_to((int16_t)sx, (int16_t)sy, P(int16_t, u, U_X), P(int16_t, u, U_Y));
                s = P(int16_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 6 + d * 2);
                tries++;
            }
        }
        if (!found && (SECTORS_W >> 1) <= tries)
            s = P(int16_t, u, U_SECTOR);
        if (P(int16_t, u, U_SECTOR) != s && s != -1)
            return s;
        /* the next of the three, then back down */
        if (pick < 2 && !turned) {
            pick++;
            target = others[pick];
            if (pick == 2)
                turned = 1;
        } else {
            if (!turned || !pick)
                return -1;
            pick--;
            target = others[pick];
        }
    }
}

/* ---- the Recon unit's double click: scouting a sector still in fog ---- */

#define orig_srand ORIG(void(__cdecl *)(unsigned), 0x100f4d60)
#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define orig_time ORIG(int32_t(__cdecl *)(int32_t *), 0x100f4df0)
#define orig_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)
#define on_map_px ORIG(int(__cdecl *)(int, int), 0x100d2a70)
#define ignores_marks ORIG(int(__cdecl *)(uint8_t *), 0x100d26fb)
#define IsoSnapTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, int *, int *, int *), 0x100aeebb)
#define order_move_to                                                                        \
    ORIG(int(__cdecl *)(uint32_t, int, int, int, int, int, int, int, int, int), 0x100b4ba4)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, char), 0x1009ce9f)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)

#define SEC(s) (I(uint8_t *, 0xc2eb) + (s) * 0x1e)
#define S_NEIGHBOUR 6   /* 8 shorts */
#define S_MARKED 0x1b   /* players whose scouts were sent here and could not go */
#define S_SEEN 0x1c     /* players who have seen it */
#define S_BLOCKED 0x1d  /* never scouted (water and the like) */
#define SECTORS I(int32_t, 0xc2f1)
#define SECTOR_SHIFT (I(uint8_t, 0xc2f5) & 0x1f)
#define U_BOX 0x38a     /* shorts: the area it scouts, x0 x1 y0 y1 */
#define U_BOX_ALL 0x392 /* byte: the whole map */
/* eight steps of a third of a cell: (dx, dy) shorts */
#define STEP_X(k) G(int16_t, 0x10118f98 + (k) * 4)
#define STEP_Y(k) G(int16_t, 0x10118f9a + (k) * 4)

/* 0x100150a9: no way through (off the map, or the cell marked 1 or 2) */
int __cdecl cell_blocked(int x, uint32_t y)
{
    if (!on_map_px(x, (int)y))
        return 1;
    int tx = x, ty = (int)y, a, b;
    uint32_t c = I(uint32_t *, 0x1934)[IsoVTilePos(g_iso, &tx, &ty, &a, &b)];
    return (c & 1) || (c >> 1 & 1);
}

/* 0x10014d9e: the unit's goal (+0x70) in the sector: its middle, else a free spot around
 * it; 0 if none. Original: a range check on the sector number never fires (it tests a
 * 16-bit value for being negative). */
int __cdecl recon_target_in_sector(uint32_t id, uint32_t sector)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int x, y, ox, oy;
    IsoGetSectorCoordinates(g_iso, (int)(sector & 0xffff), &x, &y);
    IsoVTilePos(g_iso, &x, &y, &ox, &oy);
    x = ox;
    y = oy;
    IsoVTilePos(g_iso, &x, &y, &ox, &oy);
    int px = ox, py = oy;
    int16_t tx = (int16_t)ox, ty = (int16_t)oy;
    if (cell_blocked(tx, (uint32_t)(int)ty) == 1) {
        int found = 0;
        for (int k = 0; k < 8 && !found; k++) {
            tx = (int16_t)(STEP_X(k) + (int16_t)px);
            ty = (int16_t)(STEP_Y(k) + (int16_t)py);
            found = !cell_blocked(tx, (uint32_t)(int)ty);
        }
        for (int k = 0; k < 8 && !found; k++) {
            tx = (int16_t)(STEP_X(k) * 2 + (int16_t)px);
            ty = (int16_t)(STEP_Y(k) * 2 + (int16_t)py);
            found = !cell_blocked(tx, (uint32_t)(int)ty);
        }
        if (!found) {
            /* round a square from its corner */
            px -= 0x60;
            py -= 0x30;
            for (int k = 0; k < 8 && !found; k += 2) {
                for (int m = 0; m < 3 && !found; m++) {
                    tx = (int16_t)(STEP_X(k) * m + (int16_t)px);
                    ty = (int16_t)(STEP_Y(k) * m + (int16_t)py);
                    found = !cell_blocked(tx, (uint32_t)(int)ty);
                }
                if (!found) {
                    px = STEP_X(k) + tx;
                    py = STEP_Y(k) + ty;
                }
            }
        }
        if (!found)
            return 0;
    }
    P(int16_t, u, 0x70) = tx;
    P(int16_t, u, 0x72) = ty;
    return 1;
}

/* 0x10015a94: whether the player (in multiplayer: or one of its allies) has seen the
 * sector */
uint32_t __cdecl sector_seen_by(uint8_t *s, int16_t player)
{
    uint32_t seen = 0;
    if (!I(int, 0x175)) {
        seen = P(uint8_t, s, S_SEEN) & (1u << (player & 0x1f));
    } else {
        for (int c = 0; c < I(int32_t, 0x1b1f); c++)
            if ((I(uint8_t, player * ISO_PLAYER_SIZE + 0x1ba0) & (uint8_t)(1u << (c & 0x1f))) &&
                (seen = P(uint8_t, s, S_SEEN) & (1u << (c & 0x1f))))
                break;
    }
    return seen ? 1 : 0;
}

/* 0x10015c29: whether a straight line of cells from `from` to `to` (tile positions) is
 * clear */
int __cdecl tile_line_clear(int16_t *to, int16_t *from)
{
    int dx = to[0] - from[0], dy = to[1] - from[1];
    int ax = (dx < 0 ? -dx : dx), ay = (dy < 0 ? -dy : dy);
    int ax2 = ax * 2, ay2 = ay * 2, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int x = from[0], y = from[1], err;
    uint32_t c;
    if (ay2 < ax2) {
        err = -(ax2 >> 1);
        for (;;) {
            err += ay2;
            c = I(uint32_t *, 0x1934)[((uint32_t)y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x];
            if ((c & 1) || (c >> 1 & 1))
                return 0;
            if (to[0] == x)
                return 1;
            if (err >= 0) {
                y += sy;
                err -= ax * 2;
            }
            x += sx;
        }
    }
    err = -(ay2 >> 1);
    for (;;) {
        err += ax2;
        c = I(uint32_t *, 0x1934)[((uint32_t)y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x];
        if ((c & 1) || (c >> 1 & 1))
            return 0;
        if (to[1] == y)
            return 1;
        if (err >= 0) {
            x += sx;
            err -= ay * 2;
        }
        y += sy;
    }
}

/* 0x10015b7e: whether the way from sector a's middle to sector b's is clear */
int __cdecl sector_line_clear(int16_t a, int16_t b)
{
    int x, y, ox, oy;
    int16_t from[2], to[2];
    IsoGetSectorCoordinates(g_iso, a, &x, &y);
    IsoVTilePos(g_iso, &x, &y, &ox, &oy);
    from[0] = (int16_t)x;
    from[1] = (int16_t)y;
    IsoGetSectorCoordinates(g_iso, b, &x, &y);
    IsoSnapTilePos(g_iso, &x, &y, &ox, &oy);
    to[0] = (int16_t)x;
    to[1] = (int16_t)y;
    return tile_line_clear(from, to);
}

/* a sector the unit may scout: in its area (unless anywhere), open, unseen by its side and
 * not marked by its player */
static int scoutable(uint8_t *u, int s, int check_box)
{
    int sy = s >> SECTOR_SHIFT, sx = (I(uint16_t, 0xc2fd) - 1) & s;
    uint8_t *rec = SEC(s);
    if (check_box && !(sx >= P(int16_t, u, U_BOX) && sx <= P(int16_t, u, U_BOX + 2) && sy >= P(int16_t, u, U_BOX + 4) &&
                       sy <= P(int16_t, u, U_BOX + 6)))
        return 0;
    if (P(uint8_t, rec, S_BLOCKED) || sector_seen_by(rec, P(uint8_t, u, U_PLAYER)))
        return 0;
    return ignores_marks(u) || !((1u << (P(uint8_t, u, U_PLAYER) & 0x1f)) & P(uint8_t, rec, S_MARKED));
}

/* a neighbouring sector of `from` to scout, reachable in a straight line */
static int neighbour_ok(uint8_t *u, int from, int n)
{
    if (n < 0 || n >= SECTORS)
        return 0;
    if (!scoutable(u, n, P(uint8_t, u, U_BOX_ALL) != 1))
        return 0;
    return sector_line_clear((int16_t)from, (int16_t)n) == 1;
}

/* 0x100153a1: the sector a Recon unit scouts next: a random neighbour still in fog, else
 * the nearest such sector in its area, else (once) anywhere. The randomness is the
 * original's C library rand() seeded with the wall clock (time()): not repeatable, and
 * in multiplayer each machine draws differently (kept). */
int16_t __cdecl find_fogged_sector(uint8_t *u)
{
    int s0 = P(int16_t, u, 0x1a1);
    if (s0 < 0 || s0 >= SECTORS)
        s0 = ((P(int16_t, u, U_Y) + I(int32_t, 0xd71e)) >> 8 << SECTOR_SHIFT) + ((P(int16_t, u, U_X) + I(int32_t, 0xd709)) >> 8);
    uint8_t *rec = SEC(s0);
    orig_srand((unsigned)orig_time(NULL));
    int16_t k = (int16_t)(orig_rand() % 8);
    int16_t n = P(int16_t, rec, S_NEIGHBOUR + k * 2);
    if (neighbour_ok(u, s0, n))
        return n;
    int16_t start = (int16_t)(orig_rand() % 8);
    for (int16_t i = start; i < start + 7; i++) {
        n = P(int16_t, rec, S_NEIGHBOUR + (int16_t)((i + k) % 8) * 2);
        if (neighbour_ok(u, s0, n))
            return n;
    }
    /* the nearest */
    float best = 1e20f;
    int16_t pick = -1;
    int cy = s0 >> SECTOR_SHIFT, cx = (I(uint16_t, 0xc2fd) - 1) & s0;
    for (int16_t s = 0; s < SECTORS; s++) {
        if (!scoutable(u, s, 1))
            continue;
        int sy = s >> SECTOR_SHIFT, sx = (I(uint16_t, 0xc2fd) - 1) & s;
        long double d = orig_sqrt((double)((cx - sx) * (cx - sx) + (cy - sy) * (cy - sy)));
        if (d < (long double)best) {
            pick = s;
            best = (float)d;
        }
    }
    if (pick == -1 && !P(uint8_t, u, U_BOX_ALL)) {
        /* nothing left in its area: the whole map from now on */
        P(int16_t, u, U_BOX) = 0;
        P(int16_t, u, U_BOX + 2) = (int16_t)(I(uint16_t, 0xc2fd) - 1);
        P(int16_t, u, U_BOX + 4) = 0;
        P(int16_t, u, U_BOX + 6) = (int16_t)(I(uint16_t, 0xc2ef) - 1);
        P(uint8_t, u, U_BOX_ALL) = 1;
        for (int16_t s = 0; s < SECTORS; s++) {
            uint8_t *r = SEC(s);
            if (P(uint8_t, r, S_BLOCKED) || sector_seen_by(r, P(uint8_t, u, U_PLAYER)))
                continue;
            int sy = s >> SECTOR_SHIFT, sx = (I(uint16_t, 0xc2fd) - 1) & s;
            long double d = orig_sqrt((double)((cx - sx) * (cx - sx) + (cy - sy) * (cy - sy)));
            if (d < (long double)best) {
                pick = s;
                best = (float)d;
            }
        }
    }
    return pick;
}

/* 0x10015137: the Recon unit's double click (standing still): it sets off to a sector in
 * fog; a sector it cannot reach is marked so its player's scouts skip it */
int __cdecl recon_double_click(uint32_t id)
{
    DebugStr((const char *)ORIG_PTR(0x1011904c));
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (P(uint8_t, u, 0x147))
        return 1;
    int s = find_fogged_sector(u);
    uint8_t bit = (uint8_t)(1u << (P(uint8_t, u, U_PLAYER) & 0x1f));
    if (s < 0) {
        DebugStr((const char *)ORIG_PTR(0x10119060));
        unit_set_mode(id, 0x1e);
        unit_set_action(id, 0x1e, 1, 0);
        return 1;
    }
    if (recon_target_in_sector(id, (uint32_t)s) == 1) {
        P(int16_t, u, 0x74) = P(int16_t, u, 0x70);
        P(int16_t, u, 0x76) = P(int16_t, u, 0x72);
        if (order_move_to(id, P(uint16_t, u, 0x70), P(uint16_t, u, 0x72), 2, 0, 0, 1, 0, 0, 1) < 1) {
            if (P(int16_t, u, 0x119) < 1) {
                P(uint8_t, SEC(s), S_MARKED) |= bit;
            } else {
                /* original: checks the order at +0x11b although +0x119 was tested */
                uint8_t *o = I(uint8_t *, 0x1984) + P(int16_t, u, 0x11b) * 0x12;
                int dx = P(int16_t, u, U_X) - P(int16_t, o, 4), dy = P(int16_t, u, U_Y) - P(int16_t, o, 6);
                if ((dx < 0 ? -dx : dx) < 0x10 && (dy < 0 ? -dy : dy) < 8)
                    P(uint8_t, SEC(s), S_MARKED) |= bit;
            }
        }
    } else {
        P(uint8_t, SEC(s), S_MARKED) |= bit;
    }
    unit_set_mode(id, ')');
    unit_set_action(id, 0x29, 1, 0);
    return 1;
}

/* 0x10015e1a */
void __cdecl recon_double_click_x(uint32_t id) { recon_double_click(id); }

/* 0x10015e36: the share of open sectors the player has not seen, in percent (0 when
 * there are none) */
int16_t __cdecl percent_unseen(int16_t player)
{
    int open = 0, seen = 0;
    for (int s = 0; s < SECTORS; s++)
        if (!P(uint8_t, SEC(s), S_BLOCKED)) {
            open++;
            if (sector_seen_by(SEC(s), player) == 1)
                seen++;
        }
    if (open < 1)
        return 0;
    return (int16_t)((open - seen) * 100 / open);
}

/* 0x10015efc: the share of a quarter of the map (0-3) the player has not seen, in
 * percent; 100 when it has no open sectors (original: a quarter number over 3 gives 0) */
uint16_t __cdecl quarter_unseen(uint32_t quarter, int16_t player)
{
    int w = I(uint16_t, 0xc2fd), h = I(uint16_t, 0xc2ef), x0 = 0, y0 = 0, x1 = w, y1 = h;
    if (quarter >= 4)
        return 0;
    switch (quarter) {
    case 0: x1 = w >> 1; y1 = h >> 1; break;
    case 1: x0 = w >> 1; y1 = h >> 1; break;
    case 2: x1 = w >> 1; y0 = h >> 1; break;
    case 3: x0 = w >> 1; y0 = h >> 1; break;
    }
    int open = 0, seen = 0;
    for (int s = 0; s < SECTORS; s++) {
        int sy = s >> SECTOR_SHIFT, sx = (w - 1) & s;
        if (sx >= x0 && sx < x1 && sy >= y0 && sy < y1 && !P(uint8_t, SEC(s), S_BLOCKED)) {
            open++;
            if (sector_seen_by(SEC(s), player) == 1)
                seen++;
        }
    }
    DebugStr((const char *)ORIG_PTR(0x10119080), open, seen);
    if (!open)
        return 100;
    return (uint16_t)((open - seen) * 100 / open);
}

/* 0x10016153: a scout ordered to the sectors around `sector` (`radius` sectors each way):
 * that is its area; it heads for the sector's middle; marked if it cannot go */
void __cdecl recon_scout_area(uint32_t id, uint32_t sector, int radius)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int sy = (int)sector >> SECTOR_SHIFT, sx = (I(uint16_t, 0xc2fd) - 1) & (int)sector;
    int maxx = I(uint16_t, 0xc2fd) - 1, maxy = I(uint16_t, 0xc2ef) - 1;
    P(int16_t, u, U_BOX) = (int16_t)(sx - radius < 1 ? 0 : sx - radius);
    P(int16_t, u, U_BOX + 2) = (int16_t)(sx + radius <= maxx ? sx + radius : maxx);
    P(int16_t, u, U_BOX + 4) = (int16_t)(sy - radius < 1 ? 0 : sy - radius);
    P(int16_t, u, U_BOX + 6) = (int16_t)(sy + radius <= maxy ? sy + radius : maxy);
    P(uint8_t, u, U_BOX_ALL) = 0;
    recon_target_in_sector(id, sector);
    P(int16_t, u, 0x74) = P(int16_t, u, 0x70);
    P(int16_t, u, 0x76) = P(int16_t, u, 0x72);
    if (!order_move_to(id, P(uint16_t, u, 0x70), P(uint16_t, u, 0x72), 2, 0, 0, 1, 0, 0, 1) || P(int16_t, u, 0x113) < 1)
        P(uint8_t, SEC(sector), S_MARKED) |= (uint8_t)(1u << (P(uint8_t, u, U_PLAYER) & 0x1f));
    unit_set_mode(id, ')');
    unit_set_action(id, 0x29, 1, 0);
}

/* 0x10016302: the local player's scouts forget the marked sectors and scout again */
void __cdecl recon_rescout(void)
{
    for (int s = 0; s < SECTORS; s++)
        P(uint8_t, SEC(s), S_MARKED) &= (uint8_t)~(1u << (I(uint8_t, ISO_LOCAL_PLAYER) & 0x1f));
    int n = I(int, ISO_UNIT_COUNT);
    for (int i = 0; i < n; i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[v];
        if (u && P(int32_t, u, U_ID_TYPE) > 0x3b && P(int32_t, u, U_ID_TYPE) < 0x40) {
            unit_set_mode(v, ')');
            unit_set_action(v, 0x29, 1, 0);
        }
    }
}
