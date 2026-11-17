/* The game logic, 0x100090e0-0x1000a86b and 0x1000c898-0x1000cdd4: setting up a unit when it is placed on the map,
 * the player's upgrades for it, keeping track of the map sector it is in, its sprite
 * following its state every frame, and making room where a structure is put down. */
#include "iso_core.h"
#include "events.h"
#include "sector.h"
#include "combat.h"
#include "unit.h"

/* not rewritten yet (names ours, provisional) */
#define unit_set_rank ORIG(void(__cdecl *)(uint32_t, int16_t, int), 0x10058bf5)
#define slot_offset ORIG(void(__cdecl *)(uint32_t, int16_t, int16_t *), 0x10091860)
#define unit_clear_path ORIG(void(__cdecl *)(uint16_t), 0x1004f34e)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_set_target ORIG(void(__cdecl *)(uint16_t, uint16_t), 0x100dcabd)
#define reveal_around ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, uint16_t, int), 0x10090223)
#define turret_new ORIG(uint8_t *(__cdecl *)(uint32_t), 0x10085e60)
#define sprite_recolour ORIG(void(__cdecl *)(void *, uint8_t, int32_t, int, int), 0x100b1ce0)
#define unit_set_frame ORIG(void(__cdecl *)(uint32_t), 0x1004ea01)
#define sector_seen hotspot_entered
#define unit_remove unit_kill

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define U_FLAGS2 0x10e
#define U_TYPEINFO 0x32 /* the unit type's record */
#define U_CELL 0x56     /* its map cell (index), 0 off the map */
#define U_TOP 0x39d     /* the upper part of a turret unit */

/* 0x100090e0: the player's upgrade for the unit's kind: infantry get a rank, the others
 * (original: halved) 0xd6 plus one */
int __cdecl unit_apply_upgrade(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t t = P(uint8_t, u, U_ID_TYPE);
    int infantry = 0, k = -1;
    if (t < 8) {
        k = 0xf;
        infantry = 1;
    } else if (t >= 0x18 && t <= 0x1f) {
        k = 0xe;
    } else if (t >= 0x20 && t <= 0x27) {
        k = 10;
    } else if (t >= 0x28 && t <= 0x2f) {
        k = 0xd;
    } else if (t >= 0x30 && t <= 0x37) {
        k = 0xc;
    }
    if (k < 0)
        return 0;
    uint32_t block = P(uint8_t, u, U_PLAYER) * ISO_PLAYER_SIZE;
    if (!I(uint8_t, block + k + 0x2dcb))
        return 0;
    if (infantry) {
        unit_set_rank(id, I(uint8_t, block + k + 0x2de4), 0);
    } else {
        P(int16_t, u, 0xd6) = (int16_t)(P(int16_t, u, 0xd6) >> 1);
        P(int16_t, u, 0xd6) = (int16_t)(P(int16_t, u, 0xd6) + 1);
    }
    return 1;
}

/* 0x100092bb: puts a unit on the map at (x, y) for `player`, facing `dir`, and sets up
 * everything about it. loaded: from a saved game (no revealing). Returns 1, 0 or -0x5a
 * when its turret could not be made. */
int __cdecl unit_place(uint32_t id, uint8_t a16, uint8_t player, int16_t x, int16_t y, uint8_t dir,
                       uint8_t a30, int loaded)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    uint8_t *ti;
    P(uint8_t, u, 0x2a) = 0;
    int tx = x, ty = y, ox, oy;
    P(int32_t, u, U_CELL) = IsoVTilePos(g_iso, &tx, &ty, &ox, &oy);
    if ((uint32_t)tx >= I(uint32_t, ISO_MAP_W) || (uint32_t)ty >= I(uint32_t, ISO_MAP_H))
        P(int32_t, u, U_CELL) = 0;
    if (!P(int32_t, u, U_CELL) || !P(uint8_t, u, 0xf1) || P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x74) != 1) {
        P(uint32_t, u, U_FLAGS2) &= 0xffffff7f;
    } else {
        /* one of four places on the cell */
        uint32_t *cell = &I(uint32_t *, 0x1934)[P(int32_t, u, U_CELL)];
        for (uint32_t c = 0; (int)c < 4; c++)
            if (!(*cell >> 4 & (1u << c) & 0xf) && !(*cell >> 8 & (1u << c) & 0xf)) {
                P(uint32_t, u, U_FLAGS2) = (c & 3) << 10 | (P(uint32_t, u, U_FLAGS2) & 0xfffff3ff);
                break;
            }
        *cell = ((*cell >> 8 | 1u << (P(uint32_t, u, U_FLAGS2) >> 10 & 3)) & 0xf) << 8 | (*cell & 0xfffff0ff);
        *cell |= 4;
        if (I(int, 0x175)) {
            int16_t off[4];
            slot_offset(0, (int16_t)(P(uint32_t, u, U_FLAGS2) >> 10 & 3), off);
            x = (int16_t)(off[0] + x);
            y = (int16_t)(off[2] + y);
        }
        P(uint32_t, u, U_FLAGS2) |= 0x80;
    }
    ti = P(uint8_t *, u, U_TYPEINFO);
    P(int16_t, u, U_X) = x;
    P(int16_t, u, U_Y) = y;
    P(int16_t, u, 0x6c) = x;
    P(int16_t, u, 0x6e) = y;
    P(int16_t, u, 0x70) = x;
    P(int16_t, u, 0x72) = y;
    P(int16_t, u, 0x74) = x;
    P(int16_t, u, 0x76) = y;
    P(uint8_t, u, 0x78) = 0xff;
    P(uint8_t, u, 0x14d) = 0xff;
    P(uint8_t, u, 0x14e) = 0xff;
    P(uint8_t, u, 0x79) = dir;
    P(uint8_t, u, 0x7a) = dir;
    P(uint8_t, u, 0x7b) = dir;
    P(uint8_t, u, 0x7c) = (uint8_t)(dir * 6);
    P(uint32_t, u, 0x7e) = 0;
    P(uint16_t, u, 0x82) = 0;
    P(uint32_t, u, 0x36) = 0;
    P(uint8_t, u, 0x16) = a16;
    P(int16_t, u, 0x28) = (int16_t)P(int32_t, ti, 0x74);
    P(uint8_t, u, U_PLAYER) = player;
    P(uint32_t, u, U_FLAGS) |= 0x40;
    P(uint32_t, u, U_FLAGS) |= 0x400;
    P(uint32_t, u, U_FLAGS) |= 8;
    P(uint8_t, u, U_SEL_STATE) = 2;
    P(uint8_t, u, 0x159) = 0;
    P(uint32_t, u, U_FLAGS2) &= 0xfffffeff;
    P(uint16_t, u, 0x8c) = 0xffff;
    P(uint16_t, u, 0x8e) = 0xffff;
    P(uint16_t, u, 0x1b3) = 0xffff;
    unit_clear_path((uint16_t)id);
    P(uint16_t, u, 0x1c9) = 0xffff;
    int32_t type = P(int32_t, u, U_ID_TYPE);
    if (type < 0x30 || type > 0x37)
        P(uint32_t, u, U_FLAGS) &= 0xffffff7f;
    else
        P(uint32_t, u, U_FLAGS) |= 0x80;
    P(uint8_t, u, 0x30) = a30;
    P(int16_t, u, 0xe4) = (int16_t)P(int32_t, ti, 8);
    P(int16_t, u, 0xd6) = (int16_t)unit_max_health(u, 0);
    P(int16_t, u, 0xd8) = (int16_t)P(int32_t, ti, 0x18);
    P(int16_t, u, 0xda) = (int16_t)P(int32_t, ti, 0x1c);
    P(int16_t, u, 0xe6) = (int16_t)P(int32_t, ti, 0x20);
    P(int16_t, u, 0xe8) = (int16_t)P(int32_t, ti, 0x24);
    P(int16_t, u, 0xdc) = (int16_t)P(int32_t, ti, 0x28);
    P(int16_t, u, 0xe0) = (int16_t)P(int32_t, ti, 0x30);
    P(uint8_t, u, 0xef) = P(uint8_t, ti, 0xfc);
    P(uint8_t, u, 0xed) = 1;
    if (I(int, ISO_HAVE_SPRITES) && P(int16_t, u, 0xe6) >= 0) {
        int32_t *wt = I(int32_t *, 0x1990); /* the weapon table */
        P(int16_t, u, 0xea) = (int16_t)P(int32_t, (uintptr_t)wt[0], wt[0xb] * P(int16_t, u, 0xe6) * 4 + 0x58);
    }
    if (!I(int, ISO_HAVE_SPRITES) || I(int, 0x17f5) == 1)
        P(uint8_t, u, 0xf1) = 1;
    P(uint16_t, u, 0x1a1) = 0xffff;
    if (!P(uint8_t, u, 0xf1))
        unit_apply_upgrade(id);
    P(uint8_t, u, 0x158) = 0;
    if (!P(uint8_t, u, 0xf1) && P(int32_t, ti, 0x70)) {
        /* still being built */
        P(uint32_t, u, U_FLAGS2) |= 0x8000;
        unit_set_anim(id, 0x26, 0, 0);
        unit_set_action(id, 0x13, 0, 0);
        P(uint8_t, u, 0x79) = 0;
        P(uint8_t, u, 0x7a) = 0;
        P(uint8_t, u, U_SEL_STATE) = 1;
    } else {
        P(uint32_t, u, U_FLAGS2) &= 0xffff7fff;
        unit_set_anim(id, 0, 0, 0);
        unit_set_action(id, 0x1e, 0, 0);
        if (P(uint8_t, u, 0xf1) && !I(int, ISO_HAVE_SPRITES))
            P(uint16_t, u, 0x145) = dir;
    }
    P(void *, u, 0x15f) = G(void *, 0x1012fe70 + P(int32_t, ti, 4) * 4);
    P(void *, u, 0x163) = P(void *, u, 0x15f);
    unit_set_target((uint16_t)id, 0);
    P(uint16_t, u, 0x18b) = 0;
    P(uint8_t, u, 0x198) = 2;
    if (!loaded && P(int32_t, u, U_CELL))
        reveal_around(P(int16_t, u, U_X), P(int16_t, u, U_Y), P(uint8_t, u, U_PLAYER), 6, 0xffffffff, (uint16_t)id, 0);
    unit_sector_update(id, 1, !loaded);
    typedef void(__cdecl * PlacedHandler)(uint32_t, int);
    PlacedHandler placed = G(PlacedHandler, 0x101545ec + P(int32_t, u, U_ID_TYPE) * 0x44);
    if (placed)
        placed(id, 1);
    void *spr = P(void *, u, 4), *src = P(void *, u, 8);
    type = P(int32_t, u, U_ID_TYPE);
    if (type < 0x30 || type > 0x37) {
        SpriteReplaceCellRange(HSL, src, spr, P(uint16_t, u, 0x145), 0, 1);
        SpriteReplaceCellRange(HSL, src, spr, P(uint32_t, ti, 0x84), 1, 10);
        SpriteReplaceCellRange(HSL, src, spr, P(uint32_t, ti, 0x80), 0xb, 1);
    }
    if (P(int32_t, ti, 0xb4)) {
        P(void *, u, 0x36) = winmalloc(0x40, 1);
        if (!P(void *, u, 0x36))
            P(void *, u, 0x36) = NULL;
        else
            SpriteSetExtOffsets(HSL, spr, P(void *, u, 0x36));
        SpriteReplaceCellRange(HSL, src, spr, P(uint32_t, ti, 0x90), 0xe, 1);
    }
    type = P(int32_t, u, U_ID_TYPE);
    if ((type < 100 || type > 0x67) && (type < 0x38 || type > 0x3b))
        P(int16_t, u, 0x66) = (int16_t)(P(int16_t, u, U_Y) + 10000);
    else
        P(int16_t, u, 0x66) = 0;
    SpriteSetLevel(HSL, spr, P(int16_t, u, 0x66));
    SpriteSetPosition(HSL, spr, P(int16_t, u, U_X), P(int16_t, u, U_Y), 0);
    SpriteGetBounds(HSL, spr, (RECT *)(u + 0xc6));
    P(int16_t, u, 0xb8) = (int16_t)((int16_t)P(int32_t, u, 0xce) - (int16_t)P(int32_t, u, 0xc6));
    P(int16_t, u, 0xba) = (int16_t)((int16_t)P(int32_t, u, 0xd2) - (int16_t)P(int32_t, u, 0xca));
    P(uint32_t, u, 0x14f) = I(uint32_t, 0x17dd);
    P(uint32_t, u, 0x1a3) = I(uint32_t, 0x17dd);
    typedef void(__cdecl * KindHandler)(uint8_t *);
    KindHandler kind = G(KindHandler, 0x10118d88 + P(int32_t, ti, 4) * 12);
    if (kind)
        kind(u);
    P(uint32_t, u, 0x1a7) = 0;
    for (int c = 0; c < 4; c++)
        P(uint16_t, u, 0x1b5 + c * 2) = 0xffff;
    P(uint32_t, u, 0x3a) = 0;
    P(uint32_t, u, U_FLAGS2) &= 0xffffbfff;
    P(uint16_t, u, 0x11f) = 0;
    P(uint16_t, u, 0x11d) = P(uint16_t, u, 0x11f);
    P(uint16_t, u, 0x119) = P(uint16_t, u, 0x11d);
    P(uint16_t, u, 0x11b) = P(uint16_t, u, 0x119);
    P(uint32_t, u, U_FLAGS2) &= 0xf7ffffff;
    P(uint32_t, u, U_FLAGS2) &= 0xffffdfff;
    P(uint16_t, u, 0x15d) = 0xffff;
    P(uint8_t, u, 0x15a) = 0;
    P(uint8_t, u, 0x177) = 0xff;
    P(uint32_t, u, U_FLAGS2) &= 0xfffffffd;
    P(uint32_t, u, U_FLAGS2) &= 0xfffffffb;
    P(uint32_t, u, U_FLAGS2) &= 0xffffefff;
    P(uint32_t, u, U_FLAGS2) &= 0xfdffffff;
    P(uint32_t, u, U_FLAGS2) &= 0xfffffffe;
    P(uint32_t, u, U_FLAGS) |= 0x40000;
    P(uint16_t, u, 0x1cb) = (uint16_t)id;
    P(uint16_t, u, 0x1cd) &= 0xfffe;
    P(uint16_t, u, 0x1cd) &= 0xfffd;
    P(uint16_t, u, 0x1cd) &= 0xfff7;
    P(uint16_t, u, 0x1cd) |= 0x10;
    P(uint16_t, u, 0x1cd) &= 0xffdf;
    P(uint16_t, u, 0x1cd) &= 0xffbf;
    P(uint16_t, u, 0x1cd) &= 0xff7f;
    P(uint16_t, u, 0x1cd) &= 0xfeff;
    P(uint16_t, u, 0x1cd) &= 0xfdff;
    P(uint16_t, u, 0x1cd) &= 0xfbff;
    P(uint16_t, u, 0x1cd) &= 0xf7ff;
    P(uint16_t, u, 0x1cd) &= 0xefff;
    P(uint32_t, u, 0x1cf) = 0;
    P(uint32_t, u, 0x1d3) = 0;
    P(uint16_t, u, 0x1e7) = 0;
    P(uint16_t, u, 0x360) = 0x8300;
    P(uint16_t, u, 0x362) = 0x8300;
    P(uint32_t, u, 0x364) = 0;
    P(uint32_t, u, 0x368) = P(uint32_t, u, U_CELL);
    P(uint16_t, u, 0x36c) = 0x8300;
    P(uint16_t, u, 0x36e) = 0x8300;
    P(uint16_t, u, 0x370) = 0xffff;
    for (int c = 0; c < 8; c++)
        P(uint16_t, u, 0x372 + c * 2) = 0xffff;
    for (int c = 0; c < 2; c++)
        P(uint16_t, u, 0x1bf + c * 2) = 0xffff;
    P(uint16_t, u, 0x1c3) = 0xffff;
    P(uint16_t, u, 0x38a) = 0;
    P(uint16_t, u, 0x38c) = I(uint16_t, 0xc2fd);
    P(uint16_t, u, 0x38e) = 0;
    P(uint16_t, u, 0x390) = I(uint16_t, 0xc2ef);
    P(uint8_t, u, 0x392) = 1;
    P(uint32_t, u, 0x399) = 0xffffffff;
    if (I(int, ISO_HAVE_SPRITES))
        SpriteSetProcessMsgs(HSL, spr, 0);
    P(uint32_t, u, 0x173) = 0;
    P(uint8_t, u, 0x153) = 0;
    P(uint16_t, u, 0xf4) = 0;
    type = P(int32_t, u, U_ID_TYPE);
    if (type < 0x30 || type > 0x37)
        P(uint32_t, u, U_FLAGS2) &= 0xfffffdff;
    else
        P(uint32_t, u, U_FLAGS2) |= 0x200;
    P(uint8_t *, u, U_TOP) = NULL;
    if (P(uint32_t, u, U_FLAGS2) >> 9 & 1) {
        /* the turret on top */
        uint8_t *top = turret_new(id);
        P(uint8_t *, u, U_TOP) = top;
        if (!top)
            return 0;
        P(float, top, 0x18) = (float)P(int16_t, u, U_X);
        P(float, top, 0x1c) = (float)P(int16_t, u, U_Y);
        P(uint32_t, top, 0x14) = 0x30;
        P(uint32_t, top, 0) = RtCopy(APP, I(uint32_t, 0xc33b));
        RtLoad(APP, P(uint32_t, top, 0));
        P(void *, top, 4) = RtGetResource(APP, P(uint32_t, top, 0))[1];
        unsigned restype = RtGetResType(APP, P(uint32_t, top, 0));
        if (!P(uint32_t, top, 0) || !P(void *, top, 4) || restype != 0x14)
            return -0x5a;
        I(uint32_t, 0x1858) = P(uint32_t, u, 0);
        if (I(int, 0x175))
            sprite_recolour(P(void *, top, 4), (uint8_t)I(uint32_t, player * ISO_PLAYER_SIZE + 0x1ba2),
                            P(int32_t, u, U_ID_TYPE), 0, 0);
        SpriteReplaceCellRange(HSL, src, spr, P(uint16_t, u, 0x145), 0, 1);
        SpriteReplaceCellRange(HSL, src, P(void *, top, 4), P(uint16_t, u, 0x145), 0, 1);
        SpriteReplaceCellRange(HSL, src, P(void *, top, 4), P(uint32_t, ti, 0x84), 1, 10);
        SpriteReplaceCellRange(HSL, src, P(void *, top, 4), P(uint32_t, ti, 0x80), 0xb, 1);
    }
    if (I(int, ISO_HAVE_SPRITES)) {
        uint8_t d = P(uint8_t, u, 0x79);
        type = P(int32_t, u, U_ID_TYPE);
        if (d < 3 || d > 5 || ((type < 0 || type > 0x27) && type != 0x22)) {
            if (P(uint8_t, u, 0x159))
                P(uint32_t, u, U_FLAGS2) |= 0x100;
            P(uint8_t, u, 0x159) = 0;
            P(uint16_t, u, 0x145) = (uint16_t)((int16_t)P(int32_t, ti, 0x7c) * (uint16_t)d);
        } else {
            /* facing left: the mirrored cells */
            if (!P(uint8_t, u, 0x159))
                P(uint32_t, u, U_FLAGS2) |= 0x100;
            P(uint8_t, u, 0x159) = 1;
            P(int16_t, u, 0x145) = (int16_t)(G(int8_t, 0x1015b578 + d) * (int16_t)P(int32_t, ti, 0x7c));
        }
        P(uint16_t, u, 0x191) = 0xffff;
        P(uint16_t, u, 399) = 0xffff;
        unit_set_frame(id);
    }
    P(uint32_t, u, U_FLAGS2) &= 0xffefffff;
    P(uint8_t, u, 0x112) &= 0xfd;
    P(uint32_t, u, U_FLAGS) &= 0xdfffffff;
    P(uint32_t, u, U_FLAGS) &= 0x7fffffff;
    P(uint16_t, u, 0x193) &= 0xf7ff;
    P(uint8_t, u, 0x178) = 0;
    return 1;
}

/* 0x1000a474: the unit's place in the fog grid (32-pixel squares; reveal: uncover around it
 * when it enters a new one) and in the sector grid (256 pixels; mode 1 enters the new
 * sector, 2 even when it is the same) */
void __cdecl unit_sector_update(uint32_t id, int16_t mode, int reveal)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!P(int32_t, u, U_CELL))
        return;
    int gx = P(int16_t, u, U_X) + I(int32_t, 0xd709);
    int gy = P(int16_t, u, U_Y) + I(int32_t, 0xd71e);
    uint8_t player = P(uint8_t, u, U_PLAYER);
    if (reveal && P(int8_t, u, 0x196) != '#' && P(int8_t, u, 0x197) != '#') {
        int sq = ((gy >> 5) << (I(uint8_t, 0x17b1) & 0x1f)) + (gx >> 5);
        if (sq >= 0 && sq < I(int, 0x17ad) && P(int16_t, u, 0x19d) != sq) {
            uint8_t *seen = I(uint8_t *, 0x17a7);
            if (!((1u << (player & 0x1f)) & seen[sq])) {
                if (!I(int, 0xd79f)) {
                    seen[sq] |= (uint8_t)(1u << (player & 0x1f));
                    reveal_around(P(int16_t, u, U_X), P(int16_t, u, U_Y), player, 6, P(uint8_t, u, 0x79),
                                  (uint16_t)id, 0);
                } else {
                    reveal_around(P(int16_t, u, U_X), P(int16_t, u, U_Y), player, 6, 0xffffffff, (uint16_t)id, 0);
                }
            }
            P(int16_t, u, 0x19d) = (int16_t)sq;
        }
    }
    int sector = ((gy >> 8) << (I(uint8_t, 0xc2f5) & 0x1f)) + (gx >> 8);
    if (sector < 0 || sector >= I(int, 0xc2f1)) {
        int32_t type = P(int32_t, u, U_ID_TYPE);
        if (type >= 0x30 && type <= 0x37 && P(int16_t, u, 0x1a1) >= 0)
            sector_leave(u);
        return;
    }
    if (P(int16_t, u, 0x1a1) == sector && mode != 2)
        return;
    if (P(int16_t, u, 0x1a1) >= 0)
        sector_leave(u);
    if (mode == 1) {
        P(uint16_t, u, 0x1a1) = 0xffff;
        int16_t *s = (int16_t *)(I(uint8_t *, 0xc2eb) + sector * 0x1e);
        P(uint8_t, s, 0x1c) |= (uint8_t)(1u << (player & 0x1f));
        if (P(uint8_t, s, 0x1a) & (1u << (player & 0x1f)))
            sector_seen(sector, id);
        sector_enter(u, s, (int16_t)sector);
    }
    if (I(uint8_t, 0xeeb) & 0x10)
        DebugStr((const char *)ORIG_PTR(0x10118b90), I(uint32_t, 0xc2ff));
}

/* 0x1000c898: the unit's sprite follows its state (depth, mirroring, cell, position) and
 * its marker (selection colour: own, other, multiplayer; health bar) */
void __cdecl unit_sprite_update(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    void *s = P(void *, u, 4);
    int level = P(int16_t, u, U_Y) + 10000;
    if (P(uint8_t, u, U_SEL_STATE) == 1)
        return;
    if (P(int16_t, u, 0x66) != level && P(int32_t, u, U_CLASS) != 100 && (P(uint32_t, u, U_FLAGS) >> 3 & 1)) {
        P(int16_t, u, 0x66) = (int16_t)level;
        if (P(uint8_t, u, 0x147) == 'E')
            P(int16_t, u, 0x66) = 0;
        SpriteSetLevel(HSL, s, P(int16_t, u, 0x66));
    }
    SpriteSetRedraw(HSL, s, 0, 0);
    if (P(uint32_t, u, U_FLAGS2) >> 8 & 1) {
        SpriteSetFlip(HSL, s, P(uint8_t, u, 0x159) == 1, 0);
        P(uint32_t, u, U_FLAGS2) &= 0xfffffeff;
    }
    SpriteReplaceCellRange(HSL, P(void *, u, 8), s, P(uint16_t, u, 0x145), 0, 1);
    SpriteSetPosition(HSL, s, P(int16_t, u, U_X), P(int16_t, u, U_Y), 0);
    if (!P(uint8_t, u, U_SELECTED)) {
        G(uint32_t, 0x1015728c) = 0;
        G(uint32_t, 0x10157290) = 0xffffffff;
        G(uint32_t, 0x10157294) = 0xffffffff;
    } else {
        uint32_t other = P(uint8_t, u, U_PLAYER) != I(uint32_t, ISO_LOCAL_PLAYER);
        G(uint32_t, 0x1015728c) = (I(int, 0x175) ? 2u : 0u) + other + 1;
        G(uint32_t, 0x10157290) = 0;
        G(int32_t, 0x10157294) = P(int16_t, u, 0x8c + P(uint8_t, u, 0x159) * 2);
    }
    G(uint32_t, 0x10157298) = (I(uint8_t, 0xeea) & 8) ? 0xb : 0xffffffff;
    SpriteSendMessage(HSL, s, 7, ORIG_PTR(0x10157288));
    if (P(uint8_t, u, U_SEL_STATE) == 2 && I(uint32_t, 0x17dd) > 2)
        P(uint8_t, u, U_SEL_STATE) = 1;
}

/* 0x1000cb68: whether a unit (a structure being put down) has room: a unit standing on its
 * middle cell is removed, as are units of the sector on the same cell when the cell has
 * places taken. 0: no room. */
int __cdecl unit_clear_ground(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return 1;
    int x = P(int16_t, u, U_X), tx, ty;
    int y = P(int16_t, u, U_Y);
    RECT r;
    SpriteGetBounds(HSL, P(void *, u, 4), &r);
    int h = r.bottom - y;
    h = h < 0 ? -h : h;
    int cy = y + h / 2, cx = x;
    int cell = IsoVTilePos(g_iso, &cx, &cy, &tx, &ty);
    uint32_t c = I(uint32_t *, 0x1934)[((uint32_t)cy << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)cx];
    uint32_t other = c >> 16 & 0xfff;
    if (other && other != (id & 0xffff) && iso_unit((int)other)) {
        unit_remove(other, 0x20, 1, 0);
        return 1;
    }
    if ((c >> 3 & 1) || !(c >> 8 & 0xf))
        return 0;
    int sx = (I(int32_t, 0xd6f1) >> 1) + P(int16_t, u, U_X);
    int sy = P(int16_t, u, U_Y) + (I(int32_t, 0xd6f5) >> 1);
    int sector = ((sy >> 8) << (I(uint8_t, 0xc2f5) & 0x1f)) + (sx >> 8);
    uint16_t v = P(uint16_t, I(uint8_t *, 0xc2eb) + sector * 0x1e, 2);
    while (v) {
        uint16_t next = P(uint16_t, iso_unit(v), 0x393);
        if (P(int32_t, iso_unit(v), U_CELL) == cell)
            unit_remove(v, 0x20, 1, 0);
        v = next;
    }
    return 1;
}
