/* The game logic, 0x100065c3-0x10008772: the display loop (one frame: units drawn or
 * hidden, the unit under the mouse, the cursor, effects, the interface), the map
 * editor's loop, the debug sprites, "next unit" and selecting groups. */
#include "iso_core.h"
#include "radarmap.h"
#include "healthbar.h"
#include "mouse.h"
#include "cursor.h"
#include "view.h"
#include "display.h"
#include "loop.h"
#include "screen.h"
#include "statelog.h"
#include "popup.h"

/* not rewritten yet (names ours, provisional) */
#define IsoBriefingScreen ORIG(void(__cdecl *)(uint8_t *, int), 0x1005b438)
#define IsoShowObjects ORIG(void(__stdcall *)(uint8_t *, int), 0x100ae0ae)
#define IsoNetWindow ORIG(void(__stdcall *)(uint8_t *, int, int, int), 0x10046d25)
#define IsoSetFollowUnit ORIG(void(__cdecl *)(uint8_t *, int), 0x10055f9f)
#define BF_BlastText                                                                         \
    ORIG(int(__cdecl *)(void *, void *, uint32_t, void *, int, int, const char *, int), 0x101120b6)
#define unit_cost ORIG(void(__cdecl *)(int, int *, int *), 0x100a5423)
#define status_text ORIG(int(__cdecl *)(char *, int), 0x10053ef5)
#define radar_update ORIG(int(__cdecl *)(void), 0x100a25d0)
#define view_follow ORIG(void(__cdecl *)(int, int), 0x100549b0)
#define objectives_window ORIG(void(__cdecl *)(int), 0x10047656)
#define unit_shade ORIG(int(__cdecl *)(uint16_t), 0x10076ca3)
#define unit_shade_done ORIG(int(__cdecl *)(uint16_t), 0x10076af2)
#define structure_shade ORIG(int(__cdecl *)(uint16_t), 0x10076e53)
#define unit_draw_hidden ORIG(int(__cdecl *)(uint8_t *, uint32_t, int), 0x10050e74)
#define unit_effects ORIG(int(__cdecl *)(uint32_t), 0x1006d672)
#define find_event_pos ORIG(int(__cdecl *)(int *, int *, int, int), 0x1005c77a)
#define unit_selectable ORIG(int(__cdecl *)(uint16_t), 0x100f4167)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define group_clear ORIG(void(__cdecl *)(uint32_t), 0x10095214)
#define group_made ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1009519a)

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define U_FLAGS2 0x10e
#define U_KIND 0x196 /* byte; '#': not selectable */

/* the area around the view (view.c keeps it up to date) */
static int in_view(int x, int y)
{
    return x >= I(int32_t, 0xd6b7) && x < I(int32_t, 0xd6bf) && y >= I(int32_t, 0xd6bb) &&
           y < I(int32_t, 0xd6c3);
}

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x100065c3: one frame of the game's display */
int __stdcall IsoDisplayLoop(uint8_t *iso)
{
    int selected_any = 0, unused = 0, over_map = 0, fixed = 0;
    uint32_t cursor = 0, over = 0, over_unit = 0;
    uint8_t *last = NULL;
    g_iso = iso;
    statelog_frame();
    if ((I(int, 0x175) && !I(int, 0x17cd) && !I(uint8_t, 0xd90f)) ||
        ((I(uint8_t, 0x181) || I(uint8_t, 0x17e)) && I(uint32_t, 0x17cd) > 1))
        return 1;
    if (I(int, 0x17cd) == 1) {
        /* the first frame: the briefing goes, the map comes */
        IsoCheckRadarScreenAreaUpdate(iso);
        IsoBriefingScreen(g_iso, 0);
        IsoShowObjects(g_iso, 1);
        IsoRefreshScreen(g_iso);
        RtFadeIn(APP, 0);
        if (!I(int, 0x175)) {
            if (I(uint8_t, 0x17e))
                I(uint8_t, 0x17e) = 0;
            if (I(uint8_t, 0x181))
                I(uint8_t, 0x181) = 0;
        }
        if (I(int, 0x179b)) {
            uint8_t rec[0x22];
            memset(rec, 0, sizeof rec);
            P(uint32_t, rec, 2) = I(uint32_t, 0x179b);
            rec[9] = 0;
            RtRunMsg(APP, rec);
        }
    }
    I(int32_t, 0x17cd)++;
    for (int k = 1; k < 0xe; k++)
        G(uint32_t, 0x10157288 + k * 4) = 0xffffffff;
    selection_abilities();
    mouse_state(&selected_any, &unused, &over_map, &cursor, &fixed);

    uint32_t local = I(uint32_t, ISO_LOCAL_PLAYER);
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        uint32_t id = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        last = u;
        if (!u)
            continue;
        int32_t type = P(int32_t, u, U_ID_TYPE);
        int x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), visible;
        if (type < 0x30 || type > 0x37) {
            visible = in_view(x, y);
        } else {
            /* these have a part above them (a float height) that may be in view alone */
            visible = in_view(x, y);
            if (!visible && x >= I(int32_t, 0xd6b7) && x < I(int32_t, 0xd6bf)) {
                uint8_t *top = P(uint8_t *, u, 0x39d);
                int ty = y - (int16_t)ftol(P(float, top, 0x20));
                if (ty >= I(int32_t, 0xd6bb)) {
                    ty = y - (int16_t)ftol(P(float, top, 0x20));
                    visible = ty < I(int32_t, 0xd6c3);
                }
            }
        }
        uint32_t allied = I(uint8_t, local * ISO_PLAYER_SIZE + 0x1ba0) & (1u << (P(uint8_t, u, U_PLAYER) & 0x1f));
        if (P(int32_t, u, 0x84) == 1 && !allied)
            visible = 0;
        if (type < 0x40 || type > 0x9f) {
            int shade = unit_shade(P(uint16_t, u, 0x1cb));
            SpriteSetBlitFX(P(void *, u, 4), shade < 1 ? 0 : 1, 0);
            unit_shade_done(P(uint16_t, u, 0x1cb));
        } else {
            int shade = structure_shade(P(uint16_t, u, 0x1cb));
            if (shade < 0)
                SpriteSetBlitFX(P(void *, u, 4), 0, 0);
            else
                SpriteSetBlitFX(P(void *, u, 4), 3, (uint32_t)shade);
        }
        uint32_t flags = P(uint32_t, u, U_FLAGS);
        if (visible && (!(flags >> 16 & 1) || P(int8_t, u, 0x147) != 'M' || allied)) {
            if (!(flags >> 6 & 1) && P(int8_t, u, U_SEL_STATE) == 1)
                P(int8_t, u, U_SEL_STATE) = 2;
            P(uint32_t, u, U_FLAGS) |= 0x40;
            if (P(uint32_t, u, U_FLAGS2) >> 15 & 1) {
                if (!(P(uint32_t, u, U_FLAGS) >> 16 & 1) || !allied || !I(int, 300)) {
                    radar_plot(P(uint16_t, u, 0x98), 0, 0);
                    P(uint16_t, u, 0x98) = 0;
                } else {
                    unit_draw_hidden(u, allied, 0);
                }
                continue;
            }
            UnitHandler draw = unit_type_handler(P(int32_t, u, U_ID_TYPE));
            if (draw)
                draw(id);
        } else {
            if (flags >> 6 & 1) {
                SpriteHide(HSL, P(void *, u, 4), 1);
                if (P(int32_t, u, U_ID_TYPE) > 0x2f && P(int32_t, u, U_ID_TYPE) < 0x38)
                    SpriteHide(HSL, P(void *, P(uint8_t *, u, 0x39d), 4), 1);
            }
            P(uint32_t, u, U_FLAGS) &= 0xffffffbf;
        }
        if (I(int, 300))
            unit_draw_hidden(u, allied, 0);
        uint16_t hit;
        if (over_map && (!over || !over_unit) && mouse_over_unit((uint16_t)id, &hit)) {
            over = hit;
            over_unit = (uint32_t)P(int32_t, iso_unit(hit), U_ID_TYPE) < 0x40;
            cursor = cursor_for_unit(over, unused, selected_any, over_map, cursor, fixed);
            if (cursor == 0xb && (I(int, 0xd692) || I(uint32_t, 0xd68a) != over))
                IsoHideUnitNamePopup(iso);
        }
        if (P(int16_t, u, 0x1b3) >= 0)
            unit_effects(id);
    }

    /* the unit under the mouse: its name and figures */
    if (!over) {
        I(uint16_t, 0xd781) = 0;
        IsoHideUnitNamePopup(iso);
    } else {
        I(uint16_t, 0xd781) = (uint16_t)over;
        if ((cursor == 10 || cursor == 6) && I(uint32_t, 0xd68a) != over) {
            int a, b;
            IsoHideUnitNamePopup(iso);
            I(uint32_t, 0xd68a) = over;
            I(uint32_t, 0xd68e) = 0;
            I(uint32_t, 0xd692) = 1;
            if (cursor == 10) {
                a = type_value(P(int32_t, iso_unit((int)over), U_ID_TYPE), 0x28);
                b = type_value(P(int32_t, iso_unit((int)over), U_ID_TYPE), 0x2c);
            } else {
                unit_cost((int)over, &a, &b);
            }
            uint8_t c = G(uint8_t, 0x10118a5c + I(uint8_t, local * ISO_PLAYER_SIZE + 0x1b9f));
            char text[80];
            wsprintfA(text, (const char *)ORIG_PTR(0x10118aac), c + 1, a, (unsigned)c, b);
            uint8_t *dib = *(uint8_t **)RtGetResource(APP, I(uint32_t, 0xd682));
            if (!dib)
                return 0;
            memset(P(void *, dib, 0x20), 0, P(uint32_t, dib, 8) * P(uint32_t, dib, 0xc));
            BF_BlastText(I(void *, 0x1812), dib, I(uint32_t, 0xd9a6), ORIG_PTR(0x101428d8), -1, 0, text, 0);
            if (last) /* original: the last unit of the list, not this one */
                unit_name_popup(over);
        }
    }
    if (!I(int, 0xd7ab) && I(uint32_t, 0xd779) != cursor && !I(uint8_t, 0x17e))
        IsoSetMouseCursorState(g_iso, cursor, 1);
    if (I(uint8_t, 0xeea) & 1)
        IsoRefreshScreen(g_iso);
    uint32_t now = timeGetTime();
    if (I(uint32_t, 0x107b) < now - I(uint32_t, 0x1077)) {
        if (I(int, 0xbf06) && I(int, 300)) {
            G(uint32_t, 0x1015728c) = 2;
            G(uint32_t, 0x10157290) = 0;
            G(uint32_t, 0x10157294) = 3;
            G(uint32_t, 0x10157298) = 1;
            G(uint32_t, 0x1015729c) = 0xffffffff;
            SpriteSendMessage(HSL, I(void *, 0xbf1e), 7, ORIG_PTR(0x10157288));
        }
        I(uint32_t, 0x1077) = now;
    }
    if (I(int, 0xd8d0) && I(uint32_t, 0xd8d0) < now)
        status_text(NULL, 0);
    /* effects: drawn in view, hidden outside */
    for (int k = 0; k < I(int, 0x1b47); k++) {
        int32_t e = I(int32_t *, 0xd6e3)[k];
        uint8_t *p = I(uint8_t *, 0xd6db) + e * 0x44;
        if ((p[0x43] >> 3 & 1) || !P(int32_t, p, 0))
            continue;
        UnitHandler draw = G(UnitHandler, 0x10157150 + P(int32_t, P(uint8_t *, p, 0x16), 0x50) * 0x18);
        if (!draw)
            continue;
        int two = P(int32_t, p, 4) != 0;
        int shown = in_view(P(int16_t, p, 0x22), P(int16_t, p, 0x24)) ||
                    (two && in_view(P(int16_t, p, 0x2a), P(int16_t, p, 0x2c)));
        if (shown) {
            p[0x43] |= 4;
            draw((uint32_t)e);
        } else {
            if (p[0x43] >> 2 & 1) {
                SpriteHide(HSL, P(void *, p, 0), 1);
                if (two)
                    SpriteHide(HSL, P(void *, p, 4), 1);
            }
            p[0x43] &= 0xfb;
        }
    }
    if (I(uint8_t, 0xd696) == 1 && !(I(uint8_t, 0x17dd) & 7)) {
        radar_update();
        I(uint8_t, 0xd696) = 0;
    }
    if (I(int, 0x24)) { /* recording frames */
        RtCaptureFrame(APP, (char *)ORIG_PTR(0x10118ab8), I(uint16_t, 0xa16) + I(uint16_t, 0xa18) * 1000u);
        I(uint16_t, 0xa16)++;
    }
    if (I(int, 0x144)) {
        uint8_t *f = iso_unit(I(uint16_t, 0x17bb));
        if (!f)
            I(int32_t, 0x144) = 0;
        else
            view_follow(P(int16_t, f, U_X), P(int16_t, f, U_Y));
    }
    if (I(int, 0x175))
        messages_draw();
    if (I(int, 0x1af1))
        IsoNetWindow(iso, 2, -1, -1);
    if (I(int, 0xbe71))
        objectives_window(I(uint32_t, 0xbec1) < I(uint32_t, 0x17dd));
    if (I(int, 0xcd8a))
        countdown_draw();
    return 1;
}

/* 0x10007668: one frame of the map editor */
int __stdcall IsoMapEditDisplayLoop(uint8_t *iso, int sync)
{
    g_iso = iso;
    I(uint32_t, 0xd76d) = RtGetVar(APP, 0x138d);
    I(uint32_t, 0xd771) = RtGetVar(APP, 0x138e);
    view_scroll(0, 0, 0);
    for (int i = 0; i < I(int, ISO_UNIT_COUNT);) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u = iso_unit(id);
        if (u) {
            RtSetPos(APP, P(uint32_t, u, 0), P(int16_t, u, U_X), P(int16_t, u, U_Y));
            SpriteShow(HSL, P(void *, u, 4), P(uint8_t, u, 0x79), 1);
            UnitHandler draw = unit_type_handler(P(int32_t, u, U_ID_TYPE));
            if (draw)
                draw((uint32_t)id);
            if (P(int8_t, u, U_SEL_STATE) == 4)
                P(int8_t, u, U_SEL_STATE) = 2;
        }
        i++;
    }
    if (sync) {
        SpriteSetSync(HSL, 0);
        SpriteSetSync(HSL, 1);
    }
    return 1;
}

/* 0x1000788d: the two debug sprites (resource, value), 0 for none */
int __stdcall IsoSetDebugSprite(uint8_t *iso, uint32_t res1, uint32_t v1, uint32_t res2, uint32_t v2)
{
    g_iso = iso;
    if (!res1) {
        I(uint32_t, 0xeee) = 0;
        I(uint32_t, 0xefe) = 0;
        I(uint32_t, 0xf0e) = 0;
    } else {
        I(void *, 0xeee) = RtGetResource(APP, res1)[1];
        I(uint32_t, 0xefe) = res1;
        I(uint32_t, 0xf0e) = v1;
    }
    if (!res2) {
        I(uint32_t, 0xef2) = 0;
        I(uint32_t, 0xf02) = 0;
        I(uint32_t, 0xf12) = 0;
    } else {
        I(void *, 0xef2) = RtGetResource(APP, res2)[1];
        I(uint32_t, 0xf02) = res2;
        I(uint32_t, 0xf12) = v2;
    }
    return 1;
}

static void select_one(uint32_t player, uint8_t *u, uint16_t id)
{
    selected(player)[SELECTED_N(player)] = id;
    SELECTED_N(player)++;
    P(uint8_t, u, U_SELECTED) = 1;
    unit_health_bar(id, 0);
    if (P(int8_t, u, U_SEL_STATE) == 1)
        P(int8_t, u, U_SEL_STATE) = 2;
}

/* 0x100079bd: selects the player's next unit of a kind after list entry `start` (0 any,
 * 1 two kinds of 0x196, 2 damaged, 3 badly damaged, 4 energy well, 5 class 0x5c, 6 a
 * damaged structure, 7-8 type ranges, 9 class 0x20; 10 jumps to an event instead) and
 * follows it; returns the entry after it */
int __stdcall IsoFindNextUnit(uint8_t *iso, uint32_t start, uint32_t player, int kind)
{
    g_iso = iso;
    uint32_t end = I(uint32_t, ISO_UNIT_COUNT) & 0xffff, i;
    int pass = 1, match = 0;
    if (kind == 10) {
        int x, y;
        if (find_event_pos(&x, &y, (int)player, 0))
            view_follow(x, y);
        return 0;
    }
    i = (int)start < I(int, ISO_UNIT_COUNT) ? start : 0;
    for (;;) {
        while ((int)end <= (int)i) {
        done:
            if (pass != 1 || I(uint32_t, ISO_UNIT_COUNT) != i || I(int, ISO_UNIT_COUNT) <= (int)start)
                return (int)i + 1;
            pass = 2;
            i = 0;
            end = start & 0xffff;
        }
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint16_t id = (uint16_t)v;
        uint8_t *u = iso_unit(id);
        if (!u) {
            i++;
            continue;
        }
        if (P(uint8_t, u, U_PLAYER) == player && !(P(uint32_t, u, U_FLAGS2) >> 15 & 1) &&
            !(P(uint32_t, u, U_FLAGS) >> 16 & 1) && P(int8_t, u, U_KIND) != '#' && unit_selectable(id)) {
            int32_t type = P(int32_t, u, U_ID_TYPE), cls = P(int32_t, u, U_CLASS);
            switch (kind) { /* original: other kinds keep the previous unit's answer */
            case 0: match = 1; break;
            case 1: match = P(int8_t, u, U_KIND) == 0x14 || P(int8_t, u, U_KIND) == 0x18; break;
            case 2: match = P(int16_t, u, 0xd6) < (int16_t)unit_max_health(u, 0); break;
            case 3: match = P(int16_t, u, 0xd6) < (int16_t)((int16_t)unit_max_health(u, 0) >> 1); break;
            case 4: match = cls == CLASS_ENERGY_WELL; break;
            case 5: match = cls == 0x5c; break;
            case 6: match = type >= 0x40 && type <= 0x9f && (P(uint32_t, u, U_FLAGS) >> 12 & 1); break;
            case 7: match = type >= 0x10 && type <= 0x13; break;
            case 8: match = type >= 0x30 && type <= 0x37; break;
            case 9: match = cls == 0x20; break;
            }
        } else {
            match = 0;
        }
        if (id && match) {
            selection_clear((int)player);
            I(int32_t, 0x144) = 0;
            selected(player)[0] = id;
            SELECTED_N(player) = 1;
            P(uint8_t, u, U_SELECTED) = 1;
            unit_health_bar(id, 0);
            P(uint8_t, u, U_SEL_STATE) = 2;
            IsoSetFollowUnit(g_iso, 1);
            pass = 0;
            goto done;
        }
        i++;
    }
}

/* 0x10007e49: selections by group.
 * player -1: the local player selects by `kind`: 10 all (that can), 11 those in view,
 *   12 and 13 two type ranges, 14 the class of the last one selected.
 * else, make 0: selects group `kind` (below 1: every grouped unit), added to the
 *   selection with `add`; make 1: the local player's selection becomes group `kind`.
 * Returns how many are selected. */
int __stdcall IsoSelectGroup(uint8_t *iso, uint32_t player, uint32_t kind, int make, int add)
{
    g_iso = iso;
    if (player == 0xffffffff) {
        uint32_t p = I(uint32_t, ISO_LOCAL_PLAYER);
        int32_t cls = -1;
        if (kind == 0xe) {
            uint16_t lastsel = selected(p)[SELECTED_N(p) - 1];
            if (!iso_unit(lastsel))
                return 0;
            cls = P(int32_t, iso_unit(lastsel), U_CLASS);
        }
        selection_clear((int)p);
        for (uint16_t j = 0; (int)j < I(int, ISO_UNIT_COUNT); j++) {
            uint16_t id = (uint16_t)I(uint32_t *, ISO_UNIT_LIST)[j];
            uint8_t *u = iso_unit(id);
            if (!u || !unit_selectable(id))
                continue;
            if (P(uint8_t, u, U_PLAYER) != p || P(int8_t, u, U_KIND) == '#' || (P(uint32_t, u, U_FLAGS) >> 16 & 1) ||
                (P(uint32_t, u, U_FLAGS2) >> 15 & 1))
                continue;
            int ok = 1, x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y);
            int32_t type = P(int32_t, u, U_ID_TYPE);
            switch (kind) {
            case 10: ok = P(int16_t, u, 0xe6) >= 0 && P(int32_t, u, U_CLASS) != 0x38; break;
            case 0xb:
                ok = x >= I(int32_t, 0xd6b7) + 0x80 && x < I(int32_t, 0xd6bf) - 0x80 &&
                     y >= I(int32_t, 0xd6bb) + 0x80 && y < I(int32_t, 0xd6c3) - 0x80 &&
                     P(int16_t, u, 0xe6) >= 0 && P(int32_t, u, U_CLASS) != 0x38;
                break;
            case 0xc: ok = type >= 0x30 && type <= 0x37; break;
            case 0xd: ok = type >= 0x40 && type <= 0x9f; break;
            case 0xe: ok = P(int32_t, u, U_CLASS) == cls; break;
            }
            if (ok)
                select_one(p, u, id);
        }
        return SELECTED_N(p);
    }
    if (!make) {
        if (!add)
            selection_clear((int)player);
        for (uint16_t j = 0; (int)j < I(int, ISO_UNIT_COUNT); j++) {
            uint16_t id = (uint16_t)I(uint32_t *, ISO_UNIT_LIST)[j];
            uint8_t *u = iso_unit(id);
            if (!u)
                continue;
            if ((int)kind < 1 ? P(uint8_t, u, U_GROUP) != 0 && P(uint8_t, u, U_PLAYER) == player
                              : P(uint8_t, u, U_GROUP) == kind && P(uint8_t, u, U_PLAYER) == player &&
                                    !(P(uint32_t, u, U_FLAGS2) >> 15 & 1))
                select_one(player, u, id);
        }
        return SELECTED_N(player);
    }
    for (uint16_t j = 0; (int)j < I(int, ISO_UNIT_COUNT); j++) {
        uint8_t *u = iso_unit((uint16_t)I(uint32_t *, ISO_UNIT_LIST)[j]);
        if (u && P(uint8_t, u, U_GROUP) == kind) {
            P(uint8_t, u, U_GROUP) = 0;
            P(uint16_t, u, 0x1ab) = 0;
        }
    }
    group_clear(kind);
    /* original: counts by the given player but takes the local player's selection */
    uint32_t leader = 0;
    uint16_t j;
    for (j = 0; (int)j < SELECTED_N(player); j++) {
        uint16_t id = selected(I(uint32_t, ISO_LOCAL_PLAYER))[j];
        uint8_t *u;
        if (!id || !(u = iso_unit(id)))
            continue;
        if (P(int32_t, u, U_CLASS) == 0xc) {
            leader = id;
            if (P(uint8_t, u, U_GROUP))
                group_clear(P(uint8_t, u, U_GROUP));
        }
        P(uint8_t, u, U_GROUP) = (uint8_t)kind;
    }
    group_made(kind, leader);
    DebugStr((const char *)ORIG_PTR(0x10118ac0), kind, (unsigned)j, leader);
    return SELECTED_N(player);
}
