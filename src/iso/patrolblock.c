/* The game logic, 0x100ce277-0x100ce796: whether an enemy wall post stands on a unit's
 * patrol way, and two small helpers. Floats go through memory as floats between the steps,
 * as in the original. */
#include "iso_core.h"
#include "patrolblock.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define OCC (I(uint32_t *, 0x1934))
#define ignores_marks ORIG(int(__cdecl *)(uint8_t *), 0x100d26fb)
#define players_allied ORIG(int(__cdecl *)(uint16_t, uint16_t), 0x100d25a3)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define cells_between ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x100ce796)
#define on_map_px ORIG(int(__cdecl *)(int, int), 0x100d2a70)
#define object_at ORIG(int32_t(__cdecl *)(int32_t, int32_t, int32_t *), 0x1007883c)
#define is_post ORIG(int(__cdecl *)(int16_t), 0x100d26a4)
#define patrol_meets_post ORIG(void(__cdecl *)(uint8_t *, uint32_t, uint32_t, int32_t), 0x100ce61d)
#define crt_sqrt ORIG(long double(__cdecl *)(double), 0x100f4dd0)

/* two shorts packed into a dword, as the original passes a point */
static uint32_t pack(int16_t x, int16_t y)
{
    return (uint16_t)x | (uint32_t)(uint16_t)y << 16;
}

/* 0x100ce277: walks unit `u`'s patrol (+0x113) as far as 3 times its range plus 80, cell by
 * cell (0x100ce796, steps from table 0x101590c8), looking for a cell marked bit 14 (not its
 * own) with a wall post (0x100d26a4) of a player it is not allied with; then 0x100ce61d and
 * 1. Not for units marked +0x1cd bit 5, ignoring marks (0x100d26fb) or without legs. */
int32_t __cdecl patrol_blocked(uint8_t *u)
{
    if (P(int16_t, u, 0x113) <= 0 || (P(uint16_t, u, 0x1cd) >> 5 & 1) || ignores_marks(u) == 1 ||
        !P(int16_t, u, 0x1e7))
        return 0;
    int32_t found = 0;
    volatile float acc = 0.0f, prev, d, range, rem;
    range = (float)((long double)P(float, u, 0x1d3) * G(float, 0x101132b4) + G(float, 0x10113260));
    int16_t px = P(int16_t, u, 0x52), py = P(int16_t, u, 0x54);
    int16_t k = P(int16_t, u, 0x113);
    while (k > 0 && range > acc && !found) {
        uint8_t rec[0x12];
        memcpy(rec, VEC(k), 0x12);
        int16_t nx = P(int16_t, rec, 4), ny = P(int16_t, rec, 6);
        d = (float)crt_sqrt((double)((py - ny) * (py - ny) + (px - nx) * (px - nx)));
        prev = acc;
        long double sum = (long double)d + acc;
        acc = (float)sum;
        uint8_t dir = P(uint8_t, rec, 9);
        if (sum >= (long double)range) {
            /* the way ends on this leg */
            float v[2];
            rem = (float)((long double)range - prev);
            leg_vector(dir, v);
            nx = (int16_t)((int16_t)(int32_t)((long double)v[0] * rem) + px);
            ny = (int16_t)((int16_t)(int32_t)((long double)v[1] * rem) + py);
        }
        int32_t cnt = cells_between(pack(px, py), pack(nx, ny), dir);
        for (int32_t j = 0; j < cnt; j++) {
            px = (int16_t)(px + G(int16_t, 0x101590c8 + dir * 4));
            py = (int16_t)(py + G(int16_t, 0x101590ca + dir * 4));
            if (on_map_px(px, py) != 1)
                continue;
            int32_t cell = cell_of(px, py);
            if (P(int32_t, u, 0x56) == cell || !(OCC[cell] >> 14 & 1))
                continue;
            int tx = px, ty = py, a, b, extra;
            IsoVTilePos(g_iso, &tx, &ty, &a, &b);
            int16_t obj = (int16_t)object_at(tx, ty, &extra);
            if (obj <= 0 || obj >= 0x7d0 || !is_post(obj))
                continue;
            uint8_t *o = I(uint8_t **, ISO_UNITS)[obj];
            if (!o || players_allied(P(uint8_t, u, 0x14), P(uint8_t, o, 0x14)))
                continue;
            patrol_meets_post(u, pack(px, py), (uint16_t)obj, extra);
            found = 1;
            break;
        }
        px = nx;
        py = ny;
        k = P(int16_t, VEC(k), 0xe);
    }
    return found;
}

/* 0x100ce71f: sets or clears +0x1cd bit 12 */
void __cdecl unit_set_hold(uint8_t *u, uint32_t on)
{
    P(uint16_t, u, 0x1cd) = (uint16_t)(((on & 1) << 12) | (P(uint16_t, u, 0x1cd) & 0xefff));
}

/* 0x100ce756: the map cell at a point */
int32_t __cdecl cell_of(int32_t x, int32_t y)
{
    int tx = x, ty = y, a, b;
    return IsoVTilePos(g_iso, &tx, &ty, &a, &b);
}
