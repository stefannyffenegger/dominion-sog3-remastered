/* The game logic, 0x10010ca9-0x100118ad: power relays ("umbilicals"): putting one on the
 * map and taking it off, the ring of eight sprites circling a relay to show its reach,
 * and the nearest relay to a point. */
#include "iso_core.h"
#include "sector.h"
#include "power.h"
#include "relay.h"

/* not rewritten yet (names ours, provisional) */
#define cell_set_unit ORIG(void(__cdecl *)(uint32_t *, int16_t), 0x10070a36)
#define cell_clear_unit ORIG(void(__cdecl *)(uint32_t *), 0x10070a64)
#define relay_after_power ORIG(void(__cdecl *)(uint32_t), 0x100f223a)

#define HSL I(void *, ISO_HSL)
#define g_ring_unit G(int16_t, 0x10118d84) /* the relay whose ring is shown, or 0 */
#define RING_SPRITE(i) I(void *, 0xceec + (i) * 0xe)
/* the relay's slot (16 bytes, at 0x25ab of its player's block + slot * 16) */
#define SLOT(u) (g_iso + P(uint8_t, u, U_PLAYER) * ISO_PLAYER_SIZE + P(uint16_t, u, 0x9e) * 0x10)
#define RING_ANGLE 0x25ad  /* short: tenths of a degree */
#define RING_SPEED 0x25af
#define RING_RADIUS 0x25b1
#define RING_FULL 0x25b3   /* the radius it grows to */
#define RING_CEL 0x25b5
#define RING_UNTIL 0x25b7  /* timeGetTime value; -1 forever */

static uint32_t *unit_cell(uint8_t *u)
{
    int x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), tx, ty;
    IsoVTilePos(g_iso, &x, &y, &tx, &ty);
    return &I(uint32_t *, 0x1934)[((uint32_t)y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x];
}

/* 0x10010ca9: a relay put on the map: a slot of its player's, its cell marked */
int __cdecl relay_add(uint16_t id)
{
    uint8_t *u = iso_unit(id);
    if (!u)
        return 0;
    uint32_t *cell = unit_cell(u);
    int16_t slot = player_slot_alloc(P(uint8_t, u, U_PLAYER));
    if (slot < 0)
        return 0;
    cell_set_unit(cell, (int16_t)id);
    *cell |= 1;
    P(int16_t, u, 0x9e) = slot;
    P(uint8_t, u, 0x196) = 0;
    return 1;
}

/* 0x10010e0e */
void __cdecl relay_remove(uint16_t id)
{
    uint8_t *u = iso_unit(id);
    if (!u)
        return;
    if ((int)g_ring_unit == (int)id)
        ring_hide(g_ring_unit);
    uint32_t *cell = unit_cell(u);
    *cell &= 0xfffffffe;
    cell_clear_unit(cell);
    player_slot_free((int16_t)id);
}

/* 0x100113e3: the eight ring sprites around the relay at `angle`, `radius` pixels out (half
 * as high as wide) */
void __cdecl ring_draw(int16_t id, int16_t angle, int16_t radius)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    uint8_t *slot = SLOT(u);
    for (int16_t i = 0; i < 8; i++) {
        int16_t a = (int16_t)(angle + i * 0x1c2);
        if (a > 0xe0f)
            a = (int16_t)(a - 0xe10);
        G(int32_t, 0x1015727c) = Cosine(a) * radius / 10000 + P(int16_t, I(uint8_t **, ISO_UNITS)[id], U_X);
        G(int32_t, 0x10157280) = Sine(a) * radius / 20000 + P(int16_t, I(uint8_t **, ISO_UNITS)[id], U_Y);
        SpriteSendMessage(HSL, RING_SPRITE(i), 4, ORIG_PTR(0x10157278));
        SpriteSetLevel(HSL, RING_SPRITE(i), 23000);
        SpriteShow(HSL, RING_SPRITE(i), (uint32_t)(int32_t)P(int16_t, slot, RING_CEL), 1);
    }
}

/* 0x10011105 */
void __cdecl ring_hide(int16_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    if (u) {
        P(uint32_t, u, U_FLAGS) &= 0xffdfffff;
        P(uint8_t, u, 0x196) = 0;
    }
    for (int16_t i = 0; i < 8; i++)
        SpriteHide(HSL, RING_SPRITE(i), 1);
    g_ring_unit = 0;
    u = I(uint8_t **, ISO_UNITS)[id];
    if (u && (P(int32_t, u, U_CLASS) < 0x84 || P(int32_t, u, U_CLASS) > 0x87))
        player_slot_free(id);
}

/* 0x10010f3a: shows the ring around a unit until `until` (timeGetTime), growing to `radius`
 * (-1: the relay's reach, in cells of 64 pixels) */
void __cdecl ring_show(int16_t id, uint32_t until, int16_t radius)
{
    static const int16_t reach[11] = {0, 1, 2, 2, 3, 4, 4, 5, 6, 7, 8};
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    if (!u)
        return;
    uint8_t *slot = SLOT(u);
    if (g_ring_unit) {
        if (id == g_ring_unit)
            return;
        ring_hide(g_ring_unit);
    }
    P(int16_t, slot, RING_CEL) = 0;
    P(int16_t, slot, RING_ANGLE) = 0;
    P(int16_t, slot, RING_RADIUS) = 4;
    if (radius == -1) {
        int16_t r = g_power_radius[CLASS_RELAY >> 2]; /* original: indexes 11 entries unchecked */
        P(int16_t, slot, RING_FULL) = (int16_t)((r >= 0 && r < 11 ? reach[r] : 0) << 6);
    } else {
        P(int16_t, slot, RING_FULL) = radius;
    }
    P(int16_t, slot, RING_SPEED) = 10;
    P(uint32_t, slot, RING_UNTIL) = until;
    P(uint32_t, I(uint8_t **, ISO_UNITS)[id], U_FLAGS) |= 0x200000;
    if (radius == -1)
        P(uint8_t, I(uint8_t **, ISO_UNITS)[id], 0x196) = 1;
    g_ring_unit = id;
    ring_draw(id, P(int16_t, slot, RING_ANGLE), P(int16_t, slot, RING_RADIUS));
}

/* 0x10011222: every tick for a unit with a ring: it turns and grows (then stays five
 * seconds unless it is for ever); relays also update their power */
void __cdecl ring_tick(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return;
    if (P(uint32_t, u, U_FLAGS) >> 0x15 & 1) {
        uint8_t *slot = SLOT(u);
        if (timeGetTime() < P(uint32_t, slot, RING_UNTIL)) {
            ring_draw((int16_t)id, P(int16_t, slot, RING_ANGLE), P(int16_t, slot, RING_RADIUS));
            P(int16_t, slot, RING_ANGLE) = (int16_t)(P(int16_t, slot, RING_ANGLE) + P(int16_t, slot, RING_SPEED));
            if (P(int16_t, slot, RING_ANGLE) > 0xe0f)
                P(int16_t, slot, RING_ANGLE) = (int16_t)(P(int16_t, slot, RING_ANGLE) - 0xe10);
            P(int16_t, slot, RING_CEL)++;
            if (P(int16_t, slot, RING_CEL) > 0x13)
                P(int16_t, slot, RING_CEL) = 0;
            if (P(int16_t, slot, RING_RADIUS) < P(int16_t, slot, RING_FULL)) {
                P(int16_t, slot, RING_RADIUS) = (int16_t)(P(int16_t, slot, RING_RADIUS) + 0x1e);
                if (P(int16_t, slot, RING_FULL) <= P(int16_t, slot, RING_RADIUS)) {
                    P(int16_t, slot, RING_RADIUS) = P(int16_t, slot, RING_FULL);
                    if (P(int32_t, slot, RING_UNTIL) != -1)
                        P(uint32_t, slot, RING_UNTIL) = timeGetTime() + 5000;
                }
            }
        } else {
            ring_hide((int16_t)id);
        }
    }
    if (P(int32_t, u, U_CLASS) > 0x83 && P(int32_t, u, U_CLASS) < 0x88) {
        power_unit_update(id);
        relay_after_power(id);
    }
}

/* 0x100115af */
int __cdecl relay_selected(int16_t id)
{
    ring_show(id, 0xfffffffe, -1);
    return 1;
}

/* 0x100115d4: the player's relay nearest to (x, y), looking through the 11x11 sectors
 * around; 0 if none */
uint16_t __cdecl relay_nearest(int16_t x, int16_t y, uint16_t player)
{
    uint32_t best = 0xffffffff;
    uint16_t found = 0;
    for (int16_t j = 0; j < 0xb; j++)
        for (int16_t i = 0; i < 0xb; i++) {
            int16_t sx = (int16_t)(i + (int16_t)((uint32_t)((I(int32_t, 0xd6f1) >> 1) + x) >> 8) - 5);
            int16_t sy = (int16_t)(j + (int16_t)((uint32_t)((I(int32_t, 0xd6f5) >> 1) + y) >> 8) - 5);
            if (sx < 0 || sx >= (int)I(uint16_t, 0xc2fd) || sy < 0 || sy >= (int)I(uint16_t, 0xc2ef))
                continue;
            int16_t s = (int16_t)((int16_t)((int)sy << (I(uint8_t, 0xc2f5) & 0x1f)) + sx);
            for (uint16_t v = P(uint16_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 2); v; v = P(uint16_t, iso_unit(v), 0x393)) {
                uint8_t *w = iso_unit(v);
                if (P(uint8_t, w, U_PLAYER) != player || P(int32_t, w, U_CLASS) != CLASS_RELAY)
                    continue;
                int dx = x - P(int16_t, w, U_X), dy = y - P(int16_t, w, U_Y);
                uint32_t d = (uint32_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
                if (d < best) {
                    best = d;
                    found = v;
                }
            }
        }
    return found;
}

/* 0x1001180d: the ring on the player's relay nearest to (x, y), for ever; none: hidden */
void __stdcall IsoShowClosestUmbilical(uint8_t *iso, int16_t x, int16_t y, uint16_t player)
{
    g_iso = iso;
    uint16_t id = relay_nearest(x, y, player);
    if (id)
        ring_show((int16_t)id, 0xffffffff, -1);
    else if (g_ring_unit)
        ring_hide(g_ring_unit);
}

/* 0x10011890 */
void __cdecl unit_set_16b(uint8_t *p) { P(uint32_t, p, 0x16b) = 0xa0; }
