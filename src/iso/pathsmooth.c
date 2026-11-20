/* The game logic, 0x1008f31e-0x1008fa3f: smoothing a path. A path (the record at `path`:
 * +0 the number of points, +2 + k*2 the sector of point k, +0x64 + k*4 point k as x, y)
 * from (x1, y1) to (x2, y2) is found across sectors; then each corner is moved: where the
 * path bends sharply, onto the sector's edge path (its nearest point); where it is almost
 * straight, to where the line from the previous point to the next crosses the sector's
 * gate (or the gate's nearer end). The float math follows the original's x87 code. */
#include "iso_core.h"
#include "vecscore.h"
#include "aim.h"
#include "pathsmooth.h"

#define sector_at ORIG(uint32_t(__cdecl *)(int32_t, int32_t), 0x100987c8)
#define path_find ORIG(int32_t(__cdecl *)(int16_t, int16_t, int16_t, int16_t, int32_t *), 0x1008deb1)
#define sector_ptr ORIG(uint8_t *(__cdecl *)(int32_t), 0x10098905)
#define edge_find ORIG(int32_t(__cdecl *)(int32_t, uint8_t *), 0x1009a456)
#define edge_nearest ORIG(int32_t(__cdecl *)(int32_t, uint8_t *), 0x1009a4ad)
#define edge_walk ORIG(int32_t(__cdecl *)(int32_t, int32_t, uint8_t *, int, int, int32_t, int16_t *), 0x1008fa3f)

static int16_t ftol16(long double v) { return (int16_t)(int64_t)v; }

/* the map cell under (x, y), or -1 if off the map */
static int32_t cell_at(int32_t x, int32_t y)
{
    int tx, ty;
    int32_t c = IsoVTilePos(g_iso, &x, &y, &tx, &ty);
    if (c < 0 || I(int32_t, ISO_MAP_H) * I(int32_t, ISO_MAP_W) <= c)
        return -1;
    return c;
}

/* walks the sector's edge path from its point `k` to settle `pt`, first forwards, then (if
 * that fails, from `retry`) backwards */
static void edge_settle(int32_t k, uint8_t *sector, int32_t flags, int16_t *pt, const int16_t *retry)
{
    if (!edge_walk(k, 1, sector, 3, 0, flags, pt)) {
        pt[0] = retry[0];
        pt[1] = retry[1];
        edge_walk(k, -1, sector, 3, 1, flags, pt);
    }
}

int32_t __cdecl path_smooth(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int32_t flags, int32_t *path)
{
    if (sector_at(x1, y1) == sector_at(x2, y2)) {
        path[0] = 1;
        P(int16_t, path, 0x68) = x2;
        P(int16_t, path, 0x6a) = y2;
        return 1;
    }
    int32_t n = path_find(x1, y1, x2, y2, path);
    if (n <= 1)
        return n;
    float prev[2] = { (float)x1, (float)y1 };
    float cur[2] = { (float)P(int16_t, path, 0x68), (float)P(int16_t, path, 0x6a) };
    for (int32_t k = 1; k < path[0]; k++) {
        uint8_t *sector = sector_ptr(P(int16_t, path, k * 2 + 2));
        float next[2] = { (float)P(int16_t, path, k * 4 + 0x68), (float)P(int16_t, path, k * 4 + 0x6a) };
        float in[2], out[2];
        vec_direction(prev, cur, in);
        vec_direction(cur, next, out);
        long double turn = vec_turn_angle(in, out);
        int16_t *point = &P(int16_t, path, k * 4 + 0x64);
        if (!(turn > (long double)G(float, 0x101131b0))) {
            /* a sharp bend: onto the sector's edge path */
            int16_t pt[2] = { point[0], point[1] };
            int32_t cell = cell_at(pt[0], pt[1]);
            if (cell >= 0) {
                int32_t e = edge_find(cell, sector);
                if (e < 0 || P(int16_t, sector, 0x6a) <= e) {
                    e = edge_nearest(cell, sector);
                    point[0] = P(int16_t, sector, 0x70 + e * 8);
                    point[1] = P(int16_t, sector, 0x72 + e * 8);
                    pt[0] = point[0];
                    pt[1] = point[1];
                }
                int16_t retry[2] = { point[0], point[1] };
                edge_settle(e, sector, flags, pt, retry);
                point[0] = pt[0];
                point[1] = pt[1];
            }
        } else {
            /* almost straight: where prev-next crosses the sector's gate a-b */
            float a[2] = { (float)P(int16_t, sector, 0xc), (float)P(int16_t, sector, 0xe) };
            float b[2] = { (float)P(int16_t, sector, 0x10), (float)P(int16_t, sector, 0x12) };
            float t1, t2;
            segment_cross(&t1, &t2, prev, next, a, b);
            float lo = G(float, 0x10113188), hi = G(float, 0x10113194);
            if (t1 >= lo && t1 <= hi && t2 >= lo && t2 <= hi) {
                float px = (float)(((long double)b[0] - a[0]) * t2 + a[0]);
                float py = (float)(((long double)b[1] - a[1]) * t2 + a[1]);
                int32_t cell = cell_at(ftol16(px), ftol16(py));
                if (cell >= 0) {
                    int32_t e = edge_find(cell, sector);
                    if (e < 0 || P(int16_t, sector, 0x6a) <= e) {
                        e = edge_nearest(cell, sector);
                        px = (float)P(int16_t, sector, 0x70 + e * 8);
                        py = (float)P(int16_t, sector, 0x72 + e * 8);
                    }
                    int16_t pt[2] = { ftol16(px), ftol16(py) };
                    int16_t retry[2] = { ftol16(px), ftol16(py) };
                    edge_settle(e, sector, flags, pt, retry);
                    point[0] = pt[0];
                    point[1] = pt[1];
                }
            } else {
                /* the crossing misses the gate: from the gate's end */
                int16_t pt[2];
                int32_t e;
                int16_t dir;
                if (!(t2 >= lo) || t2 <= hi) { /* as the x87 tests: a NaN takes this branch */
                    pt[0] = ftol16(a[0]);
                    pt[1] = ftol16(a[1]);
                    e = 0;
                    dir = 1;
                } else {
                    pt[0] = ftol16(b[0]);
                    pt[1] = ftol16(b[1]);
                    e = (int16_t)(P(int16_t, sector, 0x6a) - 1);
                    dir = -1;
                }
                edge_walk(e, dir, sector, 4, 1, flags, pt);
                point[0] = pt[0];
                point[1] = pt[1];
            }
        }
        prev[0] = cur[0];
        prev[1] = cur[1];
        cur[0] = next[0];
        cur[1] = next[1];
    }
    return n;
}

/* ---- 0x1008fc3d-0x10090120 (part 89) ---- */

#define path_cell_alloc ORIG(int32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define path_cell_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)

/* 0x1008ff50: the turn between two directions in degrees (0-180) */
long double __cdecl vec_turn_angle(const float *a, const float *b)
{
    float la = (float)vec_sqrt2((float)((long double)a[1] * a[1] + (long double)a[0] * a[0]));
    float lb = (float)vec_sqrt2((float)((long double)b[1] * b[1] + (long double)b[0] * b[0]));
    long double q = vec_dot(a, b) / ((long double)lb * la);
    float c = (float)q, r;
    if (!(q > -1.0L))
        r = 180.0f;
    else if (!(c < 1.0f))
        r = 0.0f;
    else
        r = (float)(aim_acos2(c) * 180.0f / G(float, 0x101131a0));
    return r;
}

/* 0x10090030: where the lines p3-p4 and p5-p6 cross: `out1` as a fraction along p3-p4,
 * `out2` along p5-p6. 0 when they are (nearly) parallel: |det| < 0.001 (double 0x101131a8) */
int32_t __cdecl segment_cross(float *out1, float *out2, const float *p3, const float *p4, const float *p5,
                              const float *p6)
{
    float d1[2], d2[2], e[2];
    vec_sub(p3, p4, d1);
    vec_sub(p5, p6, d2);
    float n1[2] = { -d1[1], d1[0] };
    float n2[2] = { -d2[1], d2[0] };
    float det = (float)vec_dot(d1, n2);
    if (!(aim_fabs(det) >= (long double)G(double, 0x101131a8)))
        return 0;
    vec_sub(p3, p5, e);
    *out1 = (float)(vec_dot(e, n2) / det);
    *out2 = (float)(vec_dot(e, n1) / det);
    return 1;
}

/* 0x1008fc3d: gives unit `unit` a smoothed path from (x1, y1) to (x2, y2) in its path cells
 * (0x1984, 0x12 bytes each: x, y, a count, flags): the flags take `last` (bit 1, on every
 * cell or, with +0x193 bit 2, only the last), `b6` and `b8`; all but the last are marked
 * continued (bit 0). The cells are linked; the first one, or -1 */
int32_t __cdecl unit_path_build(int16_t unit, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t last,
                                uint16_t b6, uint16_t b8)
{
    int32_t path[0x54];
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit];
    int32_t type = P(int32_t, u, 0xc);
    int32_t flag = !((type >= 0 && type < 8) || (type >= 0xc && type <= 0x17));
    int32_t n = path_smooth(x1, y1, x2, y2, flag, path);
    int32_t first = -1, prev = 0;
    for (int32_t k = 0, count = 0; k < n; k++, count++) {
        uint8_t *owner = I(uint8_t **, ISO_UNITS)[unit];
        uint32_t player = owner ? P(uint8_t, owner, 0x14) : 0;
        int32_t cell = path_cell_alloc(player, P(uint32_t, I(uint8_t **, ISO_UNITS)[unit], 0xc));
        if (cell < 0)
            return -1;
        uint8_t *c = I(uint8_t *, 0x1984) + cell * 0x12;
        P(int16_t, c, 4) = P(int16_t, path, 0x68 + k * 4);
        P(int16_t, c, 6) = P(int16_t, path, 0x6a + k * 4);
        if (!(P(uint16_t, u, 0x193) >> 2 & 1) || n - 1 == k)
            P(uint16_t, c, 10) = (uint16_t)((P(uint16_t, c, 10) & 0xfffd) | (last & 1) << 1);
        P(uint16_t, c, 10) = (uint16_t)((P(uint16_t, c, 10) & 0xffbf) | (b6 & 1) << 6);
        P(uint16_t, c, 10) = (uint16_t)((P(uint16_t, c, 10) & 0xfeff) | (b8 & 1) << 8);
        if (k < n - 1)
            P(uint16_t, c, 10) |= 1;
        P(uint8_t, c, 8)++;
        if (!count)
            first = cell;
        else
            path_cell_link(prev, cell, 1);
        prev = cell;
    }
    return first;
}
