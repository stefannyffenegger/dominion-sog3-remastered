/* The game logic, 0x100a9c54-0x100aa238: small engine exports: the reconnect window, finding
 * a unit under a point, the local player, the debug mode, ambient sounds, and adding a unit. */
#include "iso_core.h"
#include "walls.h"
#include "vecscore.h"
#include "engineexp.h"

#define markers_show ORIG(void(__cdecl *)(uint32_t), 0x100f3b0d)
#define unit_create ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t, uint32_t, uint32_t, int32_t, uint32_t, int, uint32_t), 0x100aa238)

/* IsoInitReconnectAndPauseWindow */
void __stdcall IsoInitReconnectAndPauseWindowC(uint8_t *iso, uint32_t a, uint32_t b, uint32_t c)
{
    g_iso = iso;
    I(uint32_t, 0x1af5) = a;
    I(uint32_t, 0x1af9) = b;
    I(uint32_t, 0x1af1) = 0;
    I(uint32_t, 0x1b0d) = c;
}

/* IsoFindUnit: the first unit on the map whose sprite's bounds hold (x, y), or -1. Each
 * unit's bounds are kept at +0xc6 */
int32_t __stdcall IsoFindUnitC(uint8_t *iso, LONG x, LONG y)
{
    g_iso = iso;
    for (int32_t k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[k];
        if (id <= 0 || !I(uint8_t **, ISO_UNITS)[id])
            continue;
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        SpriteGetBounds(I(void *, 0x181e), P(void *, u, 4), (RECT *)(u + 0xc6));
        POINT pt = {x, y};
        if (PtInRect((RECT *)(I(uint8_t **, ISO_UNITS)[id] + 0xc6), pt))
            return id;
    }
    return -1;
}

/* IsoSetPlayerId: sets the local player; the previous one */
uint32_t __stdcall IsoSetPlayerIdC(uint8_t *iso, uint32_t player)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10157840), player);
    uint32_t was = I(uint32_t, 0x1b65);
    I(uint32_t, 0x1b65) = player;
    return was;
}

/* IsoSetDebugMode: sets the debug mode (iso+0xeea); the previous one. The value 666 instead
 * toggles iso+0x174 bit 5, which shows or hides the map markers of the buildings */
int32_t __stdcall IsoSetDebugModeC(uint8_t *iso, int32_t mode)
{
    g_iso = iso;
    if (mode == 0x29a) {
        I(uint8_t, 0x174) = (uint8_t)(((I(uint8_t, 0x174) >> 5 & 1) ^ 1) << 5 | (I(uint8_t, 0x174) & 0xdf));
        markers_show(I(uint8_t, 0x174) >> 5 & 1);
        return 0x29a;
    }
    int32_t was = I(int32_t, 0xeea);
    I(int32_t, 0xeea) = mode;
    return was;
}

/* IsoEnableAmbients: the ambient sounds on or off (iso+0x11c); the walls are refreshed */
void __stdcall IsoEnableAmbientsC(uint8_t *iso, uint32_t unused, uint32_t on)
{
    (void)unused;
    g_iso = iso;
    I(uint32_t, 0x11c) = on;
    if (I(int32_t, ISO_HAVE_SPRITES))
        wall_pieces_refresh();
}

/* IsoAddUnit: adds a unit with the next id (iso+0x1ad1, 1-2000; else -10 and the flag
 * 0x1017df58 set), counted as built for its player (a transport under construction as kind
 * 0x8c) */
uint32_t __stdcall IsoAddUnitC(uint8_t *iso, uint32_t player, uint32_t kind, uint32_t a4, int32_t a5, uint32_t a6,
                               uint32_t a7, int32_t a8, uint32_t a9, uint32_t flags)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10157854), player, kind);
    uint16_t id = (uint16_t)I(uint32_t, 0x1ad1);
    if (!id || id > 2000) {
        G(uint8_t, 0x1017df58) = 1;
        return 0xfffffff6;
    }
    uint32_t counted = ((int32_t)kind < 0x88 || (int32_t)kind > 0x9f || !(flags & 1)) ? kind : 0x8c;
    score_unit_built(player, counted);
    /* the original passes the id with a stack leftover in its upper half, which the callee
     * masks off; ours passes 0 there */
    return unit_create(id, player, kind, a4, a5, a6, a7, a8, a9, 0, flags);
}
