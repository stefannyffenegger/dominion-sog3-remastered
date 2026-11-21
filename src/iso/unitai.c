/* The game logic, 0x100b3760-0x100b39aa: telling the AI about its units' events, and sorting
 * units by distance. */
#include "iso_core.h"
#include "unitai.h"

#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, uint32_t), 0x100e42a6)

/* 0x100b3760: for a unit of a computer player (+0x1b69 == 2): event 0xb (attacked) is passed
 * on to the AI; 1 for a computer player's unit, else 0 */
int32_t __cdecl unit_ai_event(uint32_t unit, int32_t event, uint32_t arg)
{
    uint32_t p = P(uint8_t, I(uint8_t **, ISO_UNITS)[unit & 0xffff], 0x14);
    if (I(int32_t, p * ISO_PLAYER_SIZE + 0x1b69) != 2)
        return 0;
    if (event == 0xb)
        ai_event(p, 3, unit & 0xffff, arg);
    return 1;
}

/* 0x100b38d4: a qsort comparison of two unit ids by their distance (+0xc2); 0 if one is gone */
int32_t __cdecl units_by_distance(const uint16_t *a, const uint16_t *b)
{
    uint8_t *ua = I(uint8_t **, ISO_UNITS)[*a], *ub = I(uint8_t **, ISO_UNITS)[*b];
    if (!ua || !ub)
        return 0;
    int32_t da = P(int32_t, ua, 0xc2), db = P(int32_t, ub, 0xc2);
    return da < db ? -1 : db == da ? 0 : 1;
}
