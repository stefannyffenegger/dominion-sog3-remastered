/* The game logic, 0x10001000-0x10003fd4: power grids. Structures that give power (an
 * energy well, a relay) mark the map cells within their radius with their player's bit
 * (bits 16-23 of a cell); a structure on its player's marked cells is powered. Each
 * player keeps a list of up to 128 grids. Rewritten from the original. */
#include "iso_core.h"
#include "power.h"

/* not rewritten yet (names ours, provisional) */
#define relay_add ORIG(void(__cdecl *)(uint32_t), 0x10010ca9)
#define relay_remove ORIG(void(__cdecl *)(uint32_t), 0x10010e0e)
#define unit_distance ORIG(int(__cdecl *)(int, int, int, int), 0x10067620)
#define alert_add ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x100e42a6)
#define player_announce ORIG(void(__cdecl *)(uint32_t, int, int), 0x1005810f)
#define power_bar_set ORIG(void(__cdecl *)(int, uint32_t), 0x10004067)
#define unit_power_state ORIG(void(__cdecl *)(uint32_t, int), 0x100ed2ba)
#define hint_show ORIG(void(__cdecl *)(int, int, int, int, int), 0x1001bbfc)
#define unit_anim_playing ORIG(int(__cdecl *)(int16_t, int), 0x100f3951)
#define unit_anim_start ORIG(void(__cdecl *)(uint16_t, uint32_t, int, int), 0x100f365b)
#define unit_after_power ORIG(void(__cdecl *)(uint32_t), 0x100ebb44)
#define iso_message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define IsoPauseGame ORIG(void(__stdcall *)(uint8_t *, int, int), 0x1008dac1)

#define ISO_LOCAL_PLAYER 0x1b65
#define ISO_POWER_HINTS 0x0174 /* byte: hints given (1 low power, 8 no power, 0x10 restored) */
#define ISO_ICONS 0xcf5c       /* 128 power icons of 0xe bytes: sprite, flags (bit 0 used) */

/* the unit type table: `off` of type t */
static int32_t type_value(int32_t type, int off)
{
    int32_t *t = I(int32_t *, 0x18ec);
    return P(int32_t, (uintptr_t)t[0], off + t[0xb] * type * 4);
}
#define TYPE_GIVES 0x20
#define TYPE_USES 0x24

/* the flood fill's stack (the original's, 24,000 entries of 5 bytes: direction, x, y) */
#define FILL_DIR(i) G(int8_t, 0x1015e638 + (i) * 5)
#define FILL_X(i) G(int16_t, 0x1015e639 + (i) * 5)
#define FILL_Y(i) G(int16_t, 0x1015e63b + (i) * 5)
#define FILL_MAX 24000

#define g_cheat_power G(int32_t, 0x10118030)   /* every structure powered */
#define g_show_all_icons G(int32_t, 0x1013d6f0)

static int power_reach(int x, uint32_t y, uint8_t player, int type, int any, int clear);
static void power_grid_update(uint16_t player);
static void power_icon_free(int16_t i);

#define PLAYER_GRID(p, slot) (g_iso + ISO_PLAYERS + (p) * ISO_PLAYER_SIZE + (slot) * 0x14)
/* a grid: +0x42 the giver's class, +0x44 its unit, +0x46, +0x4a power counts, +0x4e an
 * object and +0x52 its sprite (shown when sprites are) */

/* 0x10001000: marks (set) or clears the unit's player bit on the cells within radius */
void __cdecl power_mark(int16_t id, int16_t radius, int set)
{
    uint8_t *u = iso_unit(id);
    int x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), tx, ty;
    IsoVTilePos(g_iso, &x, &y, &tx, &ty);
    int16_t span = (int16_t)(radius * 2);
    int cx = x, cy = y;
    for (int i = 0; i < radius; i++) {
        cx += g_step_n[(cy & 1) * 2];
        cy += g_step_n[(cy & 1) * 2 + 1];
    }
    const char *mask = g_radius_masks[radius];
    uint32_t bit = 1u << (P(uint8_t, u, U_PLAYER) & 0x1f);
    for (int16_t r = 0; r <= span; r++) {
        int rx = cx, ry = cy;
        cx += g_step_ne[(cy & 1) * 2];
        cy += g_step_ne[(cy & 1) * 2 + 1];
        for (int16_t c = 0; c <= span; c++) {
            if (*mask && iso_on_map(rx, ry)) {
                uint32_t *cell = iso_cell(rx, ry);
                uint32_t bits = set ? (*cell >> 16 | bit) : (*cell >> 16 & ~bit);
                *cell = (bits & 0xff) << 16 | (*cell & 0xff00ffff);
            }
            rx += g_step_se[(ry & 1) * 2];
            ry += g_step_se[(ry & 1) * 2 + 1];
            mask++;
        }
    }
    if (I(uint8_t, 0xeeb) & 0x80) {
        IsoResetTileSet(g_iso);
        IsoRefreshScreen(g_iso);
    }
}

/* 0x100012fa: whether a structure of `type` at (*x, *y) would reach a powered cell of
 * the player; *x, *y end at that cell */
int __cdecl power_reachable(int *x, uint32_t *y, int16_t type, uint8_t player)
{
    int16_t radius = g_power_radius[(int16_t)type >> 2];
    if (!radius)
        return 0;
    int16_t cx = (int16_t)*x, cy = (int16_t)*y;
    for (int16_t i = 0; i < radius; i++) {
        cx = (int16_t)(g_step_n[(cy & 1) * 2] + cx);
        cy = (int16_t)(g_step_n[(cy & 1) * 2 + 1] + cy);
    }
    const char *mask = g_radius_masks[radius];
    uint32_t n = (uint32_t)(I(int, ISO_MAP_H) * I(int, ISO_MAP_W));
    for (uint32_t i = 0; i < n; i++)
        I(uint32_t *, ISO_MAP)[i] &= 0xffff7fff;
    for (int16_t r = 0; r <= (int16_t)(radius * 2); r++) {
        *x = cx;
        *y = (uint32_t)(int)cy;
        cx = (int16_t)(g_step_ne[(cy & 1) * 2] + cx);
        cy = (int16_t)(g_step_ne[(cy & 1) * 2 + 1] + cy);
        for (int16_t c = 0; c <= (int16_t)(radius * 2); c++) {
            if (*mask && iso_on_map(*x, (int)*y) &&
                ((uint8_t)(*iso_cell(*x, (int)*y) >> 16) & (uint8_t)(1u << (player & 0x1f))) &&
                power_reach(*x, (int)*y, player, 0, 1, 0))
                return 1;
            *x += g_step_se[(*y & 1) * 2];
            *y += (uint32_t)(int)g_step_se[(*y & 1) * 2 + 1];
            mask++;
        }
    }
    return 0;
}

/* 0x1000158c: a power grid for the unit (an energy well) in its player's list: its
 * slot, or -1 when all 128 are taken */
int16_t __cdecl power_grid_add(uint16_t id)
{
    uint8_t *u = iso_unit(id);
    int player = P(uint8_t, u, U_PLAYER);
    for (int16_t slot = 0; slot < 0x80; slot++) {
        uint8_t *g = PLAYER_GRID(player, slot);
        if (P(int16_t, g, 0x42))
            continue;
        P(int16_t, g, 0x42) = (int16_t)P(uint32_t, u, U_CLASS);
        P(uint16_t, g, 0x44) = id;
        P(uint32_t, g, 0x46) = 0;
        P(uint32_t, g, 0x4a) = 0;
        if (I(int, ISO_HAVE_SPRITES)) {
            void *app = I(void *, ISO_APP);
            P(uint32_t, g, 0x4e) = RtCopy(app, G(uint32_t, 0x1017bafc));
            RtLoad(app, P(uint32_t, g, 0x4e));
            void **h = RtGetResource(app, P(uint32_t, g, 0x4e));
            P(uint32_t, g, 0x52) = P(uint32_t, h, 4);
            I(uint32_t, 0x1858) = P(uint32_t, g, 0x4e);
        }
        P(int16_t, u, U_POWER_ICON) = (int8_t)slot;
        return slot;
    }
    return -1;
}

/* 0x1000172b: takes the unit's grid out of its player's list */
void __cdecl power_grid_remove(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int player = P(uint8_t, u, U_PLAYER);
    int16_t slot = 0;
    for (;; slot++) {
        if (slot > 0x7f)
            return;
        if ((int)P(int16_t, PLAYER_GRID(player, slot), 0x44) == (int)(id & 0xffff))
            break;
    }
    uint8_t *g = PLAYER_GRID(player, slot);
    P(int16_t, g, 0x42) = 0;
    P(uint32_t, g, 0x46) = 0;
    P(uint32_t, g, 0x4a) = 0;
    if (!I(int, ISO_HAVE_SPRITES))
        return;
    void *sprite = (void *)(uintptr_t)P(uint32_t, g, 0x52);
    SpriteEmptyQueue(I(void *, ISO_HSL), sprite);
    SpriteSendMessage(I(void *, ISO_HSL), sprite, 1, g_msg_hide);
    RtDelete(I(void *, ISO_APP), P(uint32_t, g, 0x4e));
}

/* 0x10001857: empties a player's list */
void __cdecl power_grids_clear(uint32_t player)
{
    for (int16_t slot = 0; slot < 0x80; slot++) {
        uint8_t *g = PLAYER_GRID((int)(player & 0xffff), slot);
        P(int16_t, g, 0x42) = 0;
        P(uint32_t, g, 0x46) = 0;
        P(uint32_t, g, 0x4a) = 0;
        if (I(int, ISO_HAVE_SPRITES) && I(int, 0x17f5) == 4) {
            void *sprite = (void *)(uintptr_t)P(uint32_t, g, 0x52);
            SpriteEmptyQueue(I(void *, ISO_HSL), sprite);
            SpriteSendMessage(I(void *, ISO_HSL), sprite, 1, g_msg_hide);
            RtDelete(I(void *, ISO_APP), P(uint32_t, g, 0x4e));
        }
    }
}

/* 0x10001959: a structure is placed */
void __cdecl power_structure_placed(uint16_t id)
{
    uint8_t *u = iso_unit(id);
    P(uint16_t, u, U_POWER) = 0x8001;
    int16_t radius = g_power_radius[(int16_t)(P(int32_t, u, U_ID_TYPE) >> 2)];
    if (radius)
        power_mark((int16_t)id, radius, 1);
    int32_t cls = P(int32_t, iso_unit(id), U_CLASS);
    if (cls == CLASS_ENERGY_WELL)
        power_grid_add(id);
    else if (cls == CLASS_RELAY)
        relay_add(id);
    power_grid_update(P(uint8_t, iso_unit(id), U_PLAYER));
}

/* 0x10001c1d: flags the power givers within 1400 pixels of the unit (U_FLAGS 0x4000) */
void __cdecl power_flag_near(uint32_t id)
{
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i], other = v & 0xffff;
        if (!(int16_t)v || other == (id & 0xffff) || !iso_unit((int)other) ||
            !g_power_radius[P(int32_t, iso_unit((int)other), U_ID_TYPE) >> 2 & 0xffff])
            continue;
        uint8_t *a = iso_unit((int)(id & 0xffff)), *b = iso_unit((int)other);
        if (unit_distance(P(int16_t, a, U_X), P(int16_t, a, U_Y), P(int16_t, b, U_X), P(int16_t, b, U_Y)) < 0x578)
            P(uint32_t, b, U_FLAGS) |= 0x4000;
    }
}

/* 0x10001dac: marks the flagged givers' cells again */
void __cdecl power_remark_flagged(void)
{
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u = iso_unit((int)(v & 0xffff));
        if (!(int16_t)v || !u || !(P(uint32_t, u, U_FLAGS) >> 14 & 1))
            continue;
        int32_t type = P(int32_t, u, U_ID_TYPE);
        P(uint32_t, u, U_FLAGS) &= 0xffffbfff;
        power_mark((int16_t)v, g_power_radius[type >> 2 & 0xffff], 1);
    }
}

/* 0x10001a6b: a structure goes: its cells are cleared and its neighbours' marked again */
void __cdecl power_structure_removed(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int16_t type = (int16_t)(P(int32_t, u, U_ID_TYPE) >> 2);
    if (P(int16_t, u, U_POWER_ICON) >= 0 && P(int32_t, u, U_CLASS) != CLASS_ENERGY_WELL) {
        if (I(int, ISO_HAVE_SPRITES))
            SpriteHide(I(void *, ISO_HSL),
                       (void *)(uintptr_t)I(uint32_t, 0xcf5c + P(int16_t, u, U_POWER_ICON) * 0xe), 1);
        power_icon_free(P(int16_t, u, U_POWER_ICON));
        P(int16_t, u, U_POWER_ICON) = -1;
    }
    if (g_power_radius[type]) {
        uint8_t *w = iso_unit((int)(id & 0xffff));
        int x = P(int16_t, w, U_X), y = P(int16_t, w, U_Y), tx, ty;
        IsoVTilePos(g_iso, &x, &y, &tx, &ty); /* original: the result is not used */
        power_flag_near(id);
        power_mark((int16_t)id, g_power_radius[type], 0);
        power_remark_flagged();
    }
    if (P(int32_t, u, U_CLASS) == CLASS_ENERGY_WELL)
        power_grid_remove(id);
    else if (P(int32_t, u, U_CLASS) == CLASS_RELAY)
        relay_remove((uint16_t)id);
    power_grid_update(P(uint8_t, u, U_PLAYER));
}

/* 0x10001eaa */
void __cdecl IsoResetAllStructurePowerGrids(uint8_t *iso)
{
    g_iso = iso;
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int16_t id = (int16_t)I(uint32_t *, ISO_UNIT_LIST)[i];
        if (!id || !iso_unit(id))
            continue;
        int16_t type = (int16_t)(P(int32_t, iso_unit(id), U_ID_TYPE) >> 2);
        if (g_power_radius[type])
            power_mark(id, g_power_radius[type], 1);
    }
}

/* ---- the "no power" icons over structures ---- */

/* 0x10002f2e */
int16_t __cdecl power_icon_alloc(void)
{
    for (int16_t i = 0; i < 0x80; i++)
        if (!(I(uint8_t, ISO_ICONS + 4 + i * 0xe) & 1)) {
            I(uint32_t, ISO_ICONS + 4 + i * 0xe) |= 1;
            return i;
        }
    return -1;
}

/* 0x10002fd2 */
static void power_icon_free(int16_t i) { I(uint32_t, ISO_ICONS + 4 + i * 0xe) &= 0xfffffffe; }
void __cdecl power_icon_free_x(int16_t i) { power_icon_free(i); }

/* 0x100023e3 */
void __cdecl power_icon_hide(uint16_t i)
{
    if (i >= 0x80)
        return;
    void *sprite = (void *)(uintptr_t)I(uint32_t, ISO_ICONS + i * 0xe);
    if (sprite) {
        SpriteEmptyQueue(I(void *, ISO_HSL), sprite);
        SpriteHide(I(void *, ISO_HSL), sprite, 1);
        SpriteSetProcessMsgs(I(void *, ISO_HSL), sprite, 0);
    }
    power_icon_free((int16_t)i);
}

/* 0x100024cb: how much of its grid's power is used, in percent (100 without a grid) */
int16_t __cdecl power_percent(int16_t id)
{
    uint8_t *u = iso_unit(id);
    int16_t grid = P(int16_t, u, U_POWER);
    if (grid < 0)
        return 100;
    uint8_t *g = PLAYER_GRID(P(uint8_t, u, U_PLAYER), grid);
    if (!P(uint32_t, g, 0x4a))
        return 100;
    return (int16_t)((((uint32_t)P(int32_t, g, 0x46) << 10) / P(uint32_t, g, 0x4a)) * 100 >> 10);
}

/* 0x10001f74: whether the structure has power now, with its icon: 0x8002 when it gets
 * power, 0x8003 when it loses it, else its state (0, 1) */
uint32_t __cdecl power_check(int16_t id)
{
    uint8_t *u = iso_unit(id);
    if (!u)
        return 0xffff;
    int16_t was = P(int16_t, u, 0x42), icon = -1; /* original: not set with the cheat */
    if (!g_cheat_power) {
        if (P(int16_t, u, U_POWER) < 0) {
            P(int16_t, u, 0x42) = 0;
            icon = 1;
        } else {
            uint8_t *g = PLAYER_GRID(P(uint8_t, u, U_PLAYER), P(int16_t, u, U_POWER));
            P(int16_t, u, 0x42) = P(uint32_t, g, 0x4a) < P(uint32_t, g, 0x46) ? 0 : 1;
            int16_t left = (int16_t)(100 - power_percent(id));
            if (left < 0)
                left = 0;
            if (left < 0x32) {
                icon = 0;
                alert_add(P(uint8_t, u, U_PLAYER), 6, id, 0);
                if (!(I(uint8_t, ISO_POWER_HINTS) & 1)) {
                    player_announce(P(uint8_t, u, U_PLAYER), 0x25, 1);
                    I(uint8_t, ISO_POWER_HINTS) |= 1;
                }
            }
        }
    } else {
        P(int16_t, u, 0x42) = 1;
    }
    if (P(int16_t, u, U_POWER_ICON) >= 0 && P(int32_t, u, U_CLASS) != CLASS_ENERGY_WELL) {
        power_icon_hide((uint16_t)P(int16_t, u, U_POWER_ICON));
        P(int16_t, u, U_POWER_ICON) = -1;
    }
    if (icon != -1 && (P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER) || g_show_all_icons)) {
        P(int16_t, u, U_POWER_ICON) = (int8_t)power_icon_alloc();
        if (P(int16_t, u, U_POWER_ICON) >= 0) {
            void *hsl = I(void *, ISO_HSL);
            void *sprite = (void *)(uintptr_t)I(uint32_t, ISO_ICONS + P(int16_t, u, U_POWER_ICON) * 0xe);
            SpriteSetProcessMsgs(hsl, sprite, 1);
            G(int32_t, 0x1015727c) = P(int16_t, u, U_X);
            G(int32_t, 0x10157280) = P(int16_t, u, U_Y) - 0x12;
            SpriteSendMessage(hsl, sprite, 4, ORIG_PTR(0x10157278));
            SpriteSetLevel(hsl, sprite, 23000);
            SpriteSendMessage(hsl, sprite, 1, ORIG_PTR(0x10157334));
            G(int32_t, 0x101572c4) = icon;
            G(int32_t, 0x101572c8) = icon;
            G(int32_t, 0x101572cc) = 0;
            SpritePostMessage(hsl, sprite, 4, ORIG_PTR(0x101572c0));
            G(int32_t, 0x1015734c) = 0xfa;
            SpritePostMessage(hsl, sprite, 2, ORIG_PTR(0x10157348));
            SpritePostMessage(hsl, sprite, 1, g_msg_hide);
            SpritePostMessage(hsl, sprite, 2, ORIG_PTR(0x10157348));
            SpritePostMessage(hsl, sprite, 4, ORIG_PTR(0x10157338));
        }
    }
    if (P(int16_t, u, 0x42) == was)
        return (uint16_t)P(int16_t, u, 0x42);
    return P(int16_t, u, 0x42) == 0 ? 0x8003 : 0x8002;
}

/* ---- grids ---- */

/* 0x100036d4: whether the player's marked cells lead from (x, y) to a structure on a
 * grid: with `any`, to any grid (a structure without one stops the search), else to
 * one with power left for a structure of `type`. clear: start a new search. */
static int power_reach(int x, uint32_t y, uint8_t player, int type, int any, int clear)
{
    if (clear) {
        uint32_t n = (uint32_t)(I(int, ISO_MAP_H) * I(int, ISO_MAP_W));
        for (uint32_t i = 0; i < n; i++)
            I(uint32_t *, ISO_MAP)[i] &= 0xffff7fff;
    }
    int8_t dir = 0;
    int16_t sp = 0;
    for (;;) {
        uint32_t o;
        uint8_t *v;
        if (!(*iso_cell(x, (int)y) >> 15 & 1) &&
            (o = I(uint32_t *, 0x1934)[(y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x], o >> 16 & 0xfff) &&
            iso_unit((int)(o >> 16 & 0xfff)) && P(int32_t, iso_unit((int)(o >> 16 & 0xfff)), U_CLASS) > 0x3f &&
            P(int32_t, iso_unit((int)(o >> 16 & 0xfff)), U_CLASS) < 0xa0 && (v = iso_unit((int)(o >> 16 & 0xfff))) &&
            P(uint8_t, v, U_PLAYER) == player) {
            if (P(int16_t, v, U_POWER) >= 0) {
                if (any)
                    return 1;
                uint8_t *g = PLAYER_GRID(player, P(int16_t, v, U_POWER));
                return (uint32_t)(type_value(type, TYPE_USES) + P(int32_t, g, 0x46)) <= P(uint32_t, g, 0x4a);
            }
            if (any)
                return 0;
        }
        *iso_cell(x, (int)y) |= 0x8000;
        int nx = g_step_dirs[dir * 4 + (y & 1) * 2] + x;
        uint32_t ny = (uint32_t)(int)g_step_dirs[dir * 4 + (y & 1) * 2 + 1] + y;
        if (iso_on_map(nx, (int)ny) && !(*iso_cell(nx, (int)ny) >> 15 & 1) &&
            ((uint8_t)(*iso_cell(nx, (int)ny) >> 16) & (uint8_t)(1u << (player & 0x1f)))) {
            if (sp < FILL_MAX) {
                FILL_X(sp) = (int16_t)x;
                FILL_Y(sp) = (int16_t)y;
                FILL_DIR(sp) = dir;
                sp++;
                dir = 0;
                x = nx;
                y = ny;
                continue;
            }
            dir = 7;
        }
        dir++;
        if (dir == 8) {
            if (!sp)
                return 0;
            sp--;
            x = FILL_X(sp);
            y = (uint32_t)(int)FILL_Y(sp);
            dir = FILL_DIR(sp);
        }
    }
}
int __cdecl power_reach_x(int x, uint32_t y, uint8_t player, int type, int any, int clear)
{
    return power_reach(x, y, player, type, any, clear);
}

/* 0x100027ca: grid `slot`: the player's structures connected to its energy well by
 * marked cells join it; returns how many */
int16_t __cdecl power_grid_fill(uint16_t player, int16_t slot)
{
    uint8_t *g = PLAYER_GRID((int16_t)player, slot);
    uint8_t *w = iso_unit(P(int16_t, g, 0x44));
    int x = P(int16_t, w, U_X), tx, ty;
    int yy = P(int16_t, w, U_Y);
    IsoVTilePos(g_iso, &x, &yy, &tx, &ty);
    uint32_t y = (uint32_t)yy;
    int8_t dir = 0;
    int16_t sp = 0, joined = 0;
    for (;;) {
        uint32_t o;
        uint8_t *v;
        if (!(*iso_cell(x, (int)y) >> 15 & 1) &&
            (o = I(uint32_t *, 0x1934)[(y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x], o >> 16 & 0xfff) &&
            !(o >> 14 & 1) && !(o >> 15 & 1) && (v = iso_unit((int)(o >> 16 & 0xfff))) &&
            P(int32_t, v, U_CLASS) > 0x3f && P(int32_t, v, U_CLASS) < 0xa0 && P(uint8_t, v, U_PLAYER) == player) {
            P(int16_t, v, U_POWER) = slot;
            joined++;
        }
        *iso_cell(x, (int)y) |= 0x8000;
        int nx = g_step_dirs[dir * 4 + (y & 1) * 2] + x;
        uint32_t ny = (uint32_t)(int)g_step_dirs[dir * 4 + (y & 1) * 2 + 1] + y;
        if (iso_on_map(nx, (int)ny) && !(*iso_cell(nx, (int)ny) >> 15 & 1) &&
            ((uint8_t)(*iso_cell(nx, (int)ny) >> 16) & (uint8_t)(1u << (player & 0x1f)))) {
            if (sp < FILL_MAX) {
                FILL_X(sp) = (int16_t)x;
                FILL_Y(sp) = (int16_t)y;
                FILL_DIR(sp) = dir;
                sp++;
                dir = 0;
                x = nx;
                y = ny;
                continue;
            }
            dir = 7;
        }
        dir++;
        if (dir == 8) {
            if (!sp)
                return joined;
            sp--;
            x = FILL_X(sp);
            y = (uint32_t)(int)FILL_Y(sp);
            dir = FILL_DIR(sp);
        }
    }
}

/* 0x10002bc1: adds up what each grid's structures give and use; the local player's
 * totals go to the power bar */
void __cdecl power_grid_sum(uint16_t player)
{
    uint32_t gives_total = 0;
    int32_t uses_total = 0;
    for (int slot = 0; slot < 0x80; slot++) {
        uint8_t *g = PLAYER_GRID((int16_t)player, slot);
        if (P(int16_t, g, 0x42) == CLASS_ENERGY_WELL && P(uint8_t, iso_unit(P(int16_t, g, 0x44)), U_PLAYER) == player) {
            P(uint32_t, g, 0x46) = 0;
            P(uint32_t, g, 0x4a) = 0;
        }
    }
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[i];
        if (!id || !iso_unit(id))
            continue;
        uint8_t *u = iso_unit(id);
        P(uint32_t, u, U_FLAGS) |= 0x20;
        if (P(int16_t, u, U_POWER) < 0 || P(uint8_t, u, U_PLAYER) != player ||
            ((P(uint32_t, u, U_FLAGS) >> 4 & 1) && P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER)))
            continue;
        uint8_t *g = PLAYER_GRID(P(uint8_t, u, U_PLAYER), P(int16_t, u, U_POWER));
        int16_t uses = (int16_t)type_value(P(int32_t, u, U_ID_TYPE), TYPE_USES);
        int16_t gives = (int16_t)type_value(P(int32_t, u, U_ID_TYPE), TYPE_GIVES);
        if (P(int32_t, u, U_CLASS) == 0x80) {
            if (P(uint32_t, u, U_FLAGS) >> 12 & 1)
                gives /= 2;
            if (P(int8_t, u, 0x147) == 2)
                gives /= 2;
            else if (P(int8_t, u, 0x147) == 3)
                gives /= 4;
        }
        if (P(uint8_t, u, 0x30) > 1) {
            uses = (int16_t)(uses / 4 + uses);
            if (P(int32_t, u, U_CLASS) == 0x80)
                gives = (int16_t)(gives * 6);
        }
        P(int32_t, g, 0x46) += uses;
        P(int32_t, g, 0x4a) += gives;
        uses_total += uses;
        gives_total += (uint32_t)(int)gives;
    }
    if (I(int32_t, ISO_LOCAL_PLAYER) == (int16_t)player)
        power_bar_set(uses_total, gives_total);
    I(uint8_t, ISO_POWER_HINTS) &= 0xfe;
    I(uint8_t, ISO_POWER_HINTS) &= 0xf7;
    I(uint8_t, ISO_POWER_HINTS) &= 0xef;
}

/* 0x1000258e: rebuilds the player's grids */
static void power_grid_update(uint16_t player)
{
    if (!I(int, ISO_HAVE_SPRITES))
        return;
    uint32_t n = (uint32_t)(I(int, ISO_MAP_H) * I(int, ISO_MAP_W));
    for (uint32_t i = 0; i < n; i++)
        I(uint32_t *, ISO_MAP)[i] &= 0xffff7fff;
    for (int16_t slot = 0; slot < 0x80; slot++) {
        uint8_t *g = PLAYER_GRID((int16_t)player, slot);
        if (P(int16_t, g, 0x42) == CLASS_ENERGY_WELL) {
            P(uint32_t, g, 0x4a) = 0;
            P(uint32_t, g, 0x46) = P(uint32_t, g, 0x4a);
        }
    }
    for (int16_t i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int16_t id = (int16_t)I(uint32_t *, ISO_UNIT_LIST)[i];
        if (iso_unit(id) && P(uint8_t, iso_unit(id), U_PLAYER) == player)
            P(int16_t, iso_unit(id), U_POWER) = -1;
    }
    for (int16_t slot = 0; slot < 0x80; slot++) {
        uint8_t *g = PLAYER_GRID((int16_t)player, slot);
        if (P(int16_t, g, 0x42) && P(int16_t, iso_unit(P(int16_t, g, 0x44)), U_POWER) == -1)
            power_grid_fill(player, slot);
    }
    power_grid_sum(player);
}
void __cdecl power_grid_update_x(uint16_t player) { power_grid_update(player); }

/* ---- per structure ---- */

/* 0x1000300f: a structure whose power may have changed: its state, messages and hints */
void __cdecl power_unit_update(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!(P(uint32_t, u, U_FLAGS) >> 5 & 1))
        return;
    if (P(uint32_t, u, U_FLAGS) >> 4 & 1) {
        P(uint32_t, u, U_FLAGS) &= 0xffffffdf;
        return;
    }
    int16_t r = (int16_t)power_check((int16_t)id);
    int local = P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER);
    if (r == (int16_t)0x8002) {
        unit_power_state(id, 0);
        if (local) {
            I(uint8_t, ISO_POWER_HINTS) &= 0xfe;
            I(uint8_t, ISO_POWER_HINTS) &= 0xf7;
            if (!(I(uint8_t, ISO_POWER_HINTS) >> 4 & 1) && P(uint32_t, u, 0x46)) {
                player_announce(P(uint8_t, u, U_PLAYER), 0x44, 1);
                I(uint8_t, ISO_POWER_HINTS) |= 0x10;
                hint_show(0xc, 0, 0, 0, 1);
            }
            P(uint32_t, u, 0x46) = 0;
        }
        if ((P(uint8_t, u, U_FLAGS) & 1) &&
            !unit_anim_playing(P(int16_t, u, 0x4e), 4 - !(P(uint32_t, u, U_FLAGS) >> 1 & 1)))
            unit_anim_start((uint16_t)id, P(uint32_t, u, U_FLAGS) >> 1 & 1, 1, 1);
    } else if (r == (int16_t)0x8003) {
        unit_power_state(id, P(int32_t, u, U_CLASS) == 0x5c ? 2 : 1);
        if (local) {
            if (P(int16_t, u, U_POWER) == -1) {
                if (!(I(uint8_t, ISO_POWER_HINTS) >> 3 & 1)) {
                    player_announce(P(uint8_t, u, U_PLAYER), 0x43, 1);
                    I(uint8_t, ISO_POWER_HINTS) |= 8;
                    P(uint32_t, u, 0x46) = 1;
                    hint_show(0xb, 0, 0, 0, 1);
                }
                if ((P(uint8_t, u, U_FLAGS) & 1) &&
                    !unit_anim_playing(P(int16_t, u, 0x4e), 6 - !(P(uint32_t, u, U_FLAGS) >> 1 & 1)))
                    unit_anim_start((uint16_t)id, P(uint32_t, u, U_FLAGS) >> 1 & 1, 1, 0);
            } else if (!(I(uint8_t, ISO_POWER_HINTS) & 1)) {
                player_announce(P(uint8_t, u, U_PLAYER), 0x25, 1);
                I(uint8_t, ISO_POWER_HINTS) |= 1;
            }
        }
    }
    P(uint32_t, u, U_FLAGS) &= 0xffffffdf;
}

/* 0x10003309 */
void __cdecl power_unit_tick(uint32_t id)
{
    power_unit_update(id);
    unit_after_power(id);
}

/* 0x10003331: marks every unit for a power update (U_FLAGS 0x20) or none */
void __cdecl power_mark_all(uint32_t on)
{
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[i];
        if (id && iso_unit(id))
            P(uint32_t, iso_unit(id), U_FLAGS) = (P(uint32_t, iso_unit(id), U_FLAGS) & 0xffffffdf) | (on & 1) << 5;
    }
    I(uint8_t, ISO_POWER_HINTS) &= 0xfe;
    I(uint8_t, ISO_POWER_HINTS) &= 0xf7;
    I(uint8_t, ISO_POWER_HINTS) &= 0xef;
}

/* 0x100033fd: a debug box with the selected structure's power figures */
int __cdecl IsoDisplayPowerInfo(uint8_t *iso)
{
    g_iso = iso;
    int sel = -1;
    for (int i = 0; i < 8; i++)
        if (P(int32_t, iso, 0x1b23 + i * 4) == 1) {
            sel = i;
            break;
        }
    if (sel == -1)
        return 0;
    uint16_t id = *P(uint16_t *, iso, 0x193c + sel * 4);
    if (!id || !P(uint8_t **, iso, ISO_UNITS)[id])
        return 0;
    IsoPauseGame(iso, 1, 1);
    uint8_t *u = iso_unit(id);
    uint32_t gives = 0xffffffff, uses = 0xffffffff;
    if (P(int16_t, u, U_POWER) >= 0) {
        uint8_t *g = PLAYER_GRID(P(uint8_t, u, U_PLAYER), P(int16_t, u, U_POWER));
        gives = P(uint32_t, g, 0x4a);
        uses = P(uint32_t, g, 0x46);
    }
    char title[128], text[2048];
    wsprintfA(title, (const char *)ORIG_PTR(0x10118868), id, P(int32_t, u, U_ID_TYPE), P(uint8_t, u, U_PLAYER),
              P(int16_t, u, U_X), P(int16_t, u, U_Y), P(int16_t, u, 0x66));
    int16_t pct = power_percent((int16_t)id);
    wsprintfA(text, (const char *)ORIG_PTR(0x101188b0), P(int16_t, u, U_POWER), gives, uses,
              type_value(P(int32_t, u, U_ID_TYPE), TYPE_GIVES), type_value(P(int32_t, u, U_ID_TYPE), TYPE_USES),
              P(int16_t, u, 0x42), P(int16_t, u, U_POWER_ICON), 100 - pct, P(uint32_t, u, U_FLAGS) >> 12 & 1,
              P(int16_t, u, 0x4c));
    iso_message_box(0, text, title, 0);
    IsoPauseGame(iso, 0, 1);
    return 1;
}

/* 0x10003b00: whether a structure takes part in the power grids */
int __cdecl power_unit_counts(uint8_t *u)
{
    int32_t cls = P(int32_t, u, U_CLASS);
    if (cls < 0x88 || cls > 0x9f)
        return !(P(uint32_t, u, 0x10e) >> 12 & 1) &&
               (P(int16_t, u, 0x42) || cls == CLASS_ENERGY_WELL || (cls == 0x80 && P(int16_t, u, U_POWER) != -1));
    return P(int16_t, u, U_POWER) != -1;
}

/* 0x10003bb2: the other players (allied by the player's mask at +0x37 of its block)
 * whose grids a structure of `type` at (*x, *y) would reach */
uint32_t __cdecl power_reach_others(int *x, uint32_t *y, int16_t type, uint8_t player)
{
    uint8_t found = 0;
    int16_t radius = g_power_radius[(int16_t)type >> 2];
    if (!radius)
        return 0;
    int16_t span = (int16_t)(radius * 2), cx = (int16_t)*x, cy = (int16_t)*y;
    for (int16_t i = 0; i < radius; i++) {
        cx = (int16_t)(g_step_n[(cy & 1) * 2] + cx);
        cy = (int16_t)(g_step_n[(cy & 1) * 2 + 1] + cy);
    }
    const char *mask = g_radius_masks[radius];
    for (int16_t r = 0; r <= span; r++) {
        *x = cx;
        *y = (uint32_t)(int)cy;
        cx = (int16_t)(g_step_ne[(cy & 1) * 2] + cx);
        cy = (int16_t)(g_step_ne[(cy & 1) * 2 + 1] + cy);
        for (int16_t c = 0; c <= span; c++) {
            if (*mask && iso_on_map(*x, (int)*y)) {
                uint8_t others = (uint8_t)(~(uint8_t)(1u << (player & 0x1f)) & (uint8_t)(*iso_cell(*x, (int)*y) >> 16));
                if (others)
                    for (uint8_t b = 0; b < 8; b++)
                        if (!(found & 1u << b) && (others & 1u << b) &&
                            (1u << b & I(uint8_t, player * ISO_PLAYER_SIZE + 0x1ba0)) &&
                            power_reach(*x, *y, b, 0, 1, 1))
                            found = (uint8_t)(1u << b) | found;
            }
            *x += g_step_se[(*y & 1) * 2];
            *y += (uint32_t)(int)g_step_se[(*y & 1) * 2 + 1];
            mask++;
        }
    }
    return (uint32_t)(uint16_t)span >> 8 << 8 | found; /* original: the upper bits are leftovers */
}

/* 0x10003e95: the other players' grids the unit's player reaches */
uint8_t __cdecl power_player_reach(uint8_t *unit)
{
    uint8_t player = P(uint8_t, unit, U_PLAYER), found = 0;
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u;
        if (!(int16_t)v || !(u = iso_unit((int)(v & 0xffff))) || P(uint8_t, u, U_PLAYER) != player ||
            P(int16_t, u, U_POWER) == -1 || P(int32_t, u, U_CLASS) <= 0x3f || P(int32_t, u, U_CLASS) >= 0xa0)
            continue;
        int x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), tx, ty;
        IsoVTilePos(g_iso, &x, &y, &tx, &ty);
        uint32_t yy = (uint32_t)y;
        found |= (uint8_t)power_reach_others(&x, &yy, (int16_t)P(int32_t, u, U_ID_TYPE), player);
    }
    return found;
}
