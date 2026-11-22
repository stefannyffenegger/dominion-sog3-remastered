/* The game logic, 0x100dde66-0x100ddf63: the two tests of a ring entry. */
#include "iso_core.h"
#include "ringtest.h"

/* 0x100dde66: whether (x, y), relative to the map's centre, lies inside the map */
int32_t __cdecl ring_point_free(int32_t x, int32_t y)
{
    x += I(int32_t, 0xd709);
    y += I(int32_t, 0xd71e);
    return x > 0 && I(int32_t, 0xd6f1) > x && y > 0 && I(int32_t, 0xd6f5) > y;
}

/* 0x100ddeda: whether map cell `c` is taken: blocked (bits 0, 2, 3), holding an object
 * (bits 16-27) or marked by a unit (bit 28) */
int32_t __cdecl ring_cell_taken(int32_t c)
{
    uint32_t v = I(uint32_t *, 0x1934)[c];
    return (v & 1) || (v >> 3 & 1) || (v >> 16 & 0xfff) || (v >> 2 & 1) || (v >> 28 & 1);
}
