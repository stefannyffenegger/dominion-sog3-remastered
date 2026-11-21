/* The game logic, 0x100ce8a6-0x100cef5d: the patrolling vehicles' cells (every 3rd tick).
 * A vehicle on patrol holds a map cell (+0x368, marked with its id in bits 16-27 and bit 3)
 * and remembers where it stood the last time (+0x36c/+0x36e). */
#include "iso_core.h"
#include "patrolveh.h"

#define OCC (I(uint32_t *, 0x1934))
#define S(u) P(uint16_t, u, 0x1cd)
#define OBJ(p) (*(p) >> 16 & 0xfff)
#define patrol_active ORIG(int(__cdecl *)(uint8_t *), 0x100cef5d)
#define patrol_reset ORIG(void(__cdecl *)(uint8_t *), 0x100cb37a)
#define flags_clear_bit3 ORIG(void(__cdecl *)(uint32_t *), 0x100708b6)
#define flags_clear_field16 ORIG(void(__cdecl *)(uint32_t *), 0x10070a64)
#define flags_set_field16 ORIG(void(__cdecl *)(uint32_t *, int32_t), 0x10070a36)
#define fn_10070899 ORIG(void(__cdecl *)(uint32_t *), 0x10070899)
#define patrol_cell_taken ORIG(void(__cdecl *)(uint8_t *, uint32_t *), 0x100cf5a1)
#define patrol_stop ORIG(void(__cdecl *)(uint8_t *), 0x100cf9df)
#define unit_free_links ORIG(void(__cdecl *)(uint32_t), 0x100dd5db)
#define fn_100d27d8 ORIG(int(__cdecl *)(uint32_t), 0x100d27d8)
#define vehicle_back_out ORIG(void(__cdecl *)(uint8_t *), 0x100caf40)
#define fn_100cf1d3 ORIG(void(__cdecl *)(uint8_t *), 0x100cf1d3)
#define unit_set_hold ORIG(void(__cdecl *)(uint8_t *, uint32_t), 0x100ce71f)
#define cell_of ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100ce756)
#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)

/* marks the vehicle's own cell (+0x56) as taken by it, when no object holds it */
static void mark_own_cell(uint8_t *u, int32_t id)
{
    if (!P(int32_t, u, 0x56))
        return;
    uint32_t *p = &OCC[P(int32_t, u, 0x56)];
    if (OBJ(p))
        return;
    *p |= 8;
    flags_set_field16(p, id);
}

/* 0x100ce8a6: for each vehicle (+0x28 == 3, not hovercraft, not inside, +0x3e bit 16):
 * while its patrol runs (0x100cef5d) it plans its legs again and moves its held cell along;
 * otherwise it stops patrolling, gives up a held cell (or backs out of a cell another
 * object holds, 0x100caf40) and holds its own; +0x1cd bit 1 notes that it stood still */
void __cdecl vehicles_patrol_update(void)
{
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        int32_t id = I(int32_t *, 0xd67a)[i];
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        if (id <= 0 || id >= 0x7d0 || !u)
            continue;
        int veh = P(uint16_t, u, 0x28) == 3 && !(P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37);
        if ((P(uint32_t, u, 0x10e) >> 15 & 1) || !veh || (P(uint32_t, u, 0x3e) >> 16 & 1))
            continue;
        if (patrol_active(u) == 1) {
            if (!(S(u) & 1)) {
                S(u) |= 1;
                S(u) &= 0xffef;
            }
            patrol_reset(u);
            int32_t cell = P(int32_t, u, 0x56);
            if (P(int32_t, u, 0x368) != cell && P(int32_t, u, 0x368)) {
                uint32_t *p = &OCC[P(int32_t, u, 0x368)];
                if ((int32_t)OBJ(p) == id) {
                    flags_clear_bit3(p);
                    flags_clear_field16(p);
                }
                P(int32_t, u, 0x368) = cell;
                patrol_cell_taken(u, &OCC[cell]);
            }
            if ((S(u) >> 3 & 1) && crt_abs(P(int16_t, u, 0x52) - P(int16_t, u, 0x360)) < 0x10 &&
                crt_abs(P(int16_t, u, 0x54) - P(int16_t, u, 0x362)) < 0x10) {
                uint32_t *p = &OCC[cell];
                if (!OBJ(p)) {
                    fn_10070899(p);
                    flags_set_field16(p, id);
                }
            }
        } else {
            if (S(u) & 1)
                patrol_stop(u);
            S(u) &= 0xfffe;
            if (S(u) >> 4 & 1) {
                mark_own_cell(u, id);
            } else {
                if (P(int16_t, u, 0x11b) <= 0) {
                    uint8_t a = P(uint8_t, u, 0x196);
                    if (a == 0x18 || a == 0xb || a == 0x14)
                        unit_free_links(id);
                    if (crt_abs(P(int16_t, u, 0x52) - P(int16_t, u, 0x70)) <= 2 &&
                        crt_abs(P(int16_t, u, 0x54) - P(int16_t, u, 0x72)) <= 2) {
                        uint32_t *p = &OCC[P(int32_t, u, 0x368)];
                        if (OBJ(p) && (int32_t)OBJ(p) != id && !fn_100d27d8(OBJ(p)) && !(*p >> 12 & 1) &&
                            !(*p >> 13 & 1)) {
                            vehicle_back_out(u);
                        } else {
                            S(u) |= 0x10;
                            unit_set_hold(u, 0);
                            P(int16_t, u, 0x1e7) = 0;
                        }
                    }
                }
                if (!(S(u) >> 4 & 1)) {
                    if (S(u) >> 3 & 1)
                        fn_100cf1d3(u);
                    if (P(int32_t, u, 0x368)) {
                        uint32_t *p = &OCC[P(int32_t, u, 0x368)];
                        if ((int32_t)OBJ(p) == id) {
                            *p &= 0xfffffff7;
                            flags_clear_field16(p);
                        }
                    }
                    /* (the original works out the cell it stands on and does not use it) */
                    cell_of(P(int16_t, u, 0x52), P(int16_t, u, 0x54));
                    mark_own_cell(u, id);
                    P(int32_t, u, 0x368) = P(int32_t, u, 0x56);
                } else {
                    mark_own_cell(u, id);
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
}
