/* The game logic, 0x10090223-0x10091400: a unit's sight. `iso_reveal` stamps a view shape
 * (a radius, optionally facing one of eight directions) onto the map around (x, y): every
 * cell in it is marked seen by the player (bits 24-31 of the map cell; also by the local
 * player when it shares its allies' sight, 0xd9d2), and for the local player (or allies with
 * shared sight, or when forced) the fog tiles (0x1960 +0x10, a byte per cell) are cleared:
 * cells inside the shape lose their fog, cells on its edge get the fog tile that fits their
 * eight neighbours. Then the screen area and the radar's fog are redrawn. */
#include "iso_core.h"
#include "radarmap.h"
#include "vision.h"

#define debug_log ORIG(void(__cdecl *)(int, const char *), 0x10095f64)
#define SHAPES ((uint8_t **)ORIG_PTR(0x10149634))     /* by radius: the round view shapes */
#define SHAPES_DIR ((uint8_t **)ORIG_PTR(0x101494d8)) /* by radius and direction: the cones */
#define STEP ((const int8_t *)ORIG_PTR(0x1013b468))   /* per direction: dx, dy for even and odd rows */
#define NEIGHBOUR(bit, parity) G(int32_t, 0x1017c538 + (bit) * 8 + (parity) * 4)
#define FOG_TILE ((const uint8_t *)ORIG_PTR(0x1014e9f0)) /* by the eight neighbours' fog */
#define MAX_EDGE 0x400 /* the original kept 128 edge cells on its stack, unchecked */

static void mark_seen(uint32_t cell, uint32_t player, int shared)
{
    uint32_t *m = &I(uint32_t *, ISO_MAP)[cell];
    *m = (*m >> 24 | 1u << (player & 0x1f)) << 24 | (*m & 0xffffff);
    if (shared)
        *m = (*m >> 24 | 1u << (I(uint8_t, ISO_LOCAL_PLAYER) & 0x1f)) << 24 | (*m & 0xffffff);
}

void __cdecl iso_reveal(int32_t x, uint32_t y, uint32_t player, int16_t radius, uint32_t dir, uint16_t unit,
                        int32_t force)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return;
    uint8_t **units = I(uint8_t **, ISO_UNITS);
    if (unit && P(int32_t, units[unit], 0xc) >= 0x30 && P(int32_t, units[unit], 0xc) <= 0x37)
        return; /* turrets see through their vehicle */
    static uint8_t *edge_cell[MAX_EDGE];
    static uint8_t edge_parity[MAX_EDGE], edge_mask[MAX_EDGE];
    uint32_t edges = 0, counted = 0, prev_value = 0;
    int32_t local = I(int32_t, ISO_LOCAL_PLAYER);
    uint8_t allies = I(uint8_t, local * ISO_PLAYER_SIZE + 0x1ba0);
    int shared = (uint32_t)local != (player & 0xffff) && I(int32_t, 0xd9d2) && (1 << (player & 0x1f) & allies);
    int clears = (uint32_t)local == (player & 0xffff) ||
                 (I(int32_t, 0xd9d2) && (1 << (player & 0x1f) & allies)) || force;
    uint8_t mask = (uint8_t)(1 << (player & 0x1f));
    if (shared)
        mask |= (uint8_t)(1 << (local & 0x1f));
    uint8_t *fog = P(uint8_t *, I(uint8_t *, 0x1960), 0x10);
    int tx = x, ty = (int)y, cx, cy;
    IsoVTilePos(g_iso, &tx, &ty, &cx, &cy);
    if (unit && units[unit]) {
        int32_t sight = P(int32_t, P(uint8_t *, I(uint8_t *, 0x197c), 0), P(int32_t, units[unit], 0xc) *
                                                                              P(int32_t, I(uint8_t *, 0x197c), 0x2c) * 4 + 8);
        radius = (int16_t)(sight / 64 + 2);
        if (radius < 1)
            radius = 2;
        else if (radius > 0xb)
            radius = 0xb;
    }
    const uint8_t *shape = dir == 0xffffffff ? SHAPES[radius] : SHAPES_DIR[radius * 8 - 8 + (int32_t)dir];
    int16_t size = (int16_t)(radius * 2);
    /* from the centre to the shape's first corner, then row by row */
    int32_t rx = tx;
    uint32_t ry = (uint32_t)ty;
    for (int32_t k = 0; k < radius; k++) {
        rx += STEP[0x10 + (ry & 1) * 2];
        ry += STEP[0x11 + (ry & 1) * 2];
    }
    for (int16_t row = 0; row <= size; row++) {
        int32_t px = rx;
        uint32_t py = ry;
        rx += STEP[4 + (ry & 1) * 2];
        ry += STEP[5 + (ry & 1) * 2];
        for (int16_t col = 0; col <= size; col++) {
            uint8_t s = *shape++;
            if (s && px >= 0 && px < I(int32_t, ISO_MAP_W) && (int32_t)py >= 0 && (int32_t)py < I(int32_t, ISO_MAP_H)) {
                uint32_t cell = (py << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)px;
                uint8_t *f = fog + cell;
                if (*f == 0 || *f > 0x36) {
                    mark_seen(cell, player, shared);
                } else {
                    if (s == 1) {
                        if (clears)
                            *f = 0; /* inside the shape: no fog */
                    } else if (clears && edges < MAX_EDGE) {
                        /* on the shape's edge: which of its neighbours exist on the map */
                        uint8_t m = 0xff;
                        edge_cell[edges] = f;
                        edge_parity[edges] = (uint8_t)(py & 1);
                        if (!edge_parity[edges]) {
                            if (px == 0)
                                m &= 0xc7;
                            if (py == 0)
                                m &= 0x1f;
                            if (I(int32_t, ISO_MAP_W) - 1 == px)
                                m &= 0xfe;
                            if ((uint32_t)I(int32_t, ISO_MAP_H) - 2 == py)
                                m &= 0xfb;
                        } else {
                            if (px == 0)
                                m &= 0xef;
                            if (py == 1)
                                m &= 0xbf;
                            if (I(int32_t, ISO_MAP_W) - 1 == px)
                                m &= 0x7c;
                            if ((uint32_t)I(int32_t, ISO_MAP_H) - 1 == py)
                                m &= 0xf1;
                        }
                        edge_mask[edges] = m;
                        edges++;
                    }
                    mark_seen(cell, player, shared);
                    counted++;
                }
            }
            px += STEP[0x1c + (py & 1) * 2];
            py += STEP[0x1d + (py & 1) * 2];
        }
    }
    if (!clears || !counted)
        return;
    /* the edge cells' fog tiles: a bit per neighbour that is clear (0) or has fog below 0x37.
     * As in the original, a neighbour that is off the map counts by the last value read. The
     * counter was a byte in the original (more than 255 edge cells would never end; the shapes
     * have far fewer) */
    for (uint32_t e = 0; e < edges; e++) {
        uint8_t *f = edge_cell[e];
        uint32_t par = edge_parity[e];
        uint8_t m = edge_mask[e], bits = 0;
        for (int b = 0; b < 8; b++) {
            if (m & 1 << b) {
                prev_value = f[NEIGHBOUR(b, par)];
                if (!prev_value)
                    continue;
            }
            if (prev_value < 0x37)
                bits |= (uint8_t)(1 << b);
        }
        if (FOG_TILE[bits] == 0x1e) {
            wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x1014eaf0), (uint32_t)(uint8_t)e, (uint32_t)FOG_TILE[bits],
                      bits >> 7, (bits & 0x40) >> 6, (bits & 0x20) >> 5, (bits & 0x10) >> 4, (bits & 8) >> 3,
                      (bits & 4) >> 2, (bits & 2) >> 1, bits & 1, (uint32_t)bits);
            debug_log(6, (const char *)(g_iso + 0xdea));
            wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x1014eb28), (uint32_t)m, m >> 7, (m & 0x40) >> 6,
                      (m & 0x20) >> 5, (m & 0x10) >> 4, (m & 8) >> 3, (m & 4) >> 2, (m & 2) >> 1, m & 1);
            debug_log(6, (const char *)(g_iso + 0xdea));
        }
        *f = FOG_TILE[bits];
    }
    /* the screen area to redraw: the shape's box, by the direction it faces */
    RECT r;
    r.left = r.right = x - I(int32_t, 0xcd82);
    r.top = r.bottom = (int32_t)y - I(int32_t, 0xcd86);
    int32_t w = ((int32_t)radius / 2 << (I(uint8_t, 0xc32b) & 0x1f)) + I(int32_t, 0xcd7e) * 2 + 0x40;
    int32_t h = ((int32_t)radius << (I(uint8_t, 0xc337) & 0x1f)) + I(int32_t, 0xc34e) * 2 + 0x20;
    switch (dir) {
    case 0: r.right += w; r.top -= h; r.bottom += h; break;
    case 1: r.top -= (h >> 1) + 0x20; r.left -= (w >> 1) + 0x40; r.right += w; r.bottom += h; break;
    case 2: r.left -= w; r.right += w; r.bottom += h; break;
    case 3: r.top -= (h >> 1) + 0x20; r.left -= w; r.right += (w >> 1) + 0x40; r.bottom += h; break;
    case 4: r.left -= w; r.top -= h; r.bottom += h; break;
    case 5: r.top -= h; r.left -= w; r.right += (w >> 1) + 0x40; r.bottom += (h >> 1) + 0x20; break;
    case 6: r.left -= w; r.right += w; r.top -= h; break;
    case 7: r.top -= h; r.left -= (w >> 1) + 0x40; r.right += w; r.bottom += (h >> 1) + 0x20; break;
    default: r.left -= w; r.right += w; r.top -= h; r.bottom += h; break;
    }
    SpriteRefreshRect(I(void *, ISO_HSL), &r);
    r.left += I(int32_t, 0xcd82);
    r.top += I(int32_t, 0xcd86);
    r.right += I(int32_t, 0xcd82);
    r.bottom += I(int32_t, 0xcd86);
    IsoUpdateRadarFogBitmap(g_iso, (uint32_t)r.left, (uint32_t)r.top, r.right, r.bottom, mask);
}
