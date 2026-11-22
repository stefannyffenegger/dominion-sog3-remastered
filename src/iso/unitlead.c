/* The game logic, 0x100e5a4d-0x100e5c00: how far a unit gets during the network delay. */
#include "iso_core.h"
#include "netflags.h"
#include "unitdelta.h"
#include "unitlead.h"

/* 0x100e5a4d: the movement of unit `id` in direction `dir` over its turn time (type +0xc8)
 * or the network delay, whichever is longer, in whole walking cycles, into (*x, *y).
 * Returns the number of steps, 0 when the unit has no step data */
int32_t __cdecl unit_step_reach(uint32_t id, uint32_t dir, int32_t *x, int32_t *y)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    uint8_t *anim = P(uint8_t *, u, 0x15f);
    if (!anim)
        return 0;
    const uint8_t *data = anim + 0xdc;
    int32_t cycle = (int8_t)data[0];
    if (!cycle)
        return 0;
    int32_t time = P(uint32_t, P(uint8_t *, u, 0x32), 0xc8) > (uint32_t)net_delay()
                       ? P(int32_t, P(uint8_t *, u, 0x32), 0xc8)
                       : net_delay();
    int32_t n = time / cycle + (time % cycle != 0);
    int32_t total = 0, dx = 0, dy = 0;
    uint8_t odd = 0;
    *y = 0;
    *x = 0;
    for (int32_t i = 0; i < n; i++) {
        total += unit_step_delta(id, cycle, data, 0, 2, dir, &dx, &dy, &odd);
        *x += dx;
        *y += dy;
    }
    return total;
}
