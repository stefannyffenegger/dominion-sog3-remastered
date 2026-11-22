/* The game logic, 0x100e5c00-0x100e6328: how many animation steps take a unit to its next
 * path node, and what is left over. */
#include "iso_core.h"
#include "pathoffset.h"
#include "unitdelta.h"
#include "unitpose.h"
#include "unitstepto.h"

#define NODE(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define NEXT(k) P(int16_t, NODE(k), 0xe)
#define ANIM(u, a) (P(uint8_t *, u, 0x15f) + (a) * 44)
#define TURN_NEEDED(d, n) G(int8_t, 0x1015b620 + (d) + (n) * 8)
#define counts_as_step ORIG(int32_t(__cdecl *)(uint32_t), 0x10010ab0)

static int32_t iabs(int32_t v)
{
    return v < 0 ? -v : v;
}

/* 0x100e5c00: unit `id` at (x, y) facing `dir`, doing `action`, heads for path node `node`.
 * Works out where its next turn must start, walks whole animation cycles up to there, and
 * gives what remains in out[0], out[1], the frames in *frames. Returns the number of cycles
 * that count as steps, 0 when it is there already, -1 without a node.
 * original: the rollback of an overshooting last cycle subtracts a variable that is never
 * set, so *frames keeps that cycle's frames. */
int32_t __cdecl path_steps_to(uint32_t id, uint32_t x_, uint32_t y_, uint32_t dir_, uint32_t next_, uint32_t action_,
                              uint32_t node_, uint32_t odd_, int32_t cycle_, const uint8_t *data_, int32_t unused,
                              int16_t *out, int32_t *frames)
{
    (void)unused;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    int16_t x = (int16_t)x_, y = (int16_t)y_, k = (int16_t)node_;
    uint8_t dir = (uint8_t)dir_, action = (uint8_t)action_;
    *frames = 0;
    if (k <= 0)
        return -1;
    if (P(int16_t, NODE(k), 4) == x && P(int16_t, NODE(k), 6) == y) {
        out[1] = 0;
        out[0] = 0;
        *frames = 0;
        return 0;
    }
    uint8_t next = NEXT(k) > 0 ? P(uint8_t, NODE(NEXT(k)), 9) : (uint8_t)next_;
    uint8_t odd = 0, phase = 2, facing;
    int32_t moving, dx = 0, dy = 0, count = 0, too_short = 0;
    uint8_t act = (uint8_t)unit_pose(id, action, (uint16_t)NEXT(k), dir, next, &facing, next != dir, &phase, &moving);
    if (act == 0x4c) {
        out[0] = 0;
        out[1] = 0;
        *frames = 0;
        return 0;
    }
    int along_x = dir != 2 && dir != 6, along_y = dir != 0 && dir != 4;
    int16_t tx, ty;
    if ((NEXT(k) > 0 && P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 1) || (next == dir && NEXT(k) > 0)) {
        tx = P(int16_t, NODE(k), 4);
        ty = P(int16_t, NODE(k), 6);
    } else {
        const uint8_t *rec = ANIM(u, act);
        int32_t first = (int8_t)rec[0];
        unit_step_delta(id, first, rec, 0, phase, dir, &dx, &dy, &odd);
        if ((along_x && iabs(P(int16_t, NODE(k), 4) - x) < iabs(dx)) ||
            (along_y && iabs(P(int16_t, NODE(k), 6) - y) < iabs(dy))) {
            too_short = 1;
            *frames = first;
        }
        if (moving && !TURN_NEEDED(dir, next)) {
            path_point_offset(id, (uint16_t)k, (uint32_t)dx, (uint32_t)dy, &tx, &ty);
        } else {
            tx = (int16_t)(P(int16_t, NODE(k), 4) - dx);
            ty = (int16_t)(P(int16_t, NODE(k), 6) - dy);
        }
    }
    phase = 2;
    odd = (uint8_t)odd_;
    act = action;
    int32_t cycle = cycle_;
    const uint8_t *rec = data_;
    int32_t want_x = tx - x, want_y = ty - y;
    if (!want_x && !want_y)
        return 0;
    int32_t sx = 0, sy = 0;
    while (!(along_x && iabs(sx) >= iabs(want_x)) && !(along_y && iabs(sy) >= iabs(want_y)) && !too_short) {
        dx = 0;
        dy = 0;
        int32_t n = unit_step_delta(id, cycle, rec, 0, phase, dir, &dx, &dy, &odd);
        sx += dx;
        sy += dy;
        *frames += n;
        act = (uint8_t)unit_pose(id, act, (uint16_t)k, dir, next, &facing, 0, &phase, &moving);
        if (counts_as_step(act))
            count++;
        rec = ANIM(u, act);
        cycle = (int8_t)rec[0];
    }
    int16_t rx = (int16_t)(want_x - sx), ry = (int16_t)(want_y - sy);
    if ((!along_x || (iabs(sx) > iabs(want_x) && iabs(dx) / 2 < iabs(rx))) &&
        (!along_y || (iabs(sy) > iabs(want_y) && iabs(dy) / 2 < iabs(ry)))) {
        sx -= dx;
        sy -= dy;
        count--;
        rx = (int16_t)(want_x - sx);
        ry = (int16_t)(want_y - sy);
    }
    out[0] = rx;
    out[1] = ry;
    return count;
}
