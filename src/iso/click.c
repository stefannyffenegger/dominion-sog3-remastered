/* The game logic, 0x10018711-0x10018edb and 0x10019b52-0x1001a434: a left click (or
 * drag) on the map: selecting, ordering the selection to a unit or a place, the box
 * selection. */
#include "iso_core.h"
#include "power.h"
#include "view.h"
#include "display.h"
#include "mouse.h"
#include "popup.h"
#include "orders.h"
#include "click.h"

/* not rewritten yet (names ours, provisional) */
#define unit_selectable ORIG(int(__cdecl *)(uint16_t), 0x100f4167)
#define unit_selected ORIG(void(__cdecl *)(uint32_t, int), 0x1004ebe6)
#define tutorial_event ORIG(int(__cdecl *)(uint8_t *, int, int, int), 0x1001b5ec)
#define order_move ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, int, int), 0x100b39aa)
#define unit_can_reach ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x1006b6d3)
#define unit_can_order ORIG(uint32_t(__cdecl *)(uint16_t), 0x100f4301)
#define effect_slot_take ORIG(uint32_t(__cdecl *)(uint8_t, uint16_t), 0x1006bbee)
#define effect_start                                                                         \
    ORIG(void(__cdecl *)(void *, int, int, int, uint32_t, int, int, int, int, uint32_t, uint32_t), 0x1006d9bd)
#define status_text ORIG(int(__cdecl *)(const char *, int), 0x10053ef5)
#define unit_sell ORIG(int(__cdecl *)(uint32_t, int), 0x1005ae71)
#define unit_repair ORIG(void(__cdecl *)(uint32_t, int), 0x1005b007)
#define unit_1005b314 ORIG(int(__cdecl *)(uint32_t, int), 0x1005b314)
#define order_guard ORIG(void(__cdecl *)(uint32_t, int, int, int, int), 0x10075bcf)
#define order_detonate_at ORIG(void(__cdecl *)(uint32_t), 0x10075b23)
#define order_place ORIG(void(__cdecl *)(uint32_t, int, int, int, int, int), 0x10073f90)
#define IsoActivateSelectionMode ORIG(void(__cdecl *)(uint8_t *, int, uint32_t, uint32_t), 0x10073acc)
#define unit_wants_follow ORIG(int(__cdecl *)(uint32_t), 0x1009530a)
#define unit_follow ORIG(void(__cdecl *)(uint32_t), 0x100955bf)
#define link_exists ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x1007a562)
#define link_remove ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1007a27b)
#define link_possible ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x10077c54)
#define link_add ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x10078dd5)
#define net_link ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x1003527c)
#define g_rand ORIG(int(__cdecl *)(void), 0x100f4d80)

typedef void(__cdecl *SpecialHandler)(uint32_t id);
#define unit_special_handler(type) G(SpecialHandler, 0x101545f4 + (type) * 0x44)

#define SEL_CAN I(uint16_t, 0xe099)
#define LOCAL I(int32_t, ISO_LOCAL_PLAYER)
#define MOD_ADD I(int32_t, 0xd7af)    /* the add-to-selection modifier is down */
#define MOD_ORDER I(int32_t, 0xd79b)  /* the order modifier is down */
#define MOD_FORCE I(int32_t, 0xd797)  /* the force (attack anything) modifier is down */
#define CLICK_X I(int32_t, 0xbf2c)    /* the press (and the drag box 0xbf2c-0xbf38) */
#define CLICK_Y I(int32_t, 0xbf30)

/* adds unit `id` to the selection list of `player` */
static void select_into(int32_t player, uint16_t id, uint8_t *u)
{
    selected((uint32_t)player)[SELECTED_N(player)] = id;
    SELECTED_N(player)++;
    P(uint8_t, u, U_SELECTED) = 1;
    if (P(uint8_t, u, U_SEL_STATE) == 1)
        P(uint8_t, u, U_SEL_STATE) = 2;
}

/* 0x10019b52: adds the local player's unit `id` to the selection; *tutorial gets the
 * tutorial's answer to the first selection */
int __cdecl selection_add(uint32_t id, int *tutorial)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!unit_selectable((uint16_t)id))
        return 0;
    select_into(LOCAL, (uint16_t)id, u);
    unit_selected(id, 0);
    if (!*tutorial)
        *tutorial = tutorial_event(u, 1, 0, 1);
    return 1;
}

/* 0x10019c41: selects another player's unit `id` alone, in its owner's list */
int __cdecl selection_foreign(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint32_t owner = P(uint8_t, u, U_PLAYER);
    if (!unit_selectable((uint16_t)id))
        return 0;
    selection_clear((int)owner);
    select_into((int32_t)owner, (uint16_t)id, u);
    unit_selected(id, 0);
    return 1;
}

/* 0x1001a3a0: the marker where a move order goes */
int __cdecl move_marker(int x, int y)
{
    uint32_t slot = effect_slot_take(0, 0);
    if ((int32_t)slot < 0 || (int32_t)slot >= (int32_t)I(uint16_t, 0xd6e7))
        return 0;
    effect_start(P(void *, I(uint8_t *, 0xd6e9) + 6 + slot * 0xe, 0), 19000, x, y, 0x684, 8, 0, 0, 0, 0x20, 0);
    return 1;
}

/* 0x10019d0a: a click on the ground with units selected: they move there */
int __cdecl click_move(void)
{
    if (I(uint8_t, 0xeea) & 2) {
        /* debug: paint the clicked tile with a random ground tile */
        int x = CLICK_X, y = CLICK_Y, cx, cy;
        int32_t cell = IsoVTilePos(g_iso, &x, &y, &cx, &cy);
        P(uint8_t, P(uint8_t *, I(uint8_t *, 0x1960), 0x10), cell) = (uint8_t)(g_rand() % 5 + 0x37);
        IsoRefreshScreen(g_iso);
    }
    if (!(SEL_CAN & 0x80))
        return 0;
    if (!SELECTED_N(LOCAL))
        return 0;
    int x = CLICK_X, y = CLICK_Y;
    order_move(LOCAL, x, (uint32_t)y, MOD_ADD ? 0 : 2, 10, 0, MOD_FORCE == 0);
    move_marker(x, y);
    for (int i = 0; i < SELECTED_N(LOCAL); i++) {
        uint16_t v = selected((uint32_t)LOCAL)[i];
        if (v && iso_unit(v) && !(P(uint32_t, iso_unit(v), U_FLAGS) & (1u << 0x19)) && unit_wants_follow(v) > 0)
            unit_follow(v);
    }
    return 1;
}

/* 0x10019f47: the drag box: selects the local player's units in it (added with the
 * add modifier) */
int __cdecl box_select(void)
{
    RECT r;
    if (!MOD_ADD)
        selection_clear(LOCAL);
    for (int i = 0; i < I(int32_t, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint16_t id = (uint16_t)v;
        uint8_t *u = iso_unit(id);
        if (!unit_selectable(id) || !id || !u || !(P(uint32_t, u, U_FLAGS) & 0x40) || (P(uint32_t, u, 0x10e) & 0x8000))
            continue;
        if (P(uint8_t, u, U_SEL_STATE) == 1)
            P(uint8_t, u, U_SEL_STATE) = 2;
        SpriteGetBounds(I(void *, ISO_HSL), P(void *, u, 4), (RECT *)(u + 0xc6));
        if (IntersectRect(&r, (RECT *)(g_iso + 0xbf2c), (RECT *)(u + 0xc6)) && P(uint8_t, u, U_PLAYER) == (uint32_t)LOCAL &&
            P(int32_t, u, U_ID_TYPE) < 0x50 && P(int32_t, u, U_CLASS) != 0x40 &&
            (P(uint8_t, u, 0x147) != 'E' || P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 1)) {
            select_into(LOCAL, id, u);
            unit_selected(id, 0);
        }
    }
    return 1;
}

/* 0x1001a166: the last selected structure and the clicked one (both of types 0x40-0x4f,
 * same owner): links them, or removes their link */
int __cdecl structure_link(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint16_t last = selected((uint32_t)LOCAL)[SELECTED_N(LOCAL) - 1];
    uint8_t *l = iso_unit(last);
    if (!last || !l)
        return 0;
    int32_t lt = P(int32_t, l, U_ID_TYPE), ut = P(int32_t, u, U_ID_TYPE);
    if (lt < 0x40 || lt > 0x4f || ut < 0x40 || ut > 0x4f || P(uint8_t, l, U_PLAYER) != P(uint8_t, u, U_PLAYER) ||
        (id & 0xffff) == last)
        return 0;
    if (unit_direction(last, id) == -1 || (P(uint32_t, l, U_FLAGS) & 0x1000) || (P(uint32_t, u, U_FLAGS) & 0x1000) ||
        !P(int16_t, l, 0x42) || !P(int16_t, u, 0x42))
        return 0;
    if (link_exists(last, id)) {
        if (I(int, 0x175))
            net_link(last, id, 0);
        link_remove(last, id);
        return 1;
    }
    if (link_possible(last, id)) {
        link_add(last, id, 0, 1);
        if (I(int, 0x175))
            net_link(last, id, 1);
        return 1;
    }
    return 0;
}

/* 0x10018711: a left click (or a drag) on the map has ended */
int __cdecl map_click(void)
{
    int tutorial = 0, tutorial2 = 0, done = 0;
    uint8_t kind;
    uint16_t id;
    if (I(int32_t, 0xdaf0))
        return 0;
    int click = mouse_click_kind(&kind, &id);
    if (!click || !id) {
        if (!click)
            return box_select();
        if (!MOD_ORDER)
            return click_move();
        if ((SEL_CAN & 0x400) && cell_free_for_cursor(I(int32_t, 0xd76d), I(uint32_t, 0xd771), 1)) {
            IsoActivateSelectionMode(g_iso, 0x3f2, 1, 0);
            order_place(I(uint32_t, 0xc30f), CLICK_X - I(int32_t, 0xcd82), CLICK_Y - I(int32_t, 0xcd86), 1, 0, 0);
            return 1;
        }
        return 0;
    }
    uint8_t *u = iso_unit(id);
    if (!u)
        return 0;
    uint32_t owner = P(uint8_t, u, U_PLAYER);
    int own = owner == (uint32_t)LOCAL;
    int friendly = (own || ((1 << (I(uint8_t, ISO_LOCAL_PLAYER) & 0x1f)) & I(uint8_t, 0x1ba0 + owner * ISO_PLAYER_SIZE))) &&
                   !MOD_FORCE;
    if (own) {
        switch (I(int32_t, 0xd77d)) { /* the cursor's operation */
        case 6:
            return unit_sell(id, 0);
        case 8:
            if (MOD_FORCE || (unit_can_order(id) && power_unit_counts(u)))
                unit_repair(id, 0);
            return 0;
        case 10:
            return unit_1005b314(id, 0);
        }
    }
    int32_t n = SELECTED_N(LOCAL);
    if (P(int32_t, u, U_CLASS) >= 0x88 && P(int32_t, u, U_CLASS) <= 0x9f && !MOD_FORCE && (SEL_CAN & 0x80))
        return click_move();
    if (own && MOD_ADD && !MOD_ORDER) {
        if (P(uint8_t, u, U_SELECTED)) {
            selection_remove(id, (uint32_t)LOCAL, 1);
            return 1;
        }
        return selection_add(id, &tutorial);
    }
    if (n) {
        if (n == 1) {
            uint16_t first = selected((uint32_t)LOCAL)[0];
            if (own && P(uint8_t, u, U_SELECTED)) {
                SpecialHandler h = unit_special_handler(P(int32_t, u, U_ID_TYPE));
                if (h)
                    h(id);
                return 1;
            }
            if (unit_can_reach(first, id, 1, 0))
                done = order_type21(first, id, &tutorial);
            else
                done = structure_link(id);
        }
        if (MOD_ORDER && !MOD_ADD) {
            if (friendly) {
                order_guard(id, 1, 0, 0, 0);
                status_text(ORIG_PTR(0x10130690), 2000);
            } else if (SEL_CAN & 0x200) {
                order_detonate_at(id);
                status_text(ORIG_PTR(0x101306b4), 2000);
            }
            done = 1;
        }
        if (!done)
            for (int i = 0; i < n; i++) {
                uint16_t v = selected((uint32_t)LOCAL)[i];
                if (!v || !iso_unit(v))
                    continue;
                int32_t vt = P(int32_t, iso_unit(v), U_ID_TYPE);
                int engineer = vt >= 0x10 && vt <= 0x13;
                if (own && friendly && !engineer) {
                    if (P(int32_t, u, U_CLASS) == 100)
                        done |= order_enter(v, id, &tutorial);
                    else if (P(int32_t, P(uint8_t *, u, 0x32), 0xe0))
                        done |= order_board(v, id, &tutorial);
                }
                if (friendly) {
                    if (engineer)
                        done |= order_engineer(v, id, &tutorial, &tutorial2);
                } else {
                    done |= order_attack(v, id, &tutorial, &tutorial2);
                }
            }
    }
    if (own && !done) {
        for (int p = 0; p < I(int32_t, 0x1b1f); p++)
            if (LOCAL != p || !MOD_FORCE)
                selection_clear(p);
        selection_add(id, &tutorial);
    } else if (!own && !done) {
        for (int p = 0; p < I(int32_t, 0x1b1f); p++)
            selection_clear(p);
        done = selection_foreign(id);
    }
    return done;
}
