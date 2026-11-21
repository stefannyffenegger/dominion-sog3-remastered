/* The game logic, 0x100cfb0d-0x100cff4a: the patrolling soldiers (every 3rd tick). */
#include "iso_core.h"
#include "patrolsoldier.h"

#define S(u) P(uint16_t, u, 0x1cd)
#define STUCK ((uint32_t *)ORIG_PTR(0x10180358)) /* 8 bytes each: the unit, a sort key */
#define patrol_active ORIG(int(__cdecl *)(uint8_t *), 0x100cef5d)
#define patrol_reset ORIG(void(__cdecl *)(uint8_t *), 0x100cb37a)
#define patrol_cell_taken ORIG(void(__cdecl *)(uint8_t *, uint32_t *), 0x100cf5a1)
#define unit_set_hold ORIG(void(__cdecl *)(uint8_t *, uint32_t), 0x100ce71f)
#define fn_100cf1d3 ORIG(void(__cdecl *)(uint8_t *), 0x100cf1d3)
#define soldier_stuck ORIG(int(__cdecl *)(uint8_t *), 0x100ccb2b)
#define soldier_way_clear ORIG(int(__cdecl *)(uint8_t *), 0x100d0270)
#define soldier_unstick ORIG(void(__cdecl *)(uint8_t *, int), 0x100cff4a)
#define orig_qsort ORIG(void(__cdecl *)(void *, size_t, size_t, void *), 0x100f57d0)
#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)

/* 0x100cfb0d: for each soldier (+0x28 == 1): while its patrol runs (0x100cef5d) it plans its
 * legs again and moves its held cell along (+0x368, 0x100cf5a1); otherwise it stops at its
 * goal, and +0x1cd bit 1 notes that it stood still at its stop. Then up to 30 soldiers that
 * stood still and are stuck (0x100ccb2b) are sorted by the key next to them in the list
 * 0x10180358 (set elsewhere; comparator 0x100cb254) and the first whose way is not clear
 * (0x100d0270) is helped out (0x100cff4a). */
void __cdecl soldiers_patrol_update(void)
{
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        int32_t id = I(int32_t *, 0xd67a)[i];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (id <= 0 || id >= 0x7d0 || !u)
            continue;
        if ((P(uint32_t, u, 0x10e) >> 15 & 1) || P(uint16_t, u, 0x28) != 1)
            continue;
        if (patrol_active(u) == 1) {
            if (!(S(u) & 1)) {
                S(u) |= 1;
                S(u) &= 0xffef;
            }
            patrol_reset(u);
            int32_t cell = P(int32_t, u, 0x56);
            if (P(int32_t, u, 0x368) != cell) {
                P(int32_t, u, 0x368) = cell;
                patrol_cell_taken(u, &I(uint32_t *, 0x1934)[cell]);
            }
        } else {
            S(u) &= 0xfffe;
            if (!(S(u) >> 4 & 1)) {
                if (P(int16_t, u, 0x11b) <= 0 && crt_abs(P(int16_t, u, 0x52) - P(int16_t, u, 0x70)) <= 2 &&
                    crt_abs(P(int16_t, u, 0x54) - P(int16_t, u, 0x72)) <= 2) {
                    S(u) |= 0x10;
                    unit_set_hold(u, 0);
                    P(int16_t, u, 0x1e7) = 0;
                }
                if (!(S(u) >> 4 & 1)) {
                    if (S(u) >> 3 & 1)
                        fn_100cf1d3(u);
                    P(int32_t, u, 0x368) = P(int32_t, u, 0x56);
                }
            }
            S(u) &= 0xfffd;
            if (P(int16_t, u, 0x36c) == P(int16_t, u, 0x52) && P(int16_t, u, 0x36e) == P(int16_t, u, 0x54) &&
                (S(u) >> 3 & 1) && !(S(u) >> 4 & 1))
                S(u) |= 2;
        }
        P(int16_t, u, 0x36c) = P(int16_t, u, 0x52);
        P(int16_t, u, 0x36e) = P(int16_t, u, 0x54);
    }
    int32_t cnt = 0;
    for (int32_t i = 0; i < n && cnt < 0x1e; i++) {
        int32_t id = I(int32_t *, 0xd67a)[i];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (!u || (P(uint32_t, u, 0x10e) >> 15 & 1) || P(uint16_t, u, 0x28) != 1 || !(S(u) >> 1 & 1))
            continue;
        if (soldier_stuck(u) == 1)
            STUCK[cnt++ * 2] = (uint32_t)id;
    }
    if (cnt <= 0)
        return;
    orig_qsort(STUCK, cnt, 8, ORIG_PTR(0x100cb254));
    for (int32_t i = 0; i < cnt; i++) {
        uint8_t *u = I(uint8_t **, ISO_UNITS)[STUCK[i * 2]];
        if (!soldier_way_clear(u)) {
            soldier_unstick(u, 1);
            break;
        }
    }
}
