/* The game logic, 0x100e663c-0x100e761b: where a unit will be after a number of frames, by
 * playing its walk forward without changing it. */
#include "iso_core.h"
#include "debugmarks.h"
#include "sector.h"
#include "unitpose.h"
#include "unitstepmove.h"
#include "unitstepto.h"
#include "unitwalk.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, NODE(k), 0xe)
#define ANIM(u, a) (P(uint8_t *, u, 0x15f) + (a) * 44)
#define TAB(a, i) G(int8_t, (a) + (i))
#define STEP_X G(int32_t, 0x10118b20)
#define STEP_Y G(int32_t, 0x10118b24)
#define IS_SIDESTEP(a) ((a) == 0xd || (a) == 0xe || (a) == 0xf || (a) == 0x10)

/* The original reads two locals it has not set when the unit has no path: the direction it
 * hands back and the node of the last test. They are whatever the stack held there. */
uint32_t walk_stale[2];
__asm__(".text\n"
        ".globl _unit_walk\n"
        "_unit_walk:\n"
        "\tmovl -0x84(%esp), %eax\n"
        "\tmovl %eax, _walk_stale\n"
        "\tmovl -0x68(%esp), %eax\n"
        "\tmovl %eax, _walk_stale+4\n"
        "\tjmp _unit_walk_c\n");

static int32_t iabs(int32_t v)
{
    return v < 0 ? -v : v;
}

/* v / n rounded away from zero */
static int32_t away(int32_t v, int32_t n)
{
    return v / n + (v % n != 0);
}

/* v / n, one less when not exact */
static int32_t less(int32_t v, int32_t n)
{
    return v / n - (v % n != 0);
}

/* the last step ran past the frames asked for: the position snaps to the tile grid along
 * the unit's direction */
static void snap(int16_t *pos, uint8_t dir, int16_t node, int32_t bx, int32_t by)
{
    int16_t ox = (int16_t)(pos[0] - bx), oy = (int16_t)(pos[1] - by);
    int far_x = iabs(P(int16_t, NODE(node), 4) - bx) > iabs(ox);
    int far_y = iabs(P(int16_t, NODE(node), 6) - by) > iabs(oy);
    int16_t kx, ky;
    switch (dir) {
    case 0:
        if (far_x) {
            kx = (int16_t)away(ox, 64);
            pos[0] = (int16_t)(kx * 64 + bx);
        }
        break;
    case 1:
    case 3:
    case 5:
    case 7:
        if (far_x && far_y) {
            kx = (int16_t)(dir == 1 || dir == 7 ? away(ox, 32) : less(ox, 32));
            ky = (int16_t)(dir == 1 || dir == 3 ? away(oy, 16) : less(oy, 16));
            pos[0] = (int16_t)(kx * 32 + bx);
            pos[1] = (int16_t)(ky * 16 + by);
        }
        break;
    case 2:
        if (far_y) {
            ky = (int16_t)away(oy, 32);
            pos[1] = (int16_t)(ky * 32 + by);
        }
        break;
    case 4:
        if (far_x) {
            kx = (int16_t)less(ox, 64);
            pos[0] = (int16_t)(kx * 64 + bx);
        }
        break;
    case 6:
        if (far_y) {
            ky = (int16_t)less(oy, 32);
            pos[1] = (int16_t)(ky * 32 + by);
        }
        break;
    }
}

/* 0x100e663c: plays unit `id`'s walk forward for at least `frames` frames on copies of its
 * state. pos gets where it will be, *dirp its direction, *nodes the path nodes passed,
 * *still 1 when it has not moved from its node, *cell the cell of its last node. Returns the
 * frames played. Each step is marked for the debug view. */
int32_t __cdecl unit_walk_c(uint32_t id, int16_t *pos, uint8_t *dirp, int32_t frames, int16_t *nodes, int32_t *still,
                            int32_t *cell)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    uint8_t dir = (uint8_t)walk_stale[0];
    int16_t node = (int16_t)walk_stale[1];
    marks_clear();
    *nodes = 0;
    int32_t played = 0;
    pos[0] = P(int16_t, u, 0x52);
    pos[1] = P(int16_t, u, 0x54);
    int32_t bx = P(int16_t, u, 0x68), by = P(int16_t, u, 0x6a);
    int starting = P(uint8_t, u, 0x7d) == 8;
    if (P(int16_t, u, 0x113) > 0) {
        uint8_t act = P(uint8_t, u, 0x147), next = P(uint8_t, u, 0x7a), face = P(uint8_t, u, 0x7b);
        uint8_t odd = P(uint8_t, u, 0x15a), phase = 2;
        dir = P(uint8_t, u, 0x79);
        node = P(int16_t, u, 0x113);
        int32_t cycle = P(uint8_t, u, 0x158);
        const uint8_t *rec = P(const uint8_t *, u, 0x163);
        int16_t steps = P(int16_t, u, 0x123);
        int16_t rem[2], step[2], period[2], count[2];
        memcpy(rem, u + 0x127, 4);
        memcpy(step, u + 0x12b, 4);
        memcpy(period, u + 0x12f, 4);
        memcpy(count, u + 0x133, 4);
        int32_t turning = P(uint32_t, u, 0x10e) >> 30 & 1, sidestep = 0;
        if (starting && next != dir)
            turning = 1;
        if (turning && IS_SIDESTEP(act))
            sidestep = 1;
        int32_t settle = P(uint32_t, u, 0x10e) >> 24 & 1;
        while (played < frames) {
            uint8_t frame = 0;
            if (turning && sidestep) {
                int8_t side = TAB(0x1015b5a0, dir + next * 8);
                phase = side < 0 ? 0 : side > 0 ? 1 : 2;
            } else {
                phase = 2;
            }
            for (; cycle; cycle--, played++, frame++) {
                if (P(int32_t, u, 0xc) >= 0x28 && P(int32_t, u, 0xc) < 0x40 && phase < 2) {
                    int32_t k = phase * 12 + dir * 24 + frame * 2;
                    STEP_X = TAB(0x101589d8, k);
                    STEP_Y = TAB(0x101589d9, k);
                    if (cycle == 1)
                        phase = 2;
                } else if (!odd) {
                    STEP_X = (int8_t)rec[TAB(0x1015b580, dir) + 4] * TAB(0x1015b568, dir);
                    STEP_Y = (int8_t)rec[TAB(0x1015b588, dir) + 4] * TAB(0x1015b570, dir);
                } else {
                    STEP_X = (int8_t)rec[TAB(0x1015b590, dir) + 4] * TAB(0x1015b568, dir);
                    STEP_Y = (int8_t)rec[TAB(0x1015b598, dir) + 4] * TAB(0x1015b570, dir);
                }
                pos[0] = (int16_t)(pos[0] + STEP_X);
                pos[1] = (int16_t)(pos[1] + STEP_Y);
                if (STEP_X || STEP_Y)
                    path_move(rem, count, step, period, pos, pos + 1, &STEP_X, &STEP_Y);
                debug_mark(pos[0], pos[1], 0);
                odd ^= 1;
            }
            if (turning) {
                dir = face;
                if (next == dir) {
                    turning = 0;
                    settle = 1;
                }
            }
            if (played >= frames)
                break;
            if (!turning && settle) {
                uint8_t keep = act;
                int32_t want;
                act = (uint8_t)unit_pose(id, act, (uint16_t)node, dir, next, &face, 0, &phase, &turning);
                if (turning && IS_SIDESTEP(act))
                    sidestep = 1;
                rec = ANIM(u, act);
                cycle = (int8_t)rec[0];
                steps = (int16_t)path_steps_to(id, (uint16_t)pos[0], (uint16_t)pos[1], dir, next, act, (uint16_t)node,
                                               odd, cycle, rec, steps, rem, &want);
                path_step(id, want, &steps, rem, count, step, period);
                act = keep;
                settle = 0;
            }
            if (node <= 0)
                break;
            if (steps > 0) {
                if (action_counts(act))
                    steps--;
                act = (uint8_t)unit_pose(id, act, (uint16_t)node, dir, next, &face, 0, &phase, &turning);
                if (turning && IS_SIDESTEP(act))
                    sidestep = 1;
            } else {
                if (next == dir && !starting) {
                    bx = P(int16_t, NODE(node), 4);
                    by = P(int16_t, NODE(node), 6);
                    node = NEXT(node);
                    if (node > 0) {
                        (*nodes)++;
                        next = P(uint8_t, NODE(node), 9);
                        if (next == dir)
                            settle = 1;
                    }
                }
                starting = 0;
                act = (uint8_t)unit_pose(id, act, (uint16_t)node, dir, next, &face, 1, &phase, &turning);
                if (!turning)
                    dir = next;
                else if (IS_SIDESTEP(act))
                    sidestep = 1;
            }
            rec = ANIM(u, act);
            cycle = (int8_t)rec[0];
        }
    }
    if (P(int16_t, u, 0x52) == pos[0] && pos[1] == P(int16_t, u, 0x54)) {
        *still = !*nodes;
        *cell = P(int32_t, u, 0x56);
    } else {
        *still = 0;
        int32_t cx = bx, cy = by, a, b;
        *cell = IsoVTilePos(g_iso, &cx, &cy, &a, &b);
        if (frames <= played && node > 0)
            snap(pos, dir, node, bx, by);
    }
    *dirp = dir;
    return played;
}
