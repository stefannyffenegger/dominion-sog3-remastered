/* The game logic, 0x100787e3-0x10079568: wall segments between two of a player's wall posts:
 * a record (0x86 bytes, at 0xe123, 0xe11f of them) with the two posts, the direction and
 * up to 20 cells with their wall pieces. */
#include "iso_core.h"
#include "display.h"
#include "damage.h"
#include "intfwave.h"
#include "lineofsight.h"
#include "power.h"
#include "walls.h"

#define STEP ((const int8_t *)ORIG_PTR(0x1013b468))
#define SEGS I(uint8_t *, 0xe123)
#define SEG_N I(int32_t, 0xe11f)

#define posts_too_close ORIG(int(__cdecl *)(uint8_t *, uint8_t *), 0x100d1e0f)
#define units_between ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *, int32_t *), 0x100d2083)
#define route_neighbour ORIG(uint32_t(__cdecl *)(int16_t, int16_t), 0x10077b2b)
#define route_split ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x10079c6c)
#define segment_find ORIG(uint32_t(__cdecl *)(int16_t, int16_t), 0x10079568)

static uint8_t *unit_ptr(int32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

/* 0x100787e3: whether a unit is a structure of types 0x40-0x4f */
int32_t __cdecl unit_is_post(int16_t unit)
{
    uint8_t *u = unit_ptr(unit);
    return u && P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x4f;
}

/* 0x10079430: a free segment record (in use 0, no posts), or a new one: the table grows by
 * 50. -1 if out of memory */
int16_t __cdecl wall_segment_alloc(void)
{
    int16_t k;
    for (k = 0; (int32_t)k < SEG_N; k++)
        if (!P(int32_t, SEGS, k * 0x86) && !P(int16_t, SEGS, k * 0x86 + 4) && !P(int16_t, SEGS, k * 0x86 + 6))
            return k;
    int32_t old = SEG_N;
    SEG_N += 0x32;
    SEGS = winrealloc(SEGS, (SIZE_T)(SEG_N * 0x86), 1);
    return SEGS ? (int16_t)old : -1;
}

/* 0x10078fa5: builds the wall segment from post `a` to post `b` along a clear straight line:
 * splits the posts' existing segments in that direction, reuses a matching one or takes a
 * new record, and fills in the cells with their wall pieces. The record, or -1 */
int16_t __cdecl wall_segment_build(uint32_t a, uint32_t b)
{
    if (!unit_fire_line_clear(a, b))
        return -1;
    uint8_t *ua = unit_ptr((int16_t)a), *ub = unit_ptr((int16_t)b);
    uint16_t dir = (uint16_t)unit_direction(a, b);
    uint16_t back = (uint16_t)(dir - 4) & 7;
    int16_t seg;
    if (unit_route_kind2((int16_t)a, (int16_t)dir) == 1 || unit_route_kind2((int16_t)b, (int16_t)back) == 1) {
        int16_t n = (int16_t)route_neighbour((int16_t)a, (int16_t)dir);
        if (n > 0 && (int16_t)b != n)
            route_split(a, (uint16_t)n, 1);
        n = (int16_t)route_neighbour((int16_t)b, (int16_t)back);
        if (n > 0 && (int16_t)a != n)
            route_split(b, (uint16_t)n, 1);
        seg = (int16_t)segment_find((int16_t)a, (int16_t)b);
        if (seg < 0 && (seg = wall_segment_alloc()) < 0)
            return -1;
    } else if ((seg = wall_segment_alloc()) < 0) {
        return -1;
    }
    uint8_t *r = SEGS + seg * 0x86;
    P(int16_t, r, 4) = (int16_t)a;
    P(int16_t, r, 6) = (int16_t)b;
    P(int32_t, r, 10) = 0;
    int x = P(int16_t, ua, 0x52), tx, ty;
    uint32_t y = (uint32_t)(int32_t)P(int16_t, ua, 0x54);
    IsoVTilePos(g_iso, &x, (int *)&y, &tx, &ty);
    int x2 = P(int16_t, ub, 0x52);
    uint32_t y2 = (uint32_t)(int32_t)P(int16_t, ub, 0x54);
    IsoVTilePos(g_iso, &x2, (int *)&y2, &tx, &ty);
    int32_t n = 0;
    int16_t d = (int16_t)dir;
    int32_t sx = STEP[d * 4 + (y & 1) * 2], sy = STEP[d * 4 + (y & 1) * 2 + 1];
    x += sx;
    y += sy;
    uint32_t owner = ((uint32_t)(uintptr_t)ua & 0xffff0000) | P(uint8_t, ua, 0x14);
    do {
        if (x2 == x && y2 == y)
            break;
        int16_t piece = wall_piece_at(x, y, dir, owner);
        if (piece < 0)
            break;
        r = SEGS + seg * 0x86;
        P(int16_t, r, 0xe + n * 6) = piece;
        P(int16_t, r, 0x10 + n * 6) = (int16_t)x;
        P(int16_t, r, 0x12 + n * 6) = (int16_t)y;
        sx = STEP[d * 4 + (y & 1) * 2];
        sy = STEP[d * 4 + (y & 1) * 2 + 1];
        x += sx;
        y += sy;
        n++;
    } while (n < 0x14);
    r = SEGS + seg * 0x86;
    if (n < 1) {
        memset(r, 0, 0x86);
    } else {
        P(int32_t, r, 0) = 1;
        P(uint16_t, r, 8) = dir;
        P(int32_t, r, 10) = n;
        P(int16_t, ua, 0x372 + d * 2) = seg;
        P(int16_t, ub, 0x372 + (int16_t)back * 2) = seg;
    }
    return seg;
}

/* 0x10078f1e: the units standing between two posts take 50% damage from the first's
 * player */
void __cdecl wall_crush_between(uint8_t *a, uint8_t *b)
{
    int32_t ids[20];
    int32_t n = units_between(a, b, ids);
    for (int32_t k = 0; k < n; k++) {
        uint8_t *u = unit_ptr(ids[k]);
        if (u)
            unit_take_damage(u, ((uint32_t)(uintptr_t)a & 0xffff0000) | P(uint8_t, a, 0x14), 0x32);
    }
}

/* 0x10078dd5: connects two of a player's wall posts (both with walls, +0x42, and not
 * flagged +0x3e bit 12); with `check`, not when they are too close. The units in between
 * are crushed, and in mission state 4 the player hears message 0x26. 1 if connected */
int32_t __cdecl walls_connect(uint32_t a, uint32_t b, uint32_t unused, int check)
{
    (void)unused;
    uint8_t *ua = unit_ptr(a & 0xffff), *ub = unit_ptr(b & 0xffff);
    if (!ua || !ub || P(uint8_t, ub, 0x14) != P(uint8_t, ua, 0x14))
        return 0;
    if (!P(int16_t, ua, 0x42) || (P(uint32_t, ua, 0x3e) >> 12 & 1) || !P(int16_t, ub, 0x42) ||
        (P(uint32_t, ub, 0x3e) >> 12 & 1))
        return 0;
    if (check == 1 && posts_too_close(ua, ub) == 1)
        return 0;
    if (wall_segment_build(a, b) >= 0) {
        wall_crush_between(ua, ub);
        if (I(int32_t, 0x17f5) == 4)
            player_announce(P(uint8_t, ua, 0x14), 0x26, 1);
    }
    return 1;
}

/* ---- 0x10079669-0x10079c6c (part 81): wall pieces ---- */

#define PIECES I(uint8_t *, 0x1b13) /* 0xe bytes each: the sprite (+6), flags (+10: bit 0 in use,
                                       bits 1-8 the player, bits 9-11 the direction) */
#define pieces_grow ORIG(int(__cdecl *)(int32_t), 0x1007735c)

/* 0x100798c3: a free wall piece (marked in use), or 50 more pieces. -1 if none */
int16_t __cdecl wall_piece_alloc(void)
{
    int16_t k;
    for (k = 0; (int32_t)k < I(int32_t, 0xe11b); k++)
        if (!(P(uint8_t, PIECES, 10 + k * 0xe) & 1)) {
            P(uint32_t, PIECES, 10 + k * 0xe) |= 1;
            return k;
        }
    int32_t old = I(int32_t, 0xe11b);
    I(int32_t, 0xe11b) += 0x32;
    return pieces_grow(old) == 1 ? (int16_t)old : -1;
}

/* 0x1007999f: marks a cell as walled: blocked (bit 0, bit 14), not bits 1 and 15 */
void __cdecl wall_cell_set(int32_t x, int32_t y)
{
    line_visit(x, y);
    uint32_t *c = &I(uint32_t *, 0x1934)[(y << (I(uint8_t, 0x1aa0) & 0x1f)) + x];
    *c |= 1;
    *c &= 0xfffffffd;
    *c |= 0x4000;
    *c &= 0xffff7fff;
}

/* 0x10079bd6: marks a free neighbour cell of a wall as blocked and raised (bits 0, 14, 15) */
void __cdecl wall_side_cell_set(int32_t x, int32_t y)
{
    line_visit(x, y);
    uint32_t *c = &I(uint32_t *, 0x1934)[(y << (I(uint8_t, 0x1aa0) & 0x1f)) + x];
    if (!(*c & 1) && !(*c >> 14 & 1)) {
        *c |= 1;
        *c &= 0xfffffffd;
        *c |= 0x4000;
        *c |= 0x8000;
    }
}

/* 0x10079a13: for a wall piece in an even direction, the four cells beside it (directions +1, +3, -3,
 * -1) that are on the map */
void __cdecl wall_sides_set(int32_t x, uint32_t y, int16_t dir)
{
    static const int turns[] = { 1, 3, -3, -1 };
    for (int k = 0; k < 4; k++) {
        uint16_t d = (uint16_t)(dir + turns[k]) & 7;
        int32_t dx = STEP[(y & 1) * 2 + (int16_t)d * 4], dy = STEP[(y & 1) * 2 + (int16_t)d * 4 + 1];
        if (map_cell_valid(dx + x, (int32_t)(dy + y)) == 1)
            wall_side_cell_set(dx + x, (int32_t)(dy + y));
    }
}

/* 0x10079669: places a wall piece of `owner` (low byte) facing `dir` at cell (x, y): its
 * cell is walled, its sprite placed (mirrored for directions 1 and 5) and shown, and for
 * even directions the cells beside it are blocked. The piece, or 0 when none is free (the
 * original returned 0 there too) */
int16_t __cdecl wall_piece_at(int32_t x, uint32_t y, uint16_t dir, uint32_t owner)
{
    int16_t k = wall_piece_alloc();
    if (k < 0)
        return 0;
    uint8_t *p = PIECES + k * 0xe;
    P(uint32_t, p, 10) = (P(uint32_t, p, 10) & 0xfffffe01) | (owner & 0xff) * 2;
    P(uint32_t, p, 10) = (P(uint32_t, p, 10) & 0xfffff1ff) | ((uint32_t)(int32_t)(int16_t)dir & 7) << 9;
    wall_cell_set(x, (int32_t)y);
    void *s = P(void *, p, 6);
    SpriteEmptyQueue(I(void *, ISO_HSL), s);
    SpriteSetPosition(I(void *, ISO_HSL), s, (int)((y & 1) * 0x20 + x * 0x40 - I(uint32_t, 0xd709) + 0x20),
                      (int)(y * 0x10 - I(uint32_t, 0xd71e) + 0x10), 0);
    SpriteSetLevel(I(void *, ISO_HSL), s, (int32_t)(y * 0x10 - I(uint32_t, 0xd71e) + 10000));
    SpriteSetFlip(I(void *, ISO_HSL), s, (int16_t)dir == 1 || (int16_t)dir == 5, 0);
    SpriteSetProcessMsgs(I(void *, ISO_HSL), s, 1);
    wall_piece_frame(k);
    if (!((int16_t)dir & 1))
        wall_sides_set(x, y, (int16_t)dir);
    return k;
}

/* ---- 0x1007aa2b-0x1007c3c8 (part 82) ---- */

#define units_on_line ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t, int32_t *), 0x10076f60)
#define MSG_STOP ((uint32_t *)ORIG_PTR(0x10157334))
#define MSG_ANIM ((uint32_t *)ORIG_PTR(0x101572c0))
#define MSG_LOOP ((uint32_t *)ORIG_PTR(0x10157338))

/* 0x1007aa2b: like walls_connect, for posts being built: not when a wall already runs that
 * way (route kind 1); when they are too close, a post standing between them splits the old
 * segment instead (and nothing is connected). 1 if connected */
int32_t __cdecl walls_connect_new(uint32_t a, uint32_t b)
{
    uint8_t *ua = unit_ptr((int16_t)a), *ub = unit_ptr((int16_t)b);
    if (!ua || !ub || P(uint8_t, ub, 0x14) != P(uint8_t, ua, 0x14))
        return 0;
    if (!P(int16_t, ua, 0x42) || (P(uint32_t, ua, 0x3e) >> 12 & 1) || !P(int16_t, ub, 0x42) ||
        (P(uint32_t, ub, 0x3e) >> 12 & 1))
        return 0;
    int16_t dir = unit_direction(a, b);
    if (unit_route_kind1((int16_t)a, dir) == 1)
        return 0;
    if (posts_too_close(ua, ub) == 1) {
        int32_t ids[20];
        int16_t n = (int16_t)units_on_line(a, b, ids);
        for (int16_t k = 0; k < n; k++)
            if (unit_is_post((int16_t)ids[k]) == 1) {
                route_split(a, b, 1);
                break;
            }
        return 0;
    }
    if (wall_segment_build(a, b) >= 0)
        wall_crush_between(ua, ub);
    return 1;
}

/* IsoHideBeaconDirections: hides the 16 beacon direction sprites (0x2e, 0xe bytes each) */
void __cdecl IsoHideBeaconDirections(uint8_t *iso)
{
    g_iso = iso;
    for (int16_t k = 0; k < 0x10; k++)
        if (I(int32_t, 0x2e + k * 0xe) && I(void *, 0x32 + k * 0xe)) {
            SpriteEmptyQueue(I(void *, ISO_HSL), I(void *, 0x32 + k * 0xe));
            SpriteHide(I(void *, ISO_HSL), I(void *, 0x32 + k * 0xe), 1);
        }
}

/* 0x1007c1d5: shows a wall piece's frame for its direction: still (cel dir*9+4), or with
 * 0x11c set, its looping animation (cels dir*10 to +9) */
void __cdecl wall_piece_frame(int16_t k)
{
    uint8_t *p = PIECES + k * 0xe;
    SpriteEmptyQueue(I(void *, ISO_HSL), P(void *, p, 6));
    uint32_t dir = P(uint32_t, p, 10) >> 9 & 7;
    if (!I(int32_t, 0x11c)) {
        SpriteShow(I(void *, ISO_HSL), P(void *, p, 6), dir * 9 + 4, 1);
    } else {
        SpriteSendMessage(I(void *, ISO_HSL), P(void *, p, 6), 1, MSG_STOP);
        MSG_ANIM[1] = dir * 10;
        MSG_ANIM[2] = MSG_ANIM[1] + 9;
        MSG_ANIM[3] = 0x53;
        SpritePostMessage(I(void *, ISO_HSL), P(void *, p, 6), 4, MSG_ANIM);
        SpritePostMessage(I(void *, ISO_HSL), P(void *, p, 6), 4, MSG_LOOP);
    }
}

/* 0x1007c35e: redraws every wall piece in use */
void __cdecl wall_pieces_refresh(void)
{
    for (int16_t k = 0; (int32_t)k < I(int32_t, 0xe11b); k++)
        if (P(uint8_t, PIECES, 10 + k * 0xe) & 1)
            wall_piece_frame(k);
}

/* loads the sprite copies of `res` into a table of `n` 0xe-byte records (resource at
 * `res_off`, sprite at +4 after it); `idle` stops their message processing */
static void load_sprites(uint8_t *base, int32_t n, uint32_t res, uint32_t err, int idle)
{
    for (int16_t k = 0; (int32_t)k < n; k++) {
        uint8_t *r = base + k * 0xe;
        if (P(uint32_t, r, 0))
            continue;
        P(uint32_t, r, 0) = RtCopy(I(void *, ISO_APP), res);
        RtLoad(I(void *, ISO_APP), P(uint32_t, r, 0));
        P(void *, r, 4) = RtGetResource(I(void *, ISO_APP), P(uint32_t, r, 0))[1];
        if (!P(uint32_t, r, 0) || !P(void *, r, 4)) {
            wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(err), (int)k);
            OutputDebugStringA((const char *)(g_iso + 0xdea));
            I(int32_t, 0xa06) = 1;
        }
        if (idle)
            SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, r, 4), 0);
        I(uint32_t, 0x1858) = P(uint32_t, r, 0);
    }
}

/* IsoInitEnergyWalls: makes 100 wall pieces (copies of `wall`) and 100 segment records, the
 * 16 beacon direction arrows, 8 "umbDiamond" and 128 "umbNop" sprites. Only with sprites
 * shown (not in the map editor) */
void __stdcall IsoInitEnergyWalls(uint8_t *iso, uint32_t wall, uint32_t arrow, uint32_t diamond, uint32_t nop)
{
    g_iso = iso;
    if (!P(int32_t, iso, ISO_HAVE_SPRITES))
        return;
    I(int32_t, 0xe11b) = 100;
    SEG_N = 100;
    I(uint32_t, 0xd6c7) = wall;
    int32_t size = I(int32_t, 0xe11b) * 0xe;
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10147404), size);
    OutputDebugStringA((const char *)(g_iso + 0xdea));
    PIECES = winmalloc((SIZE_T)size, 1);
    if (!PIECES)
        return;
    load_sprites(PIECES + 2, I(int32_t, 0xe11b), wall, 0x10147430, 1);
    SEGS = winmalloc((SIZE_T)(SEG_N * 0x86), 1);
    if (!SEGS)
        return;
    for (int32_t k = 0; k < SEG_N; k++)
        memset(SEGS + k * 0x86, 0, 0x86);
    load_sprites(g_iso + 0x2e, 0x10, arrow, 0x10147458, 0);
    load_sprites(g_iso + 0xcee8, 8, diamond, 0x10147484, 0);
    load_sprites(g_iso + 0xcf58, 0x80, nop, 0x101474ac, 1);
    G(uint32_t, 0x1017bafc) = diamond;
}

/* ---- 0x1007d144-0x1007d592 (part 83): a new post's walls ---- */

#define post_drop ORIG(void(__cdecl *)(uint32_t), 0x1007a8ab)
#define post_placed ORIG(void(__cdecl *)(uint32_t), 0x100f223a)
#define net_send_wall ORIG(void(__cdecl *)(int16_t, int16_t, int), 0x1003527c)

/* 0x1007d228: connects post `post` to the posts its route records point at (ids 1-1999);
 * in network games each new wall is sent to the others */
void __cdecl post_connect_all(uint32_t post, int check)
{
    uint8_t *u = unit_ptr((int16_t)post);
    if (!P(int16_t, u, 0x42) || (P(uint32_t, u, 0x3e) >> 12 & 1))
        return;
    for (int16_t d = 0; d < 8; d++) {
        if (!unit_route_kind2((int16_t)post, d))
            continue;
        int16_t other = (int16_t)route_neighbour((int16_t)post, d);
        uint32_t o = (uint32_t)(int32_t)other;
        if ((int32_t)o > 0 && (int32_t)o < 2000 && walls_connect(post, o, 0, check) && I(int32_t, 0x175))
            net_send_wall((int16_t)post, other, 1);
    }
}

/* 0x1007d16c: when a post's walls are due (+0x3e bit 5): by its power state (0x10001f74),
 * connected (powered, or state -0x7ffe) or dropped (-0x7ffd); the flag is cleared */
void __cdecl post_update_walls(uint32_t post)
{
    if (!(P(uint32_t, unit_ptr(post & 0xffff), 0x3e) >> 5 & 1))
        return;
    int16_t state = (int16_t)power_check((int16_t)post);
    if (state == -0x7ffe || state == 1)
        post_connect_all(post, 1);
    else if (state == -0x7ffd)
        post_drop(post);
    P(uint32_t, unit_ptr(post & 0xffff), 0x3e) &= 0xffffffdf;
}

/* 0x1007d144: a post was placed */
void __cdecl post_new(uint32_t post)
{
    post_update_walls(post);
    post_placed(post);
}

/* 0x1007d323: a new post looks in each direction (from its second cell, up to 19 cells on
 * the map) for the first post, and builds a wall to it if it is the same player's, has walls
 * and is not too close */
void __cdecl post_find_walls(uint32_t post)
{
    uint8_t *u = unit_ptr(post & 0xffff);
    if (!P(int16_t, u, 0x42) || (P(uint32_t, u, 0x3e) >> 12 & 1))
        return;
    int x = P(int16_t, u, 0x52), tx, ty;
    uint32_t y = (uint32_t)(int32_t)P(int16_t, u, 0x54);
    IsoVTilePos(g_iso, &x, (int *)&y, &tx, &ty);
    for (int16_t d = 0; d < 8; d++) {
        uint32_t found = 0xffffffff;
        uint32_t cy = y + STEP[d * 4 + (y & 1) * 2 + 1];
        int32_t cx = x + STEP[d * 4 + (y & 1) * 2] + STEP[d * 4 + (cy & 1) * 2];
        cy += STEP[d * 4 + (cy & 1) * 2 + 1];
        for (int n = 2; n <= 0x13 && map_cell_valid(cx, (int32_t)cy) == 1; n++) {
            line_visit(cx, (int32_t)cy);
            uint32_t c = I(uint32_t *, 0x1934)[(cy << (I(uint8_t, 0x1aa0) & 0x1f)) + cx];
            if ((c >> 16 & 0xfff) && unit_is_post((int16_t)((uint16_t)(c >> 16) & 0xfff))) {
                found = c >> 16 & 0xfff;
                break;
            }
            cx += STEP[d * 4 + (cy & 1) * 2];
            cy += STEP[d * 4 + (cy & 1) * 2 + 1];
        }
        uint8_t *o;
        if ((int32_t)found > 0 && (o = unit_ptr((int32_t)found)) && P(int16_t, o, 0x42) &&
            !(P(uint32_t, o, 0x3e) >> 12 & 1) && P(uint8_t, o, 0x14) == P(uint8_t, u, 0x14) && !posts_too_close(u, o))
            wall_segment_build(post, found);
    }
}
