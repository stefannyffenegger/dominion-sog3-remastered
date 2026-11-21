/* The game logic, 0x100cf5a1-0x100cfb0d: a patrolling vehicle's held cell against enemy
 * wall posts, and a vehicle stopping its patrol. */
#include "iso_core.h"
#include "patrolcell.h"

#define S(u) P(uint16_t, u, 0x1cd)
#define OCC (I(uint32_t *, 0x1934))
#define STEP(d, row, k) (*((const int8_t *)ORIG_PTR(0x1013b468) + (d) * 4 + ((row) & 1) * 2 + (k)))
#define object_at ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t *), 0x1007883c)
#define is_post ORIG(int(__cdecl *)(int16_t), 0x100d26a4)
#define players_allied ORIG(int(__cdecl *)(uint16_t, uint16_t), 0x100d25a3)
#define route_neighbour ORIG(uint32_t(__cdecl *)(int16_t, int16_t), 0x10077b2b)
#define wall_post_ahead ORIG(int(__cdecl *)(uint8_t *, uint8_t *, uint8_t *, int16_t), 0x100cf7cf)
#define unit_take_damage ORIG(void(__cdecl *)(uint8_t *, uint32_t, int), 0x1000d4c2)
#define unit_sound ORIG(int(__cdecl *)(uint8_t *, int, int, int), 0x1001babe)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)

/* 0x100cf5a1: vehicle `u` holds the cell `*p`. On a cell marked bit 14 that holds a wall
 * post of an unallied player, with a live post next to it along the wall (0x10077b2b), and
 * the wall ahead (0x100cf7cf): the vehicle is hit by the wall (0x1000d4c2, 50) and makes a
 * sound, once (+0x1cd bit 10). Off such cells the marks (+0x1cd bits 9, 10) are cleared. */
void __cdecl patrol_cell_taken(uint8_t *u, uint32_t *p)
{
    if (!(*p >> 14 & 1)) {
        if (S(u) >> 10 & 1) {
            S(u) &= 0xfdff;
            S(u) &= 0xfbff;
        }
        return;
    }
    int x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54), a, b, extra;
    IsoVTilePos(g_iso, &x, &y, &a, &b);
    int16_t obj = (int16_t)object_at(x, y, &extra);
    if (obj > 0 && obj < 0x7d0 && is_post(obj)) {
        uint8_t *o = I(uint8_t **, ISO_UNITS)[obj];
        if (o && !players_allied(P(uint8_t, u, 0x14), P(uint8_t, o, 0x14))) {
            int16_t dir = (int16_t)(((int16_t)extra - 4) & 7);
            int16_t n = (int16_t)route_neighbour(obj, dir);
            if (n > 0 && n < 0x7d0) {
                uint8_t *q = I(uint8_t **, ISO_UNITS)[n];
                if (q && P(int16_t, q, 0xd6) > 0 && wall_post_ahead(u, o, q, dir) == 1 && !(S(u) >> 10 & 1)) {
                    S(u) |= 0x400;
                    unit_take_damage(u, ((uint32_t)(uintptr_t)o & 0xffff0000) | P(uint8_t, o, 0x14), 0x32);
                    unit_sound(u, 0x19, 0, 0);
                }
            }
        }
    }
    if (!(S(u) >> 10 & 1)) {
        S(u) &= 0xfdff;
        S(u) &= 0xfbff;
    }
}

/* 0x100cf9df: vehicle `u` stops patrolling: when it is boxed in on a blocked cell (bit 0 on
 * its cell and all 8 neighbours) it is marked (+0x3e bit 23) and gets action 0x14 */
void __cdecl patrol_stop(uint8_t *u)
{
    int x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54), a, b;
    int32_t cell = IsoVTilePos(g_iso, &x, &y, &a, &b);
    if (!(OCC[cell] & 1))
        return;
    int boxed = 1;
    for (int i = 0; i < 8; i++) {
        int nx = STEP(i, y, 0) + x, ny = STEP(i, y, 1) + y;
        if (!(OCC[(ny << I(uint8_t, 0x1aa0)) + nx] & 1)) {
            boxed = 0;
            break;
        }
    }
    if (boxed == 1) {
        P(uint32_t, u, 0x3e) |= 0x800000;
        unit_set_action(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb), 0x14, 1, 0);
    }
}
