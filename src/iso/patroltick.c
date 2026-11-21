/* The game logic, 0x100cc1be-0x100cc55c: the patrolling units' tick (every 3rd game tick;
 * the units' list is iso+0xd67a, iso+0x1b43 long). */
#include "iso_core.h"
#include "patroltick.h"

#define F(u) P(uint32_t, u, 0x10e)
#define S(u) P(uint16_t, u, 0x1cd)
#define path_sectors_draw ORIG(void(__cdecl *)(void), 0x1009a526)
#define fn_100d1afa ORIG(void(__cdecl *)(void), 0x100d1afa)
#define fn_100d601d ORIG(void(__cdecl *)(void), 0x100d601d)
#define fn_100ce8a6 ORIG(void(__cdecl *)(void), 0x100ce8a6)
#define fn_100cfb0d ORIG(void(__cdecl *)(void), 0x100cfb0d)
#define fn_100cdc9f ORIG(void(__cdecl *)(void), 0x100cdc9f)
#define patrol_blocked ORIG(int(__cdecl *)(uint8_t *), 0x100ce277)
#define soldier_patrol_check ORIG(int(__cdecl *)(uint8_t *), 0x100d0e81)
#define vehicle_patrol_check ORIG(int(__cdecl *)(uint8_t *), 0x100cc55c)
#define patrol_redo ORIG(void(__cdecl *)(uint8_t *), 0x100cc9ca)
#define patrols_cross ORIG(int(__cdecl *)(uint8_t *, uint8_t *), 0x100ccb6d)

/* a unit on patrol (+0x1cd bit 0) that is not entering or leaving anything (+0x10e bits
 * 4, 6) */
static int patrolling(uint8_t *u)
{
    return (S(u) & 1) && !(F(u) >> 4 & 1) && !(F(u) >> 6 & 1);
}

static int hovercraft(uint8_t *u)
{
    return P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37;
}

/* 0x100cc1be: each tick the sector debug view and 0x100d1afa; every 3rd tick the patrols:
 * soldiers (+0x28 == 1) whose patrol is blocked (0x100ce277, unless +0x1cd bit 9) or who
 * need it (0x100d0e81) plan again (0x100cc9ca), likewise vehicles (+0x28 == 3, not
 * hovercraft) with 0x100cc55c; a vehicle whose patrol is fine checks the vehicles after it
 * in the list for crossing patrols (0x100ccb6d) */
void __cdecl units_patrol_tick(void)
{
    path_sectors_draw();
    fn_100d1afa();
    if (I(uint32_t, 0x17dd) % 3)
        return;
    fn_100d601d();
    fn_100ce8a6();
    int32_t n = I(int32_t, 0x1b43);
    if (I(uint32_t, 0x17dd) % 3 == 0) {
        fn_100cfb0d();
        fn_100cdc9f();
        for (int32_t i = 0; i < n; i++) {
            uint8_t *u = I(uint8_t **, ISO_UNITS)[I(int32_t *, 0xd67a)[i]];
            if (!u || (F(u) >> 15 & 1) || P(uint16_t, u, 0x28) != 1)
                continue;
            if ((F(u) >> 4 & 1) || (F(u) >> 6 & 1))
                continue;
            if (!(S(u) & 1) || (S(u) >> 3 & 1))
                continue;
            if ((!(S(u) >> 9 & 1) && patrol_blocked(u) == 1) || soldier_patrol_check(u) == 1)
                patrol_redo(u);
        }
    }
    for (int32_t i = 0; i < n; i++) {
        uint8_t *u = I(uint8_t **, ISO_UNITS)[I(int32_t *, 0xd67a)[i]];
        if (!u || (F(u) >> 15 & 1) || P(uint16_t, u, 0x28) != 3 || hovercraft(u) || !patrolling(u))
            continue;
        if (!(S(u) >> 3 & 1)) {
            if ((!(S(u) >> 9 & 1) && patrol_blocked(u) == 1) || vehicle_patrol_check(u) == 1) {
                patrol_redo(u);
                continue;
            }
        }
        for (int32_t j = i + 1; j < n; j++) {
            uint8_t *o = I(uint8_t **, ISO_UNITS)[I(int32_t *, 0xd67a)[j]];
            if (!o || P(uint16_t, o, 0x28) != 3 || hovercraft(o) || !patrolling(o))
                continue;
            if (patrols_cross(u, o) == 1)
                break;
        }
    }
}
