/* The game logic, 0x100ba61f-0x100bb1cf: finding a free map cell for a unit near a point (a
 * move order's target, a group's place). The map cells (iso+0x1934) hold: bit 0/1 blocked
 * for ground and for all units, bit 3 taken, bits 4-7 and 8-11 the formation slots in use,
 * bit 12, bits 16-27 an object. The cells taken in one order are listed so that they can
 * be freed again (the original's globals at 0x1017dfc0-0x1017e3f0). */
#include "iso_core.h"
#include "spot.h"

#define F (P(uint32_t, u, 0x10e))
#define TX G(int32_t, 0x1017dfc0) /* the cell's column and row */
#define TY G(int32_t, 0x1017dfc4)
#define WX G(int32_t, 0x1017dfd8) /* its point */
#define WY G(int32_t, 0x1017dfdc)
#define DIRS ((int16_t *)ORIG_PTR(0x1017dfc8))  /* the directions tried, in turn */
#define RINGS ((int16_t *)ORIG_PTR(0x1017e1e0)) /* the same, with rings of cells */
#define TAKEN ((int32_t *)ORIG_PTR(0x1017dfe0)) /* the cells taken so far (128) */
#define NTAKEN G(uint8_t, 0x1017e3f0)
#define STEP(d, row, k) (*((const int8_t *)ORIG_PTR(0x1013b468) + (d) * 4 + ((row) & 1) * 2 + (k)))
#define slot_offset ORIG(void(__cdecl *)(uint32_t, int16_t, int32_t *), 0x10091860)

/* the slot nibbles (4-7 | 8-11) of a cell */
static uint32_t slots(uint32_t c)
{
    return (c >> 4 & 0xf) | (c >> 8 & 0xf);
}

/* whether a unit can stand on cell `c` at all (+0x3e bit 7: it only needs bit 1 clear) */
static int passable(uint8_t *u, uint32_t c)
{
    if (P(uint32_t, u, 0x3e) >> 7 & 1)
        return !(c >> 1 & 1);
    return !(c & 1) && !(c >> 1 & 1);
}

/* takes a free formation slot of cell `*c` for the unit (+0x10e bits 10-11, the old one
 * kept in bits 21-22); 0 when all four are in use */
static int take_slot(uint8_t *u, uint32_t *c)
{
    if (slots(*c) & 1u << (F >> 10 & 3)) {
        int s;
        for (s = 0; s < 4 && (slots(*c) & 1u << s); s++)
            ;
        if (s == 4)
            return 0;
        F = ((F >> 10 & 3) << 21) | (F & 0xff9fffff);
        F = (F & 0xfffff3ff) | (uint32_t)(s & 3) << 10;
    }
    *c = (((1u << (F >> 10 & 3)) | *c >> 4) & 0xf) << 4 | (*c & 0xffffff0f);
    return 1;
}

/* 0x100ba61f: finds the map cell for unit `unit` at or near (*x, *y): its own cell when it
 * is free (unless `moved`), otherwise the first free one of its 8 neighbours, tried from
 * direction `dir` on (`rings` 1: cells further out). Formation types (+0xb0) take one of a
 * cell's four slots. The cell's point goes to (*x, *y), the cell to *cell. `how` 0 and 3
 * start a new list of taken cells, 2 frees them all (and does nothing else), 3 makes the
 * cell the unit's own (+0x5a). 1 when a cell was found */
int32_t __cdecl formation_spot(uint32_t unit, uint32_t how, int32_t a3, uint32_t dir, int32_t *x, int32_t *y,
                               int32_t *cell, int32_t rings, int32_t moved)
{
    (void)a3;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    int32_t formation = u ? P(int32_t, P(uint8_t *, u, 0x32), 0xb0) : 0;
    uint32_t *c = 0;
    int32_t own = 0, found = 0, stay = 0;

    if (!(how & 0xffff) || (how & 0xffff) == 3) {
        memset(TAKEN, 0, 0x200);
        NTAKEN = 0;
    }
    if ((how & 0xffff) == 2) {
        for (int i = 0; i < NTAKEN; i++)
            I(uint32_t *, 0x1934)[TAKEN[i]] &= 0xffffff0f;
        return 0;
    }
    TX = *x;
    WX = TX;
    TY = *y;
    WY = TY;
    own = IsoVTilePos(g_iso, &TX, &TY, &WX, &WY);
    c = &I(uint32_t *, 0x1934)[own];
    if (!(*c >> 3 & 1) || (*c >> 12 & 1)) {
        if (passable(u, *c)) {
            if (formation) {
                if (slots(*c) < 0xf)
                    stay = 1;
            } else if (!(*c >> 8 & 0xf) && !(*c >> 4 & 0xf) && (!(*c >> 16 & 0xfff) || (*c >> 12 & 1))) {
                stay = 1;
            }
        }
    }
    if (moved)
        stay = 0;

    if (!stay) {
        int16_t d = (int16_t)dir;
        if (!rings) {
            if (d < 0 || d > 7) {
                for (int i = 0; i < 8; i++)
                    DIRS[i] = ((const int16_t *)ORIG_PTR(0x101582a0))[i];
            } else {
                DIRS[0] = d;
                for (int i = 0; i < 7; i++)
                    DIRS[1 + i] = ((const int16_t *)ORIG_PTR(0x101582b0))[(d + i) & 7];
            }
        } else {
            if (d < 0 || d > 7)
                d = 2;
            for (int i = 0; i < 8; i++)
                RINGS[i] = (int16_t)((d + i) & 7);
        }
        int32_t tx = TX, ty = TY;
        for (int k = 0; k < 8; k++) {
            if (rings == 1) {
                tx = TX;
                ty = TY;
                for (int j = 0; j < ((const int16_t *)ORIG_PTR(0x10158290))[RINGS[k]]; j++) {
                    tx += STEP(RINGS[k], ty, 0);
                    ty += STEP(RINGS[k], ty, 1);
                }
            } else {
                /* the steps add up: each direction goes on from the last cell tried */
                tx += STEP(DIRS[k], ty, 0);
                ty += STEP(DIRS[k], ty, 1);
            }
            if (tx < 0 || I(int32_t, 0x1a9c) <= tx || ty < 0 || I(int32_t, 0x1a98) <= ty)
                continue;
            *cell = (ty << I(uint8_t, 0x1aa0)) + tx;
            c = &I(uint32_t *, 0x1934)[*cell];
            if (*c >> 3 & 1)
                continue;
            if (!passable(u, *c))
                continue;
            if (formation ? !(slots(*c) < 0xf) : ((*c >> 4 & 0xf) || (*c >> 16 & 0xfff)))
                continue;
            if (formation) {
                if (!take_slot(u, c)) {
                    found = 0;
                    break;
                }
                TAKEN[NTAKEN++] = *cell;
                int32_t off[2];
                slot_offset(0, (int16_t)(F >> 10 & 3), off);
                TX = tx;
                TY = ty;
                *x = ((TY & 1) << 5) + (TX << 6) - I(int32_t, 0xd709) + off[0] + 0x20;
                WX = *x;
                *y = (TY << 4) - I(int32_t, 0xd71e) + off[1] + 0x10;
                WY = *y;
            } else {
                TX = tx;
                TY = ty;
                *x = ((TY & 1) << 5) + (TX << 6) - I(int32_t, 0xd709) + 0x20;
                WX = *x;
                *y = (TY << 4) - I(int32_t, 0xd71e) + 0x10;
                WY = *y;
                TAKEN[NTAKEN++] = *cell;
                *c |= 0xf0;
            }
            found = 1;
            break;
        }
    } else if (formation) {
        if (!take_slot(u, c))
            return 0;
        int32_t off[2];
        slot_offset(0, (int16_t)(F >> 10 & 3), off);
        *x = off[0] + WX + 0x20;
        *y = off[1] + WY + 0x10;
        *cell = own;
        TAKEN[NTAKEN++] = *cell;
        found = 1;
    } else {
        *x = WX + 0x20;
        *y = WY + 0x10;
        *cell = own;
        TAKEN[NTAKEN++] = *cell;
        *c |= 0xf0;
        found = 1;
    }

    if (found) {
        uint32_t *occ = I(uint32_t *, 0x1934);
        if (P(int32_t, u, 0x5a)) {
            if (formation) {
                uint32_t e = occ[P(int32_t, u, 0x5a)];
                occ[P(int32_t, u, 0x5a)] = ((e >> 4 & ~(1u << (F >> 28 & 3)) & 0xf) << 4) | (e & 0xffffff0f);
            } else {
                occ[P(int32_t, u, 0x5a)] &= 0xffffff0f;
            }
            P(int32_t, u, 0x5a) = 0;
        }
        if ((how & 0xffff) == 3) {
            P(int32_t, u, 0x5a) = *cell;
            if (P(int32_t, P(uint8_t *, u, 0x32), 0xb0))
                F = ((F >> 10 & 3) << 28) | (F & 0xcfffffff);
        }
    }
    return found;
}
