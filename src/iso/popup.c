/* The game logic, 0x1001782a-0x10018711: the popup menu of a unit or of the selection
 * (which items it has and which are ticked), the mouse over a unit, and telling a click
 * from a drag. */
#include "iso_core.h"
#include "power.h"
#include "screen.h"
#include "mouse.h"
#include "cursor.h"
#include "records.h"
#include "buttons.h"
#include "popup.h"

/* not rewritten yet (names ours, provisional) */
#define unit_locked ORIG(int(__cdecl *)(uint32_t), 0x10042ef3)
#define unit_can_order ORIG(uint32_t(__cdecl *)(uint16_t), 0x100f4301)
#define can_repair ORIG(int(__cdecl *)(char, uint8_t), 0x100a60bb)
#define IsoUnitCanEnter ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x100f0c6c)
#define IsoShowPopupMenu \
    ORIG(void(__stdcall *)(uint8_t *, uint64_t, uint64_t, uint64_t, int, int, uint32_t), 0x100d2e41)

/* per unit category (unit_category): the items of its menu for one unit, and those
 * it keeps when several units are selected */
typedef struct { uint64_t single, multi; } MenuItems;
#define g_menu_items ((const MenuItems *)ORIG_PTR(0x10130480))

#define ISO_FOCUS_UNIT 0xdac6
#define ISO_REPAIR_TYPES 0x1980 /* table: +0 rows, +0x2c row width in ints */
#define SEL_CAN I(uint16_t, 0xe099)
#define player_field(off) I(int32_t, (off) + I(int32_t, ISO_LOCAL_PLAYER) * ISO_PLAYER_SIZE)

/* 0x10017e38: the number of items (bits) of a menu */
int __cdecl menu_count(uint32_t lo, int32_t hi)
{
    uint64_t v = (uint64_t)(uint32_t)hi << 32 | lo;
    int n = 0;
    for (int i = 0; i < 64; i++, v >>= 1)
        n += (int)(v & 1);
    return n;
}

/* 0x10017e8e: the items of a menu, and the ticked ones, adjusted to one unit */
void __cdecl menu_for_unit(uint16_t id, uint64_t *items, uint64_t *ticked)
{
    uint8_t *u = iso_unit(id);
    uint8_t type = P(uint8_t, u, U_ID_TYPE);
    int32_t player = I(int32_t, ISO_LOCAL_PLAYER);
#define BOTH_OFF(m) (*items &= ~(uint64_t)(m), *ticked &= ~(uint64_t)(m))
#define BOTH_ON(m) (*items |= (uint64_t)(m), *ticked |= (uint64_t)(m))
    if (unit_category(id) == 0x11) {
        /* the players to give it to: allies it reaches */
        uint8_t reach = power_player_reach(u);
        for (int i = 0; i < 8; i++)
            if (player == i || !((1 << i) & I(uint8_t, 0x1ba0 + player * ISO_PLAYER_SIZE)) || !((1 << i) & reach))
                BOTH_OFF(1ull << (32 + i));
    }
    if (P(uint8_t, u, 0xf3) && !P(uint8_t, u, 0x147)) {
        BOTH_ON(0x1000000);
        if (type == 0x22)
            BOTH_ON(0x400000);
    }
    if (P(int32_t, u, U_CLASS) == 0x38 && P(uint8_t, u, 0x147) != 'E' && !IsoUnitCanEnter(g_iso, id))
        BOTH_OFF(0x800000);
    if (P(uint8_t, u, 0x147) == 'F' || P(uint8_t, u, 0x147) == 'G' || P(uint8_t, u, 0x196) == ';')
        BOTH_OFF(0x600000);
    if (P(int32_t, u, U_CLASS) == 0x38 && P(uint8_t, u, 0x147) != 'E')
        BOTH_OFF(0x18ull << 48);
    if (P(uint8_t, u, 0x196) == 8 || P(uint8_t, u, 0x196) == 0xb)
        BOTH_OFF(0x300);
    if (type >= 0x40 && type <= 0x9f) {
        int32_t *t = I(int32_t *, ISO_REPAIR_TYPES);
        if ((P(uint32_t, u, U_FLAGS) & 0x100) || P(uint8_t, u, 0x30) != 1 ||
            ((int32_t *)(uintptr_t)t[0])[t[0xb] * (P(int32_t, u, U_ID_TYPE) - 0x40)] == -1 ||
            !can_repair((char)P(int32_t, u, U_CLASS), (uint8_t)player))
            BOTH_OFF(0x40000);
    }
    if (type >= 0x40 && type <= 0x9f && unit_can_order(id) && power_unit_counts(u))
        BOTH_ON(0x20000);
    if (P(int32_t, u, U_CLASS) == 0x50)
        BOTH_OFF(0x20ull << 48);
    if (!(P(uint32_t, u, U_FLAGS) & 0x800))
        BOTH_OFF(0x40ull << 48);
    if (P(int32_t, u, U_CLASS) != 0x34)
        BOTH_OFF(0x4000000);
    if ((type >= 0x40 && type <= 0x4f && P(int32_t, u, U_CLASS) != 0x40) ||
        (type < 0x40 && P(int32_t, u, U_CLASS) != 0x34 && P(uint8_t, u, 0x147) != 'E'))
        BOTH_ON(0x20000000);
    if (P(int32_t, u, 0x84) == 1 && P(uint8_t, u, 0x196) != '#')
        BOTH_ON(0x8000000);
#undef BOTH_OFF
#undef BOTH_ON
}

/* 0x1001782a: opens the popup menu at (x, y) for unit `id` (`selection` 0: the menu
 * without a unit; else for the local player's selection) */
void __cdecl popup_menu_open(int x, int y, uint32_t id, int selection)
{
    uint64_t items = 0, ticked = ~0ull, state = 0;
    int32_t n = SELECTED_N(I(int32_t, ISO_LOCAL_PLAYER));
    int multi = n > 1;
    if ((id & 0xffff) && iso_unit((int)(id & 0xffff)) && unit_locked(id))
        return;
    selection_abilities();
    I(uint16_t, ISO_FOCUS_UNIT) = (uint16_t)id;
    if (!selection) {
        ticked = g_menu_items[0].single;
        items = ticked;
        if (!player_field(0x2db3))
            ticked &= ~0x6000ull;
        if (!player_field(0x2dbf))
            ticked &= 0x3fffffffull;
        if (I(int, 0x175)) {
            items |= 0x800000ull << 32;
            ticked |= 0x800000ull << 32;
        }
    } else {
        for (int i = 0; i < n; i++) {
            uint16_t v = selected(I(uint32_t, ISO_LOCAL_PLAYER))[i];
            if (!iso_unit(v) || unit_locked(v))
                continue;
            uint8_t c = unit_category(v);
            items |= g_menu_items[c].single;
            ticked &= multi ? g_menu_items[c].multi : g_menu_items[c].single;
        }
        for (int i = 0; i < n; i++) {
            uint16_t v = selected(I(uint32_t, ISO_LOCAL_PLAYER))[i];
            uint8_t *u = iso_unit(v);
            if (!u || unit_locked(v))
                continue;
            menu_for_unit(v, &items, &ticked);
            int32_t t = P(int32_t, u, U_ID_TYPE);
            if ((t >= 0x18 && t < 0x20) || (t >= 8 && t <= 0xb) || (t >= 0x24 && t < 0x30)) {
                items |= 0x18;
                if (!multi)
                    ticked |= 0x18;
            }
            if (P(int16_t, u, 0xe6) != -1 || (t >= 0x84 && t <= 0x87)) {
                items |= 0x800;
                if (!multi)
                    ticked |= 0x800;
            }
        }
    }
    if ((SEL_CAN & 0x80) && !(SEL_CAN & 2)) {
        items |= 0x10080000;
        ticked |= 0x10080000;
        if (player_field(0x2db3)) {
            items |= 0x1000;
            ticked |= 0x1000;
        }
    }
    if (SEL_CAN & 0x400) {
        items |= 0xe0;
        ticked |= 0xe0;
    }
    if (!(SEL_CAN & 8)) {
        items &= ~0x20000ull;
        ticked &= ~0x20000ull;
    }
    if (multi)
        items &= 0x0000ffffffffffffull;
    for (int a = 0x30; a < 0x40; a++)
        if (iso_check_state((uint32_t)a, 0))
            state |= 1ull << a;
    if (!I(int32_t, 0xdac2))
        items = ticked;
    if ((id & 0xffff) && P(int32_t, iso_unit((int)(id & 0xffff)), U_CLASS) == 0x50 && (state & (1ull << 0x33)))
        ticked &= ~(1ull << 0x33);
    if (items) {
        IsoHideUnitNamePopup(g_iso);
        int count = menu_count((uint32_t)items, (int32_t)(items >> 32));
        if (count < 0x10)
            IsoShowPopupMenu(g_iso, items, ticked, state, x, y, id);
        else
            DebugStr((const char *)ORIG_PTR(0x10130654), count, 0xf);
        IsoSetMouseCursorOp(g_iso, 1);
    }
}

/* 0x1001836f: whether the mouse is over unit `id` (shown, selectable, not inside
 * something); its bounds are refreshed in passing. Sets *hit to the unit. */
int __cdecl mouse_over_unit(uint16_t id, uint16_t *hit)
{
    uint8_t *u = iso_unit(id);
    uint32_t f = P(uint32_t, u, U_FLAGS);
    if (!(f & 0x40) || !(f & 0x400) || (P(uint32_t, u, 0x10e) & 0x8000) || (f & 0x10000))
        return 0;
    SpriteGetBounds(I(void *, ISO_HSL), P(void *, u, 4), (RECT *)(u + 0xc6));
    if (P(int32_t, u, U_CLASS) < 0x18) {
        P(int32_t, u, 0xca) -= 7;
        P(int32_t, u, 0xc6) -= 7;
        P(int32_t, u, 0xd2) += 7;
        P(int32_t, u, 0xce) += 7;
    }
    int32_t t = P(int32_t, u, U_ID_TYPE);
    int in_box = 0;
    if (t < 0x40 || (t >= 0x84 && t <= 0x87)) {
        /* infantry and relays: anywhere in the box */
        int32_t x = I(int32_t, 0xcd82) + I(int32_t, 0xd76d), y = I(int32_t, 0xcd86) + I(int32_t, 0xd771);
        in_box = x >= P(int32_t, u, 0xc6) && x < P(int32_t, u, 0xce) && y >= P(int32_t, u, 0xca) &&
                 y < P(int32_t, u, 0xd2);
    }
    if (!in_box && !SpriteIsPicked(I(void *, ISO_HSL), P(void *, u, 4), I(int32_t, 0xd76d), I(int32_t, 0xd771)))
        return 0;
    *hit = id;
    return 1;
}

static int32_t abs32(int32_t v) { return v > 0 ? v : -v; }

/* 0x1001854c: the mouse went down and up: a click (1, *kind 0 plain, 1, 2, 3 with
 * the modifiers at 0xd7af, 0xd79b, 0xd797; *id the unit under it) or a drag (0,
 * *kind 4). The drag box (screen) is moved to map coordinates in passing. */
int __cdecl mouse_click_kind(uint8_t *kind, uint16_t *id)
{
    I(int32_t, 0xbf34) += I(int32_t, 0xcd82);
    I(int32_t, 0xbf2c) += I(int32_t, 0xcd82);
    I(int32_t, 0xbf30) += I(int32_t, 0xcd86);
    I(int32_t, 0xbf38) += I(int32_t, 0xcd86);
    if (abs32(I(int32_t, 0xbf2c) - I(int32_t, 0xbf34)) <= 10 &&
        abs32(I(int32_t, 0xbf30) - I(int32_t, 0xbf38)) <= 10) {
        *id = I(uint16_t, 0xd781);
        *kind = I(int32_t, 0xd7af) ? 1 : I(int32_t, 0xd79b) ? 2 : I(int32_t, 0xd797) ? 3 : 0;
        return 1;
    }
    *id = 0;
    *kind = 4;
    return 0;
}
