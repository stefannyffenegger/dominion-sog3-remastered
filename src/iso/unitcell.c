/* The game logic, 0x100ca4c6-0x100ca80b: the map cell a unit stands on (+0x56; the cell's
 * bit 2 marks it) and its terrain byte (+0x2f). With the debug flag (0xeea bit 0) the
 * cells are also drawn in a debug map (0x1960+8, the cell's old tile kept at +0x62). */
#include "iso_core.h"
#include "unitcell.h"

#define F (P(uint32_t, u, 0x10e))
#define OCC (I(uint32_t *, 0x1934))
#define DEBUG_MAP (P(uint16_t *, I(uint8_t *, 0x1960), 8))
#define dest_release ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x100be4d2)

/* 0x100ca4c6: notes the cell unit `unit` stands on; `how` 2 takes it off the map (it gives
 * up its cell, its formation slots and, in the editor, its claimed destination) */
void __cdecl unit_cell_update(uint32_t unit, uint32_t how)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    int x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54), wx, wy;
    if ((how & 0xffff) == 2) {
        if (I(int32_t, 0x154))
            dest_release(unit, 0);
        if (!P(int32_t, u, 0x56))
            return;
        OCC[P(int32_t, u, 0x56)] &= 0xfffffffb;
        if (F >> 7 & 1) {
            uint32_t e = OCC[P(int32_t, u, 0x56)];
            OCC[P(int32_t, u, 0x56)] = ((e >> 8 & ~(1u << (F >> 21 & 3)) & 0xf) << 8) | (e & 0xfffff0ff);
            F &= 0xffffff7f;
        }
        if (P(int32_t, u, 0x5a)) {
            uint32_t e = OCC[P(int32_t, u, 0x5a)];
            OCC[P(int32_t, u, 0x5a)] = ((e >> 4 & ~(1u << (F >> 28 & 3)) & 0xf) << 4) | (e & 0xffffff0f);
            P(int32_t, u, 0x5a) = 0;
            F &= 0xcfffffff;
        }
        P(int32_t, u, 0x56) = 0;
        /* (the original restores the debug map at the cell it has just cleared, cell 0) */
        if (I(uint8_t, 0xeea) & 1)
            DEBUG_MAP[P(int32_t, u, 0x56)] = (uint16_t)P(int32_t, u, 0x62);
        return;
    }
    int32_t cell = IsoVTilePos(g_iso, &x, &y, &wx, &wy);
    if (I(uint32_t, 0x1a9c) <= (uint32_t)x || I(uint32_t, 0x1a98) <= (uint32_t)y) {
        P(int32_t, u, 0x56) = 0;
        return;
    }
    P(uint8_t, u, 0x2f) = (uint8_t)(I(uint32_t *, 0x1904)[cell] >> 24);
    if (P(int32_t, u, 0x56) == cell)
        return;
    if (P(int32_t, u, 0x56)) {
        OCC[P(int32_t, u, 0x56)] &= 0xfffffffb;
        if (I(uint8_t, 0xeea) & 1)
            DEBUG_MAP[P(int32_t, u, 0x56)] = (uint16_t)P(int32_t, u, 0x62);
    }
    P(int32_t, u, 0x56) = cell;
    OCC[P(int32_t, u, 0x56)] |= 4;
    if (I(uint8_t, 0xeea) & 1) {
        P(int32_t, u, 0x62) = I(uint32_t *, 0x1904)[cell] & 0x7fff;
        DEBUG_MAP[cell] = 2;
    }
}

/* ---- 0x100ca80b-0x100caa96 ---- */

#define unit_kill ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x1000f5e5)
#define score_unit_killed ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x1007e870)
#define unit_buildup_set ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x10076d00)
#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])

/* 0x100ca80b: unit `unit` runs over the enemy soldiers (+0x28 == 1) on its cell: those of
 * players it is not allied with (+0x1ba0 of the player block, 0x1461 bytes each) and not
 * lying down (+0x196 0x23) are killed and scored; a unit that builds up (+0x84) then counts
 * up. In a network game only on the host. The sector's units (iso+0xc2eb, 0x1e bytes each,
 * +2 the first) are linked through +0x393. */
void __cdecl unit_run_over(uint32_t unit)
{
    uint8_t *u = UNIT(unit & 0xffff);
    int32_t killed = 0;
    if (I(int32_t, 0x175) && (!I(int32_t, 0xd9ca) || !I(int32_t, 0x148)))
        return;
    uint8_t *sector = I(uint8_t *, 0xc2eb) + P(int16_t, u, 0x1a1) * 0x1e;
    for (uint8_t *o = UNIT(P(uint16_t, sector, 2)); o; o = UNIT(P(uint16_t, o, 0x393))) {
        if (P(uint16_t, o, 0x28) != 1 || P(int32_t, u, 0x56) != P(int32_t, o, 0x56))
            continue;
        if (P(uint8_t, u, 0x14) == P(uint8_t, o, 0x14))
            continue;
        if (I(uint8_t, 0x1ba0 + P(uint8_t, u, 0x14) * 0x1461) & (1u << P(uint8_t, o, 0x14)))
            continue;
        if (P(uint8_t, o, 0x196) == 0x23 || G(int32_t, 0x1013d6f4))
            continue;
        /* (a debug line with both players is compiled out) */
        unit_kill(P(uint16_t, o, 0x395), 0x21, 1, 0);
        if (!(P(uint32_t, o, 0x3e) >> 30 & 1)) {
            score_unit_killed(P(uint32_t, o, 0x10), ((uint32_t)(uintptr_t)o & 0xffffff00) | P(uint8_t, o, 0x14),
                              ((uint32_t)(uintptr_t)u & 0xffffff00) | P(uint8_t, u, 0x14));
            P(uint32_t, o, 0x3e) |= 0x40000000;
        }
        killed++;
    }
    if (killed > 0 && P(int32_t, u, 0x84))
        unit_buildup_set(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb), 2);
}

/* ---- 0x100caa96-0x100caf39 ---- */

#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)
#define flags_clear_field16 ORIG(void(__cdecl *)(uint32_t *), 0x10070a64)
#define crt_abs ORIG(int(__cdecl *)(int), 0x100f4f80)
#define TYPE74(u) P(int32_t, P(uint8_t *, u, 0x32), 0x74)

/* 0x100caa96: the same for the other units (vehicles): a type with +0x74 == 3 (not while
 * packing up, 0x45) also takes its cell (bit 3) and runs over the soldiers on it; +0x3e bit
 * 19 notes an odd row. In a network game a unit off the map (but hovercraft, 0x30-0x37) is
 * deleted on the host ("Unit off map. Deleting!") */
void __cdecl unit_cell_update2(uint32_t unit, uint32_t how)
{
    uint8_t *u = UNIT(unit & 0xffff);
    int x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54), wx, wy;
    if ((how & 0xffff) == 2) {
        if (!P(int32_t, u, 0x56))
            return;
        if (P(int32_t, u, 0x5a)) {
            OCC[P(int32_t, u, 0x5a)] &= 0xffffff0f;
            P(int32_t, u, 0x5a) = 0;
        }
        if (TYPE74(u) == 3 && P(uint8_t, u, 0x147) != 0x45) {
            uint32_t *p = &OCC[P(int32_t, u, 0x56)];
            *p &= 0xfffffff7;
            if ((*p >> 16 & 0xfff) == (unit & 0xffff))
                flags_clear_field16(p);
        }
        if (I(uint8_t, 0xeea) & 1)
            DEBUG_MAP[P(int32_t, u, 0x56)] = (uint16_t)P(int32_t, u, 0x62);
        P(int32_t, u, 0x56) = 0;
        return;
    }
    if (I(int32_t, 0x175) && !(P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37)) {
        if (crt_abs(x) + I(int32_t, 0xd709) > I(int32_t, 0xd6f1) || crt_abs(y) + I(int32_t, 0xd71e) > I(int32_t, 0xd6f5)) {
            if (I(int32_t, 0x148)) {
                message_box(0, (const char *)ORIG_PTR(0x101590a8), (const char *)ORIG_PTR(0x10159098), 0);
                unit_kill(unit & 0xffff, 0x20, 1, 0);
                uint8_t *v = UNIT(unit & 0xffff);
                if (!(P(uint32_t, v, 0x3e) >> 30 & 1)) {
                    uint32_t p = ((uint32_t)(uintptr_t)v & 0xffffff00) | P(uint8_t, v, 0x14);
                    score_unit_killed(P(uint32_t, v, 0x10), p, p);
                    P(uint32_t, UNIT(unit & 0xffff), 0x3e) |= 0x40000000;
                }
            }
            return;
        }
    }
    int32_t cell = IsoVTilePos(g_iso, &x, &y, &wx, &wy);
    if (I(uint32_t, 0x1a9c) <= (uint32_t)x || I(uint32_t, 0x1a98) <= (uint32_t)y) {
        P(int32_t, u, 0x56) = 0;
        return;
    }
    P(uint8_t, u, 0x2f) = (uint8_t)(I(uint32_t *, 0x1904)[cell] >> 24);
    if (P(int32_t, u, 0x56) == cell)
        return;
    if (P(int32_t, u, 0x56) && TYPE74(u) == 3 && P(uint8_t, u, 0x147) != 0x45) {
        int32_t old = P(int32_t, u, 0x56);
        OCC[old] &= 0xfffffff7;
        if (I(uint8_t, 0xeea) & 1)
            DEBUG_MAP[old] = (uint16_t)P(int32_t, u, 0x62);
    }
    P(int32_t, u, 0x56) = cell;
    if (y & 1)
        P(uint32_t, u, 0x3e) |= 0x80000;
    else
        P(uint32_t, u, 0x3e) &= 0xfff7ffff;
    if (TYPE74(u) == 3) {
        int32_t at = P(int32_t, u, 0x56);
        if ((OCC[at] >> 8 & 0xf) && P(int16_t, u, 0x1a1) >= 0)
            unit_run_over(unit);
        if (!(OCC[at] >> 3 & 1))
            OCC[at] |= 8;
        if (I(uint8_t, 0xeea) & 1)
            DEBUG_MAP[at] = 3;
    }
}
