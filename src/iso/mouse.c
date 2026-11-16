/* The game logic, 0x10012852-0x100147d0: the mouse over the map: the cursor for the
 * current operation and for the unit under the mouse (move, attack, enter, repair, load,
 * ...), whether a cell can take the operation, and the unit name popup's text. */
#include "iso_core.h"
#include "power.h"
#include "mouse.h"

/* not rewritten yet (names ours, provisional) */
#define IsoTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, int *, int *, int *), 0x100af006)
#define unit_can_order ORIG(uint32_t(__cdecl *)(uint16_t), 0x100f4301)
#define can_repair ORIG(int(__cdecl *)(char, uint8_t), 0x100a60bb)
#define unit_cargo ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100a36d6)
#define unit_reachable ORIG(int(__cdecl *)(uint32_t, int, int), 0x1006b633)
#define IsoUnitCanEnter ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x100f0c6c)
#define BF_BlastText                                                                         \
    ORIG(int(__cdecl *)(void *, void *, uint32_t, void *, int, int, const char *, int), 0x101120b6)

#define APP I(void *, ISO_APP)
#define SEL_CAN I(uint16_t, 0xe099) /* what the selection can do (selection_abilities) */
#define CUR_OP I(int32_t, 0xd77d)
#define MOUSE_X I(int32_t, 0xd76d)
#define MOUSE_Y I(int32_t, 0xd771)
#define ISO_TICK 0x17dd

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

static uint32_t local_player(void) { return I(uint32_t, ISO_LOCAL_PLAYER); }
static uint8_t allies_of(uint32_t player) { return I(uint8_t, player * ISO_PLAYER_SIZE + 0x1ba0); }

/* 0x10012d3b: whether the cell under (x, y) can take a move (building: a structure); it
 * must be free and seen by the local player (for moves, unseen is fine unless the
 * selection has ability 0x1000) */
int __cdecl cell_free_for_cursor(int x, uint32_t y, int building)
{
    int tx = x, ty = (int)y, a, b;
    int cell = IsoTilePos(g_iso, &tx, &ty, &a, &b);
    int n = I(int32_t, ISO_MAP_H) * I(int32_t, ISO_MAP_W);
    if (cell < 0 || cell >= n)
        return 0;
    uint32_t c = I(uint32_t *, 0x1934)[((uint32_t)ty << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)tx];
    int seen = (uint8_t)(I(uint32_t *, ISO_MAP)[cell] >> 24) & (uint8_t)(1u << (local_player() & 0x1f));
    return (!(c & 1) || (SEL_CAN & 0x800)) && !(c & 2) && !(c & 8) && !(c >> 8 & 0xf) && !(c & 4) && !(c >> 0xc & 1) &&
           !(c >> 0xd & 1) && !(c >> 0xe & 1) && (seen || (!building && !(SEL_CAN & 0x1000)));
}

/* 0x10012852: the mouse this frame: whether units are selected, whether the mouse is over
 * the map, the cursor for the operation, and whether the operation needs no unit
 * (`fixed`) */
void __cdecl mouse_state(int *selected_any, int *unused, int *over_map, uint32_t *cursor, int *fixed)
{
    (void)unused;
    *selected_any = SELECTED_N(local_player()) != 0;
    *over_map = !(MOUSE_X < I(int32_t, 0xd787) || I(int32_t, 0xd78f) <= MOUSE_X || MOUSE_Y < I(int32_t, 0xd78b) ||
                  I(int32_t, 0xd793) <= MOUSE_Y);
    if (CUR_OP == 99) {
        *cursor = 99;
        return;
    }
    if (I(int, 0xdaf0) || !*over_map) {
        *cursor = 1;
        return;
    }
    int32_t op;
    if (I(int, 0xd79b) && (SEL_CAN & 0x400) && CUR_OP == 1) {
        op = 0xf;
        I(uint8_t, ISO_FLAGS) |= 0x40;
    } else {
        op = CUR_OP;
        I(uint8_t, ISO_FLAGS) &= 0xbf;
    }
    switch (op) {
    case 6:
        *cursor = 7;
        *fixed = 1;
        break;
    case 8:
        *cursor = 9;
        *fixed = 1;
        break;
    case 10:
        *cursor = 0xb;
        *fixed = 1;
        break;
    case 0xe: case 0xf: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1f: case 0x20: case 0x21: case 0x22: {
        *fixed = 1;
        int ok = cell_free_for_cursor(MOUSE_X, (uint32_t)MOUSE_Y, 1);
        if (op <= 0xf)
            *cursor = ok ? 0xe : 0xf;
        else if (op <= 0x1b)
            *cursor = ok ? 0x18 : 0x19;
        else if (op <= 0x20)
            *cursor = ok ? 0x1f : 0x20;
        else
            *cursor = ok ? 0x21 : 0x22;
        break;
    }
    case 0x14:
        *cursor = 0x15;
        *fixed = 1;
        break;
    case 0x1c:
        *cursor = 0x1d;
        *fixed = 1;
        break;
    default:
        if (I(int, 0x134)) {
            *cursor = (uint32_t)CUR_OP;
            *fixed = 1;
            break;
        }
        if (!*selected_any) {
            *cursor = (uint32_t)CUR_OP;
        } else {
            *cursor = 1;
            if (SEL_CAN & 0x80)
                *cursor = cell_free_for_cursor(MOUSE_X, (uint32_t)MOUSE_Y, 0) ? 0xc : 0xd;
        }
        I(uint32_t, 0xd783) = *cursor != 0xd;
        if (*selected_any && I(int, 0xd7af) && (SEL_CAN & 0x80))
            *cursor = *cursor == 0xc ? 0x12 : 0x13;
        break;
    }
}

/* the unit's name: its kind's name table by side (+0x16) and class */
static const char *unit_name(uint8_t *u)
{
    return (const char *)ORIG_PTR(0x1013bee8) + P(uint8_t, u, 0x16) * 0x520 + (P(uint32_t, u, U_CLASS) & 0xfffffffc) * 8;
}

/* the popup's text for a unit (own, allied or enemy), drawn into the popup's bitmap; 0 when
 * the bitmap is missing */
static int popup_text(uint32_t id, uint8_t *u)
{
    char text[64];
    uint32_t local = local_player();
    uint8_t player = P(uint8_t, u, U_PLAYER);
    uint8_t colour_char = G(uint8_t, 0x10118a5c + I(uint8_t, player * ISO_PLAYER_SIZE + 0x1b9f));
    if (player == local) {
        int32_t cls = P(int32_t, u, U_CLASS);
        if (cls == 0x5c) {
            /* how full it is, in percent (halved when damaged) */
            uint32_t pct = 0;
            int32_t slot = P(int32_t, u, 0xfa);
            if (slot >= 0 && I(uint8_t, slot * 0x2b + 0x10ab) == 2) {
                int32_t cap = type_value(P(int32_t, iso_unit((int)id), U_ID_TYPE), 0x1c);
                uint32_t have = unit_cargo(id, (uint32_t)cap);
                pct = (uint32_t)ftol((long double)have / (long double)cap * (long double)G(double, 0x10113068));
                if (P(uint32_t, u, U_FLAGS) >> 0xc & 1)
                    pct >>= 1;
            }
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f70), (unsigned)colour_char, unit_name(u), pct);
        } else if (cls == 0x7c) {
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f60), colour_char + 1, unit_name(u),
                      I(uint32_t, player * ISO_PLAYER_SIZE + 0x1b97),
                      type_value(P(int32_t, u, U_ID_TYPE), 0x34) * I(int32_t, player * ISO_PLAYER_SIZE + 0x2e01));
        } else if (cls == 0x80) {
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f7c), unit_name(u), 100 - power_percent((int16_t)id));
        } else {
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f88), unit_name(u));
        }
        if (P(uint8_t, u, 0x30) > 1 && player == local)
            lstrcatA(text, (const char *)ORIG_PTR(0x10118f8c)); /* " (Adv)" */
    } else if (!((1u << (player & 0x1f)) & allies_of(local))) {
        if (!I(int, 0x175))
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f48), unit_name(u));
        else
            wsprintfA(text, (const char *)ORIG_PTR(0x10118f54), g_iso + player * ISO_PLAYER_SIZE + 0x1b6d);
    } else {
        wsprintfA(text, (const char *)ORIG_PTR(0x10118f3c), g_iso + player * ISO_PLAYER_SIZE + 0x1b6d);
    }
    void *map;
    if (!((1u << (local & 0x1f)) & allies_of(player)))
        map = ORIG_PTR(0x101418d8);
    else if (player == local)
        map = ORIG_PTR(0x101428d8);
    else
        map = NULL;
    uint8_t *dib = *(uint8_t **)RtGetResource(APP, I(uint32_t, 0xd682));
    if (!dib)
        return 0;
    memset(P(void *, dib, 0x20), 0, P(uint32_t, dib, 8) * P(uint32_t, dib, 0xc));
    BF_BlastText(I(void *, 0x1812), dib, I(uint32_t, 0xd9a6), map, -1, 0, text, 0);
    unit_name_popup(id);
    return 1;
}

/* 0x10012ed3: the cursor over unit `id` (from the frame's cursor `cursor`, `mode` 1 for an
 * operation), and its name popup after the mouse has rested on it for 30 ticks */
uint32_t __cdecl cursor_for_unit(uint32_t id, int unused, int selected_any, int unused2, uint32_t cursor, int mode)
{
    (void)unused, (void)unused2;
    if (I(int, 0xdaf0))
        return cursor;
    if (CUR_OP == 99)
        return 99;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    uint32_t local = local_player();
    uint8_t player = P(uint8_t, u, U_PLAYER);
    uint32_t allied = allies_of(local) & (1u << (player & 0x1f));
    int x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), a, b;
    int cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
    if (cell < 0 || cell >= I(int32_t, ISO_MAP_H) * I(int32_t, ISO_MAP_W))
        return cursor;
    if (!(I(uint32_t *, ISO_MAP)[cell] >> 24 & (1u << (local & 0x1f))))
        return cursor; /* not seen */
    int32_t type = P(int32_t, u, U_ID_TYPE), cls = P(int32_t, u, U_CLASS);
    int enemy = player != local && !((1u << (player & 0x1f)) & allies_of(local));

    if (mode == 1) {
        if (!(I(uint8_t, ISO_FLAGS) >> 6 & 1)) {
            if (cursor == 0x21 || cursor == 0x22)
                return cursor;
            if (!I(int, 0xc30b) && (cursor == 0x1a || cursor == 0x1b || cursor == 0x18 || cursor == 0x19))
                return allied ? 0x1a : 0x1b;
            if (cursor == 0x15 && (I(int, 0xd797) || enemy))
                return 0x14;
            if (cursor == 0x1d && (I(int, 0xd797) || enemy) &&
                ((type > 0x3f && type < 0x50) || (type > 0x4f && type < 0x88)) && !(P(uint32_t, u, U_FLAGS) >> 0xc & 1))
                return 0x1c;
            if (player == local && type > 0x3f) {
                if (cursor == 7)
                    return 6;
                if (cursor == 9) {
                    if (player == local &&
                        (I(int, 0xd797) || (unit_can_order((uint16_t)id) && power_unit_counts(u))))
                        return 8;
                } else if (cursor == 0xb && P(uint8_t, u, 0x30) == 1) {
                    int32_t *t = I(int32_t *, 0x1980);
                    if (P(int32_t, (uintptr_t)t[0], t[0xb] * (type - 0x40) * 4) != -1 &&
                        can_repair((char)cls, (uint8_t)local))
                        return 10;
                }
            }
            return cursor;
        }
        if (!allied || I(int, 0xd797))
            return (SEL_CAN & 0x200) ? 0x14 : 0x15;
        return (SEL_CAN & 0x400) ? 0x1a : 0x1b;
    }
    if (cls >= 0x88 && cls <= 0x9f && !I(int, 0xd797))
        return cursor;

    /* the name popup */
    if (I(uint32_t, 0xd68a) == id) {
        if (!I(int, 0xd692) && !I(int, 0xdaf0) && I(uint32_t, 0xd68e) <= I(uint32_t, ISO_TICK)) {
            I(uint32_t, 0xd692) = 1;
            if (!popup_text(id, u))
                return 0;
        }
    } else {
        I(uint32_t, 0xd68a) = id;
        I(uint32_t, 0xd68e) = I(uint32_t, ISO_TICK) + 0x1e;
        if (I(int, 0xd692)) {
            void *s = RtGetResource(APP, I(uint32_t, 0xd686))[1];
            if (s)
                SpriteHide(I(void *, ISO_HSL), s, 1);
            I(uint32_t, 0xd692) = 0;
        }
    }
    if (!selected_any)
        return 0;

    int reach = unit_reachable(id, 1, I(int, 0xd797));
    int friendly = player == local || ((1u << (local & 0x1f)) & allies_of(player));
    int own = player == local;
    int nsel = SELECTED_N(local);
    if (SEL_CAN & 8)
        return 0;
    if (own && I(int, 0xd7af) && !I(int, 0xd79b))
        return 0;
    if (own && I(int, 0xd797) && !reach)
        return 0;
    if (friendly && !I(int, 0xd797) && I(int, 0xd79b))
        return (SEL_CAN & 0x400) ? 0x1a : 0x1b;
    if ((!friendly || I(int, 0xd797)) && I(int, 0xd79b))
        return (SEL_CAN & 0x200) ? 0x14 : 0x15;
    uint8_t *ti = P(uint8_t *, u, 0x32);
    if (nsel == 1 && P(uint8_t, u, U_SELECTED) && own) {
        if (P(int32_t, ti, 0xe0) && P(uint8_t, u, 0xf3))
            return 0x2e;
        if (cls == 0x38) {
            if (P(uint8_t, u, 0x147) != 'E' && !IsoUnitCanEnter(g_iso, id))
                return 0x11;
            return 0x10;
        }
    }
    if (own && (SEL_CAN & 0x10) && P(int32_t, ti, 0xe0) && !I(int, 0xd797))
        return (int)P(uint8_t, u, 0xf3) < P(int32_t, ti, 0xe0) ? 0x16 : 0x17;
    uint32_t flags = P(uint32_t, u, U_FLAGS);
    if (((type > 0x3f && type < 0x50) || (type > 0x4f && type < 0x88)) && (SEL_CAN & 1)) {
        if (!friendly)
            return (P(int16_t, u, U_POWER) != -1 && !(flags >> 0xc & 1)) ? 0x1c : 0x27;
        if ((flags >> 0xc & 1) || (flags >> 2 & 1))
            return 8;
    }
    if (I(int, 0xd797))
        friendly = 0;
    if (friendly && (SEL_CAN & 0x80) && cls >= 100 && cls <= 0x67) {
        if (I(uint16_t, 0xd6cb + local * 2) == id)
            return 0;
        int ready = !(SEL_CAN & 2) && P(int16_t, u, 0x42);
        if (flags >> 0x1a & 1)
            return ready && (SEL_CAN & 0x100) ? 0x28 : 0x29;
        if (!(flags >> 0xb & 1) || !(flags >> 0x1b & 1))
            return ready && I(int16_t, 0xd6f9 + local * 2) ? 0x2a : 0x2b;
        return ready ? 0x2c : 0x2d;
    }
    if (friendly && ((cls >= 0x18 && cls <= 0x2c && cls != 0x20) || cls == 0x38) && P(uint8_t, u, 0x147) != 'E' &&
        !P(int32_t, u, 0x84) && (SEL_CAN & 0x20) && !P(uint8_t, u, U_SELECTED))
        return 0x25;
    if ((!friendly || reach) && mode != 1)
        return reach ? 2 : ((SEL_CAN & 0x80) ? 0xd : 0);
    return 0;
}

/* 0x10014160: what the local player's selection can do, as bits in SEL_CAN (the cursors
 * and command buttons depend on it); returns how many units are selected.
 * A single unit of type 0x22 or 0x21 has a fixed set; otherwise the set starts full and
 * each selected unit's class takes away what it cannot do or adds what only it can. */
int __cdecl selection_abilities(void)
{
    uint32_t local = local_player();
    int n = SELECTED_N(local);
    uint16_t can = 0;
    uint8_t *u;
    if (n == 1 && (u = iso_unit(selected(local)[0]))) {
        if (P(int32_t, u, U_ID_TYPE) == 0x22)
            can = 0x15c0;
        else if (P(int32_t, u, U_ID_TYPE) == 0x21)
            can = 0x5a0;
    }
    if (!can) {
        can = 0xf00e;
        for (int i = 0; i < n; i++) {
            if (!(u = iso_unit(selected(local)[i])))
                continue;
            uint8_t c = P(uint8_t, u, U_CLASS);
            int entering = P(uint8_t, u, 0x147) == 'E';
            uint16_t t;
            if (c < 0x30 || c > 0x37)
                t = can & (P(int32_t, u, U_ID_TYPE) != 0x22 ? 0xeffd : 0xfffd);
            else
                t = can | 0x800;
            if (c < 0x40 || c > 0x4f)
                t &= 0xfffb;
            if (c < 0x50 || c > 0x9f)
                t &= 0xfff7;
            if (c < 0x14 && c != 8)
                t |= 0x10;
            if (c < 0x40 && !entering)
                t |= 0x80;
            if (c > 0xf && c < 0x14)
                t |= 0x101;
            if (c > 7 && (c < 0xc || c > 0x17) && (c < 0x30 || c > 0x37) && (c < 0x50 || c > 0x9f) && !entering)
                t |= 0x100;
            if ((c > 0x17 && c < 0x20) || (c > 7 && c < 0xc) || (c > 0x23 && c < 0x30))
                t |= 0x200;
            can = t;
            if (c < 0x40 && (c < 0x30 || c > 0x37) && !entering)
                can = t | 0x400;
        }
    }
    SEL_CAN = can;
    return n;
}

/* 0x100144c4: the name popup under a unit */
void __cdecl unit_name_popup(uint32_t id)
{
    void *s = RtGetResource(APP, I(uint32_t, 0xd686))[1];
    if (!s)
        return;
    uint8_t *u = iso_unit((int)(id & 0xffff));
    G(int32_t, 0x1015727c) = P(int16_t, u, U_X);
    G(int32_t, 0x10157280) = (P(uint16_t, u, 0xba) >> 1) + P(int16_t, u, U_Y) + 10;
    SpriteSendMessage(I(void *, ISO_HSL), s, 4, ORIG_PTR(0x10157278));
    G(uint32_t, 0x101572c4) = 0;
    G(uint32_t, 0x101572c8) = 0;
    G(uint32_t, 0x101572cc) = 0;
    SpriteSendMessage(I(void *, ISO_HSL), s, 4, ORIG_PTR(0x101572c0));
}

/* 0x100145d0: the scroll cursor: state's first cell plus the arrow for the direction of
 * (dx, dy) */
void __cdecl cursor_scroll(int state, int dx, int dy)
{
    int sx = dx > 0 ? 1 : dx < 0 ? -1 : 0, sy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    int32_t *t = I(int32_t *, 0xd765);
    int32_t first = P(int32_t, (uintptr_t)t[0], t[0xb] * state * 4);
    int32_t cell = first + G(int8_t, 0x10118f34 + sx * 3 + sy);
    int32_t cur = *I(int32_t *, 0x182a);
    if (cur != -1)
        SpriteHide(I(void *, ISO_HSL), RtGetResource(APP, (unsigned)cur)[1], 1);
    SpriteEmptyQueue(I(void *, ISO_HSL), I(void *, 0xd761));
    G(int32_t, 0x101572cc) = first;
    G(int32_t, 0x101572c4) = cell;
    G(int32_t, 0x101572c8) = cell;
    SpriteSendMessage(I(void *, ISO_HSL), I(void *, 0xd761), 4, ORIG_PTR(0x101572c0));
}

/* 0x10014745: the cursor sprite follows the mouse */
void __cdecl cursor_follow(void)
{
    if (I(int, 0xd761) && I(int, 0xd759) && I(int32_t, 0xd779) != 99)
        SpriteSetPosition(I(void *, ISO_HSL), I(void *, 0xd761), MOUSE_X, MOUSE_Y, 0);
}
