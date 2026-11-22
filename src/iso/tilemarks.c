/* The game logic, 0x100f34a5-0x100f365b: which ground tiles take no burnt marks. */
#include "iso_core.h"
#include "tilemarks.h"

static int in(int32_t v, int32_t lo, int32_t hi)
{
    return v >= lo && v <= hi;
}

/* 0x100f34a5: whether tile `tile` of the map's tile set (+0xc340) takes no marks: a tile
 * in the set's first range with a nonzero record (table +0xd755), or in its second range */
int32_t __cdecl tile_takes_no_marks(int32_t tile)
{
    int32_t lo, hi, lo2, hi2;
    switch (I(uint16_t, 0xc340)) {
    case 0:
        lo = 0xc5d, hi = 0x11da, lo2 = 0x12ce, hi2 = 0x136f;
        break;
    case 1:
        lo = 0x8d8, hi = 0xd04, lo2 = 0xe9a, hi2 = 0xf3b;
        break;
    case 2:
        lo = 0xd23, hi = 0x135f, lo2 = 1, hi2 = 0;
        break;
    case 3:
        lo = 0xbf0, hi = 0x1168, lo2 = 0x12ad, hi2 = 0x134e;
        break;
    default:
        return 0;
    }
    if (in(tile, lo, hi) && *I(int32_t **, 0xd755)[tile])
        return 1;
    return in(tile, lo2, hi2);
}
