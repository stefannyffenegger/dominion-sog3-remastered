/* The game logic: two small unit tests from 0x100d25a3-0x100d27d8 (the others in that range
 * are not reached by the tests and stay original). */
#include "iso_core.h"
#include "unitrel.h"

/* 0x100d25a3: whether players `a` and `b` are the same, or, in a network game only,
 * allied (a's alliance bits, +0x1ba0 of the player block, 0x1461 bytes each) */
int32_t __cdecl players_allied(uint32_t a, uint32_t b)
{
    if ((b & 0xffff) == (a & 0xffff))
        return 1;
    if (!I(int32_t, 0x175))
        return 0;
    return (I(uint8_t, 0x1ba0 + (a & 0xffff) * 0x1461) & (1u << (b & 0x1f))) != 0;
}

/* 0x100d26fb: whether a unit is a hovercraft: its class (+0x10, the type number times 4) is
 * 0x30-0x37, the Hover Bomber or Hover APC */
int32_t __cdecl ignores_marks(const uint8_t *u)
{
    return u && P(int32_t, u, 0x10) >= 0x30 && P(int32_t, u, 0x10) <= 0x37;
}
