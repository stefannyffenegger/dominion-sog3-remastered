/* The game logic, 0x100cef5d-0x100cf1d3: whether a vehicle's patrol is still under way. */
#include "iso_core.h"
#include "patrolactive.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define S(u) P(uint16_t, u, 0x1cd)
#define AT(u, x, y) (P(int16_t, u, x) == P(int16_t, u, 0x52) && P(int16_t, u, y) == P(int16_t, u, 0x54))

/* notes where the unit stands as the patrol's stop (+0x360/+0x362) */
static void stop_here(uint8_t *u)
{
    P(int16_t, u, 0x360) = P(int16_t, u, 0x52);
    P(int16_t, u, 0x362) = P(int16_t, u, 0x54);
}

/* 0x100cef5d: whether unit `u`'s patrol (+0x113) is under way (1). It is not when the unit
 * stands at its stop (+0x1cd bit 3; else at its goal +0x70/+0x72), when it has stood still
 * for more than 150 (counted at +0x364 in steps of 3; it then stops there, setting bit 3),
 * when +0x1cd bit 1 is set, or when it stands on the patrol's only waypoint. */
int32_t __cdecl patrol_active(uint8_t *u)
{
    if (P(int16_t, u, 0x113) <= 0)
        return 0;
    int32_t on = 1;
    if (S(u) >> 3 & 1) {
        if (AT(u, 0x360, 0x362))
            on = 0;
    } else if (AT(u, 0x70, 0x72)) {
        on = 0;
    }
    if (on == 1 && !(P(uint32_t, u, 0x10e) >> 30 & 1)) {
        if (AT(u, 0x36c, 0x36e)) {
            P(int32_t, u, 0x364) += 3;
            if (P(int32_t, u, 0x364) > 0x96) {
                stop_here(u);
                S(u) |= 8;
                P(int32_t, u, 0x364) = 0;
                on = 0;
            }
        } else {
            P(int32_t, u, 0x364) = 0;
        }
    }
    if (on == 1 && (S(u) >> 1 & 1)) {
        stop_here(u);
        on = 0;
    }
    uint8_t rec[0x12];
    memcpy(rec, VEC(P(int16_t, u, 0x113)), 0x12);
    if (on == 1 && P(int16_t, rec, 0xe) <= 0 && P(int16_t, u, 0x52) == P(int16_t, rec, 4) &&
        P(int16_t, u, 0x54) == P(int16_t, rec, 6)) {
        stop_here(u);
        on = 0;
    }
    return on;
}
