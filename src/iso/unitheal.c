/* The game logic, 0x100dbdae-0x100dbe86: a soldier in its resting pose getting health back. */
#include "iso_core.h"
#include "playerinfo.h"
#include "sector.h"
#include "unitheal.h"

#define health_bar ORIG(void(__cdecl *)(uint32_t, int32_t), 0x1004ebe6)
#define net_health ORIG(void(__cdecl *)(uint32_t), 0x1003d00d)
#define HW(u, off) (((uint32_t)(uintptr_t)(u) & 0xffff0000) | P(uint16_t, u, off))

/* 0x100dbdae: a unit of types 0-7 whose pose (+0x147) is the one for its direction
 * (0x2a + 3 * +0x79) and that is not at full health gains 1 point every 8th tick */
void __cdecl unit_rest_heal(uint8_t *u)
{
    if (P(int32_t, u, 0xc) < 0 || P(int32_t, u, 0xc) > 7)
        return;
    if (P(uint8_t, u, 0x79) * 3 + 0x2a != P(uint8_t, u, 0x147))
        return;
    if ((int16_t)unit_max_health(u, 0) <= P(int16_t, u, 0xd6) || (I(uint8_t, 0x17dd) & 7))
        return;
    P(int16_t, u, 0xd6)++;
    health_bar(HW(u, 0x395), 0);
    if (I(int32_t, 0x175) && unit_is_local_u(u))
        net_health(HW(u, 0x395));
}
