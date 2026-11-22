/* The game logic, 0x100e5814-0x100e59db: how far a unit moves in a number of animation steps. */
#include "iso_core.h"
#include "unitdelta.h"

#define TAB(a, i) G(int8_t, (a) + (i))

/* 0x100e5814: adds up unit `id`'s movement over `steps` steps into (*dx, *dy). Vehicles of
 * types 0x28-0x3f in their first two phases use the table at 0x101589d8 (by direction, phase
 * and frame); all others read it from the step data `data`, alternating between two columns
 * with *odd. Returns the number of steps, -1 without a unit */
int32_t __cdecl unit_step_delta(uint32_t id, int32_t steps, const uint8_t *data, uint32_t frame_, uint32_t phase_,
                                uint32_t dir_, int32_t *dx, int32_t *dy, uint8_t *odd)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return -1;
    uint8_t frame = (uint8_t)frame_, phase = (uint8_t)phase_, dir = (uint8_t)dir_;
    int32_t count = 0;
    *dx = 0;
    *dy = 0;
    for (; steps; steps--, count++) {
        int32_t a, b;
        if (P(int32_t, u, 0xc) >= 0x28 && P(int32_t, u, 0xc) < 0x40 && phase < 2) {
            int32_t k = dir * 24 + frame * 2 + phase * 12;
            a = TAB(0x101589d8, k);
            b = TAB(0x101589d9, k);
            if (steps == 1)
                phase = 2;
            frame++;
        } else if (!*odd) {
            a = (int8_t)data[TAB(0x1015b580, dir) + 4] * TAB(0x1015b568, dir);
            b = (int8_t)data[TAB(0x1015b588, dir) + 4] * TAB(0x1015b570, dir);
        } else {
            a = (int8_t)data[TAB(0x1015b590, dir) + 4] * TAB(0x1015b568, dir);
            b = (int8_t)data[TAB(0x1015b598, dir) + 4] * TAB(0x1015b570, dir);
        }
        *odd ^= 1;
        *dx += a;
        *dy += b;
    }
    return count;
}
