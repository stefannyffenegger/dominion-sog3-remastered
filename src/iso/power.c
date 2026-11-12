/* The game logic, 0x10001000-0x10001f74: power grids. Structures that give power (an
 * energy well, a relay) mark the map cells within their radius with their player's bit
 * (bits 16-23 of a cell); a structure on its player's marked cells is powered. Each
 * player keeps a list of up to 128 grids. Rewritten from the original. */
#include "iso_core.h"
#include "power.h"

/* not rewritten yet */
#define power_slot_free ORIG(void(__cdecl *)(int), 0x10002fd2)
#define power_grid_update ORIG(void(__cdecl *)(uint32_t), 0x1000258e)
#define power_reach ORIG(int(__cdecl *)(int, int, uint32_t, int, int, int), 0x100036d4)
#define relay_add ORIG(void(__cdecl *)(uint32_t), 0x10010ca9)
#define relay_remove ORIG(void(__cdecl *)(uint32_t), 0x10010e0e)
#define unit_distance ORIG(int(__cdecl *)(int, int, int, int), 0x10067620)

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
        power_slot_free(P(int16_t, u, U_POWER_ICON));
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
