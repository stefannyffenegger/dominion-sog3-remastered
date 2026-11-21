/* The game logic, 0x100c3240-0x100c561e: a ground unit's tick (the type handler at
 * 0x101545bc+type*0x44 for the units that walk: what it does each game tick, by its state).
 * DRAFT */
#include "iso_core.h"
#include "sector.h"
#include "unitthink.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define F (P(uint32_t, u, 0x10e))
#define F3E (P(uint32_t, u, 0x3e))
#define TYPEFN(type, slot) (*(void **)ORIG_PTR(0x101545bc + (type) * 0x44 + (slot)))

#define unit_order ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_sector_update ORIG(void(__cdecl *)(uint32_t, int, int), 0x1000a474)
#define unit_kill ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x1000f5e5)
#define IsoDeleteUnit ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, uint32_t), 0x100ab4aa)
#define IsoAddObject ORIG(int32_t(__stdcall *)(uint8_t *, int, int, int, int, int, int), 0x100aca6e)
#define unit_busy ORIG(int(__cdecl *)(uint32_t), 0x100e8836)
#define unit_effects_clear ORIG(void(__cdecl *)(uint32_t), 0x1006d516)
#define fn_1006a5c1 ORIG(void(__cdecl *)(uint32_t), 0x1006a5c1)
#define fn_100d979f ORIG(int(__cdecl *)(uint32_t, int, int32_t, int32_t, int), 0x100d979f)
#define dest_release ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x100be4d2)
#define SpriteHitDetect ORIG(int(__stdcall *)(void *, void *, int, int, int), 0x100f49be)
#define fn_100bca4c ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bca4c)
#define fn_100bd8e8 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bd8e8)
#define fn_100bdab0 ORIG(void(__cdecl *)(uint32_t), 0x100bdab0)
#define path_start ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t), 0x100b5897)
#define path_turn                                                                            \
    ORIG(uint32_t(__cdecl *)(uint32_t, char, int16_t, uint8_t, uint8_t, uint8_t *, int, void *, void *), 0x100e4c70)
#define DebugStr ORIG(void(__cdecl *)(const char *, ...), 0x100f492e)
#define unit_action_done ORIG(void(__cdecl *)(uint32_t), 0x10030d7d)
#define path_plan                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int16_t, int16_t, uint8_t, uint8_t, uint8_t, int16_t, uint8_t, int,     \
                        char *, int, int16_t *, int *),                                                     \
         0x100e5c00)
#define path_step                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int, int16_t *, int16_t *, int16_t *, int16_t *, int16_t *), 0x100e6328)

#define unit_set_flag_2a ORIG(void(__cdecl *)(uint8_t *, uint8_t), 0x100a2200)
#define unit_attack ORIG(int(__cdecl *)(uint32_t), 0x100e7b9b)
#define unit_guard ORIG(int(__cdecl *)(uint32_t), 0x100e80fa)
#define unit_dying ORIG(void(__cdecl *)(uint32_t), 0x100e83f5)
#define unit_set_d6f9 ORIG(void(__cdecl *)(uint32_t, int), 0x100e8f48)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)
#define unit_voice ORIG(int(__cdecl *)(uint8_t *, int, int, int), 0x1001b5ec)
#define general_sound ORIG(int(__cdecl *)(int, int16_t, int16_t, int, int), 0x1001bbfc)
#define player_announce ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x1005810f)
#define unit_buildup_set ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x10076d00)
#define player_replace_d6cb ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1005ab95)
#define player_replace_d6f9 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1005acf2)
#define status_text ORIG(int(__cdecl *)(const char *, int), 0x10053ef5)
#define fn_10068c89 ORIG(void(__cdecl *)(uint32_t), 0x10068c89)
#define fn_1007549f ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int), 0x1007549f)
#define IsoUpdateFootprint ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, int, int32_t), 0x100ef91a)
#define STEP_DX G(int32_t, 0x10118b20)
#define STEP_DY G(int32_t, 0x10118b24)
#define fn_100c697c ORIG(void(__cdecl *)(uint32_t), 0x100c697c)
#define fn_100d9ead ORIG(int(__cdecl *)(uint32_t, int32_t *, int), 0x100d9ead)
#define fn_100d9fa9 ORIG(int(__cdecl *)(uint32_t, uint32_t, int), 0x100d9fa9)
#define carried_follow ORIG(void(__cdecl *)(uint32_t), 0x100bdbeb)
#define path_move                                                                            \
    ORIG(int(__cdecl *)(int16_t *, int16_t *, int16_t *, int16_t *, int16_t *, int16_t *, int32_t *, int32_t *), \
         0x100e64f3)
#define TURN5E0(a, b) (*((const uint8_t *)ORIG_PTR(0x1015b5e0) + (a) * 8 + (b)))
#define TURN5A0(a, b) (*((const int8_t *)ORIG_PTR(0x1015b5a0) + (a) * 8 + (b)))
#define TURN620(a, b) (*((const int8_t *)ORIG_PTR(0x1015b620) + (a) * 8 + (b)))
typedef void(__cdecl *KindFn)(uint8_t *);
#define KIND_FN(kind, slot) G(KindFn, 0x10118d8c + (kind) * 12 + (slot))

/* turns the unit on a step of its patrol path (cases 0x11/0x12, 0x19/0x1a) */
static void patrol_turn(uint32_t unit, uint8_t *u)
{
    if (P(int16_t, u, 0x113) <= 0) {
        F |= 0x8000000;
        unit_order(unit, 0, 1, 0);
        return;
    }
    int32_t t40, t44;
    unit_order(unit,
               path_turn(unit, (char)P(uint8_t, u, 0x147), P(int16_t, u, 0x113), P(uint8_t, u, 0x79),
                         P(uint8_t, u, 0x7a), u + 0x7b, 1, &t40, &t44),
               1, 0);
    P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x79);
    F = (F & 0xbfffffff) | (uint32_t)(t44 & 1) << 30;
}

/* the facing step of the turning actions: +0x7b from the turn table, then action `next` */
static void turn_step(uint32_t unit, uint8_t *u, uint32_t next)
{
    P(uint8_t, u, 0x7b) = TURN5E0(P(uint8_t, UNIT(unit & 0xffff), 0x7a), P(uint8_t, UNIT(unit & 0xffff), 0x79));
    unit_order(unit, next, 1, 0);
    F |= 0x40000000;
}

/* the record pointer's upper half with a word below it, as the original passes such words */
static uint32_t hi(const void *p, uint16_t v)
{
    return ((uint32_t)(uintptr_t)p & 0xffff0000) | v;
}

void __cdecl unit_think(uint32_t unit)
{
    int32_t d1c = 0, d4 = 0; /* d4: set by the turning actions, never read (as in the original) */
    uint8_t *u = UNIT(unit & 0xffff);
    if (P(uint8_t, u, 0x158))
        goto L4ffb;
    if ((F3E >> 22 & 1) || P(uint8_t, u, 0x148) == 0x4c) {
        IsoDeleteUnit(g_iso, unit & 0xffff, ~(F3E >> 17) & 1);
        return;
    }
    if (I(int32_t, 0x175) && P(int16_t, u, 0x113) > 0 && P(int16_t, VEC(P(int16_t, u, 0x113)), 0xe) == -2 &&
        P(uint8_t, u, 0x196) != 0x1a)
        return;
    if (P(uint8_t, u, 0x148) == 0x52) {
        unit_order(unit, 0, 1, 0);
        F &= 0xffffefff;
        IsoAddObject(g_iso, 0x122, P(int16_t, u, 0x52) - 0x20, P(int16_t, u, 0x54) - 0x10, 0, 0, 2);
        unit_sector_update(unit, 1, 1);
        if (F3E >> 28 & 1)
            unit_set_action(unit, 0x3b, 1, 0);
        return;
    }
    if ((F3E >> 16 & 1) && P(uint8_t, u, 0x148) == 0x4d)
        return;
    if (unit_busy(unit))
        return;
    if (P(int16_t, u, 0x1b3) >= 0) {
        if (!P(int16_t, u, 0x1b3)) {
            unit_effects_clear(unit);
            P(int16_t, u, 0x1b3) = -1;
        } else {
            P(int16_t, u, 0x1b3)--;
        }
    }
    if (P(int16_t, u, 0x1c9) >= 0) {
        if (!P(int16_t, u, 0x1c9))
            fn_1006a5c1(unit);
        else
            P(int16_t, u, 0x1c9)--;
    }
    if (F >> 27 & 1) {
        F &= 0xf7ffffff;
        int32_t at_end = 0;
        if (P(int16_t, u, 0x119) > 0 && P(int16_t, u, 0x11b) > 0 && P(int16_t, VEC(P(int16_t, u, 0x119)), 0xe) <= 0 &&
            P(int16_t, VEC(P(int16_t, u, 0x11b)), 0xe) <= 0)
            at_end = 1;
        if (P(int16_t, u, 0x113) <= 0 && at_end &&
            fn_100d979f(unit, 0x20, P(int16_t, u, 0x70), P(int16_t, u, 0x72), 1)) {
            ((void(__cdecl *)(uint32_t, int, int))TYPEFN(P(int32_t, u, 0xc), 0x3c))(unit, (F3E >> 18 & 1) ? 0x10 : 0xb, 1);
            F3E &= 0xfffbffff;
        }
        if (P(int32_t, u, 0x5a)) {
            I(uint32_t *, 0x1934)[P(int32_t, u, 0x5a)] &= 0xffffff0f;
            P(int32_t, u, 0x5a) = 0;
        }
        dest_release(unit, 1);
        if (F >> 4 & 1) {
            uint16_t carrier = P(uint16_t, u, 0x17d);
            if (carrier && UNIT(carrier)) {
                if (SpriteHitDetect(P(void *, u, 4), P(void *, UNIT(P(uint16_t, u, 0x17d)), 4), 0, 0, 0) ||
                    P(int16_t, u, 0x113) <= 0) {
                    P(int16_t, u, 0x113) = 0;
                    if (F >> 16 & 1) {
                        fn_100bca4c(unit, hi(u, P(uint16_t, u, 0x17d)));
                        unit_kill(unit & 0xffff, 0x20, 1, 1);
                    } else {
                        unit_order(hi(u, P(uint16_t, u, 0x17d)), 0x48, 1, 0);
                        fn_100bd8e8(unit, hi(u, P(uint16_t, u, 0x17d)));
                    }
                }
            } else {
                F &= 0xffffffdf;
                unit_set_action(unit, 0x1e, 1, 0);
            }
            F &= 0xffffffef;
            F &= 0xfffeffff;
            P(uint8_t, u, 0x7d) = 1;
        }
        if (F >> 6 & 1) {
            if (P(uint16_t, u, 0x17d) && UNIT(P(uint16_t, u, 0x17d)))
                fn_100bdab0(unit);
            F &= 0xffffffbf;
        }
    }
    /* 0x100c37ba */
    if (P(int16_t, u, 0x113) <= 0 && P(int16_t, u, 0x119) > 0 && !P(uint8_t, u, 0x2a)) {
        F &= 0xfffffffb;
        F |= 0x4000;
    }
    if (F >> 14 & 1) {
        if (!I(int32_t, 0x175) || unit_is_local(unit)) {
            if (P(int16_t, u, 0x119) > 0) {
                int16_t w = P(int16_t, u, 0x11b) > 0 ? P(int16_t, u, 0x11b) : P(int16_t, u, 0x119);
                if (!P(uint32_t, VEC(w), 0) || P(uint32_t, VEC(w), 0) <= I(uint32_t, 0x17dd))
                    path_start(unit, 0, 0, 0);
            } else {
                F &= 0xffffbfff;
            }
        }
    }
    if (P(uint32_t, u, 0x149)) {
        if (P(uint32_t, u, 0x149) > I(uint32_t, 0x17dd))
            return;
        P(uint32_t, u, 0x149) = 0;
    }
    if (F >> 30 & 1) {
        if (P(uint8_t, u, 0x79) == P(uint8_t, u, 0x7a)) {
            int32_t t2c, t30;
            unit_order(unit,
                       path_turn(unit, (char)P(uint8_t, u, 0x147), P(int16_t, u, 0x113), P(uint8_t, u, 0x79),
                                 P(uint8_t, u, 0x7a), u + 0x7b, 0, &t2c, &t30),
                       1, 0);
            P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x79);
            F &= 0xbfffffff;
        } else {
            P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
            if (P(uint8_t, u, 0x79) == P(uint8_t, u, 0x7a))
                F &= 0xbfffffff;
        }
    }
    uint8_t was = P(uint8_t, u, 0x147); /* -0x18 */
    if (F >> 13 & 1) {
        if (((int(__cdecl *)(uint32_t, int))TYPEFN(P(int32_t, u, 0xc), 0x18))(unit, 1))
            F |= 0x1000000;
        F &= 0xffffdfff;
    }
    if ((P(uint8_t, u, 0x195) & 1) && P(uint8_t, u, 0x79) == P(uint8_t, u, 0x7a)) {
        P(uint8_t, u, 0x195) ^= 1;
        P(uint8_t, u, 0x7d) = 1;
        DebugStr((const char *)ORIG_PTR(0x10158a98), P(uint8_t, u, 0x195));
    }
    if (P(uint8_t, u, 0x7d) == 8 && P(uint8_t, u, 0x196) != 0x1a) {
        if (P(uint8_t, u, 0x147) == 0x26 || P(uint8_t, u, 0x148) == 0x26) {
            F &= 0xffff7fff;
            P(uint32_t, u, 0x1a7) = 0;
        }
        unit_action_done(unit);
        if (P(int32_t, u, 0x5a)) {
            I(uint32_t *, 0x1934)[P(int32_t, u, 0x5a)] &= 0xffffff0f;
            P(int32_t, u, 0x5a) = 0;
        }
    } else {
        P(uint8_t, u, 0x147) = P(uint8_t, u, 0x148);
    }
    /* 0x100c3be6: the action's animation record */
    P(uint8_t *, u, 0x163) = P(uint8_t *, u, 0x15f) + P(uint8_t, u, 0x147) * 0x2c;
    P(uint8_t, u, 0x156) = *P(uint8_t *, u, 0x163);
    P(uint8_t, u, 0x158) = P(uint8_t, u, 0x156);
    P(uint8_t, u, 0x155) = P(uint8_t *, u, 0x163)[1];
    P(uint8_t, u, 0x157) = 0;
    uint16_t cell_was = P(uint16_t, u, 0x145);
    P(uint16_t, u, 0x145) = P(uint16_t, P(uint8_t *, u, 0x163), 0xc + P(uint8_t, u, 0x157) * 2);
    if ((F >> 24 & 1) && !(F >> 30 & 1)) {
        int next;
        P(int16_t, UNIT(unit & 0xffff), 0x123) = (int16_t)path_plan(
            unit, P(int16_t, u, 0x52), P(int16_t, u, 0x54), P(uint8_t, u, 0x79), P(uint8_t, u, 0x7a),
            P(uint8_t, u, 0x147), P(int16_t, u, 0x113), P(uint8_t, u, 0x15a), P(uint8_t, u, 0x158),
            P(char *, u, 0x163), P(int16_t, u, 0x123), (int16_t *)(u + 0x127), &next);
        path_step(unit, next, (int16_t *)(u + 0x123), (int16_t *)(u + 0x127), (int16_t *)(u + 0x133),
                  (int16_t *)(u + 0x12b), (int16_t *)(u + 0x12f));
        F &= 0xfeffffff;
        if (P(int16_t, u, 0x123) <= 0) {
            F |= 0x2000;
            P(uint8_t, u, 0x158) = 0;
            P(uint8_t, u, 0x148) = P(uint8_t, u, 0x147);
            P(uint8_t, u, 0x147) = was;
            P(uint8_t, u, 0x14d) = 0xff;
            P(uint8_t, u, 0x14e) = 0xff;
            P(uint16_t, u, 0x145) = cell_was;
            return;
        }
    }
    if (P(int16_t, u, 0x123) && action_counts(P(uint8_t, u, 0x147))) {
        P(int16_t, u, 0x123)--;
        if (P(int16_t, u, 0x123) <= 0)
            F |= 0x2000;
    }
    if (!P(int16_t, u, 0x123) && action_counts(P(uint8_t, u, 0x147)))
        F |= 0x2000;
    switch (P(uint8_t, u, 0x147)) {
    case 0: {
        unit_set_flag_2a(u, 0);
        unit_order(unit, 0, 1, 0);
        int32_t kind = P(int32_t, P(uint8_t *, u, 0x32), 4);
        if (kind >= 0 && KIND_FN(kind, 0))
            KIND_FN(kind, 0)(u);
        if (P(uint8_t, u, 0x2d) == 4)
            P(uint8_t, u, 0x2d) = 2;
        if (P(uint8_t, u, 0x156) > 1)
            P(uint8_t, u, 0x2d) = 4;
        if (P(uint16_t, u, 0x9a) && !(F >> 31 & 1)) {
            if (!P(uint32_t, u, 0x1a7))
                unit_attack(unit);
            else
                P(uint32_t, u, 0x1a7)--;
        } else if (F >> 18 & 1) {
            if (!P(uint32_t, u, 0x1a7))
                unit_guard(unit);
            else
                P(uint32_t, u, 0x1a7)--;
        } else if (P(int16_t, u, 0x113) > 0 && !P(uint8_t, u, 0x148) && P(uint8_t, u, 0x7d) != 8) {
            int32_t t38, t3c;
            unit_order(unit,
                       path_turn(unit, (char)P(uint8_t, u, 0x147), P(int16_t, u, 0x113), P(uint8_t, u, 0x79),
                                 P(uint8_t, u, 0x7a), u + 0x7b, 0, &t38, &t3c),
                       1, 0);
            P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
            P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
            F &= 0xbfffffff;
        }
        break;
    }
    case 0x11:
    case 0x12:
    case 0x19:
    case 0x1a:
        patrol_turn(unit, u);
        break;
    case 0x1b:
        unit_order(unit, P(int16_t, u, 0x113) <= 0 ? 0 : 1, 1, 0);
        break;
    case 1:
        unit_order(unit, 5, 1, 0);
        unit_set_flag_2a(u, 1);
        F |= 0x1000000;
        break;
    case 2:
        unit_order(unit, 6, 1, 0);
        unit_set_flag_2a(u, 1);
        F |= 0x1000000;
        break;
    case 5:
        if (I(uint8_t, 0xeea) & 1)
            DebugStr((const char *)ORIG_PTR(0x10158ae4), unit & 0xffff);
        unit_order(unit, 6, 1, 0);
        break;
    case 6:
        if (I(uint8_t, 0xeea) & 1)
            DebugStr((const char *)ORIG_PTR(0x10158afc), unit & 0xffff);
        unit_order(unit, 5, 1, 0);
        break;
    case 0x13:
    case 0x14: {
        uint8_t *v = UNIT(unit & 0xffff);
        P(uint8_t, u, 0x7b) = TURN5E0(P(uint8_t, v, 0x7a), P(uint8_t, v, 0x79));
        d1c = TURN5A0(P(uint8_t, v, 0x7a), P(uint8_t, v, 0x79));
        if (d1c) {
            d4 = (int8_t)TURN5E0(P(uint8_t, v, 0x7a), P(uint8_t, v, 0x79));
            (void)d4;
            if (TURN620(P(uint8_t, v, 0x7a), P(uint8_t, v, 0x79))) {
                unit_order(unit, 0x13 + (d1c <= 0), 1, 0);
            } else if (P(int16_t, u, 0x113) > 0) {
                if (P(int16_t, u, 0x6c) == P(int16_t, u, 0x52) && P(int16_t, u, 0x6e) == P(int16_t, u, 0x54)) {
                    unit_order(unit, 0, 1, 0);
                    F |= 0x2000;
                } else {
                    unit_order(unit, 1, 1, 0);
                }
                unit_set_flag_2a(u, 1);
            } else {
                unit_order(unit, 0, 1, 0);
            }
        } else if (P(int16_t, u, 0x113) <= 0) {
            unit_order(unit, 0, 1, 0);
        }
        F |= 0x40000000;
        break;
    }
    case 0xf:
    case 0xd:
        turn_step(unit, u, 6);
        break;
    case 0x10:
    case 0xe:
        turn_step(unit, u, 5);
        break;
    case 0x1f:
        if (I(uint8_t, 0xeea) & 1)
            DebugStr((const char *)ORIG_PTR(0x10158b8c));
        unit_order(unit, 0, 1, 0);
        break;
    case 0x26:
        F &= 0xffff7fff;
        if (I(int32_t, 0x17f5) == 4)
            unit_voice(u, 7, 0, 0);
        unit_order(unit, 0, 1, 0);
        P(uint32_t, u, 0x1a7) = 0;
        break;
    case 0x27:
        if (I(int32_t, 0x17f5) == 4)
            unit_voice(u, 7, 0, 0);
        unit_effects_clear(unit);
        unit_order(unit, 0x4d, 1, 0);
        unit_dying(unit);
        break;
    case 0x20:
    case 0x21:
        F3E |= 0x400000;
        unit_order(unit, 0x4c, 1, 0);
        if (!(F3E >> 17 & 1))
            player_announce(P(uint8_t, u, 0x14), 0x2c, 1);
        break;
    case 0x15:
    case 0x17:
        unit_order(unit, 0x18, 1, 0);
        break;
    case 0x16:
    case 0x18:
        unit_order(unit, 0x17, 1, 0);
        break;
    case 0x41:
        unit_order(unit, 0x42, 1, 0);
        break;
    case 0x42:
        if (F3E >> 16 & 1) {
            unit_order(unit, 0x51, 1, 0);
            general_sound(0x1f, P(int16_t, u, 0x52), P(int16_t, u, 0x54), 0, 1);
        } else {
            unit_order(unit, 0x45, 1, 0);
            unit_set_d6f9(unit, 1);
            unit_buildup_set(hi(u, P(uint16_t, u, 0x1cb)), 2);
        }
        break;
    case 0x51:
        IsoAddObject(g_iso, 0x123, P(int16_t, u, 0x52) - 0x20, P(int16_t, u, 0x54) - 0x10, 0, 0, 2);
        status_text(0, 0);
        fn_10068c89(unit);
        unit_order(unit, 0x4d, 1, 0);
        break;
    case 0x43:
        F &= 0xffff7fff;
        if (!(F3E >> 16 & 1))
            IsoUpdateFootprint(g_iso, unit & 0xffff, 0, 0);
        if (!(F3E >> 16 & 1) && I(uint16_t, 0xd6f9 + P(uint8_t, u, 0x14) * 2) == (unit & 0xffff)) {
            if (UNIT(P(uint16_t, u, 0x9a)))
                P(uint16_t, UNIT(P(uint16_t, u, 0x9a)), 0x9a) = 0;
            P(uint16_t, u, 0x9a) = 0;
            player_replace_d6f9(unit & 0xffff, P(uint8_t, u, 0x14));
        }
        if (I(uint16_t, 0xd6cb + P(uint8_t, u, 0x14) * 2) == (unit & 0xffff))
            player_replace_d6cb(unit & 0xffff, P(uint8_t, u, 0x14));
        unit_order(unit, 0x44, 1, 0);
        break;
    case 0x44:
        if (F3E >> 16 & 1) {
            general_sound(0x20, P(int16_t, u, 0x52), P(int16_t, u, 0x54), 0, 1);
            unit_order(unit, 0x40, 1, 0);
        } else {
            unit_order(unit, 0, 1, 0);
            F &= 0xffffefff;
        }
        break;
    case 0x40:
        if (F3E >> 16 & 1) {
            F3E &= 0xfffeffff;
            unit_order(unit, 0x52, 1, 0);
        } else {
            unit_order(unit, 0, 1, 0);
            F &= 0xffffefff;
        }
        break;
    case 0x45:
        unit_set_flag_2a(u, 0);
        unit_order(unit, 0x45, 1, 0);
        if (P(uint8_t, u, 0x2d) > 1) {
            P(uint16_t, u, 0x66) = 0;
            P(uint8_t, u, 0x2d) = 2;
            IsoUpdateFootprint(g_iso, unit & 0xffff, 1, 0);
        }
        break;
    case 0x46:
        unit_order(unit, 0x47, 1, 0);
        break;
    case 0x47:
        unit_set_flag_2a(u, 0);
        unit_order(unit, 0x47, 1, 0);
        if (P(uint8_t, u, 0x2d) > 1)
            P(uint8_t, u, 0x2d) = 2;
        break;
    case 0x48:
        if (P(int32_t, u, 0xc) == 0x22 && (F3E >> 28 & 1)) {
            ai_event(P(uint8_t, u, 0x14), 4, unit & 0xffff, 0);
            fn_1007549f(unit & 0xffff, P(int16_t, u, 0x187), P(int16_t, u, 0x189), 0);
        } else {
            unit_order(unit, 0, 1, 0);
        }
        break;
    default:
        if (P(uint8_t, u, 0x147) >= 0x28 && P(uint8_t, u, 0x147) <= 0x3f) {
            unit_order(unit, 0, 1, 0);
            int32_t kind = P(int32_t, P(uint8_t *, u, 0x32), 4);
            if (kind >= 0 && KIND_FN(kind, 4))
                KIND_FN(kind, 4)(u);
        }
        break;
    }
    /* 0x100c4f3c: a queued follow-up action */
    if ((int8_t)P(uint8_t, u, 0x14d) != -1) {
        unit_order(unit, ((uint32_t)(uintptr_t)u & 0xffffff00) | P(uint8_t, u, 0x14d), 1, 0);
        if ((int8_t)P(uint8_t, u, 0x14e) != -1) {
            P(uint8_t, u, 0x14d) = P(uint8_t, u, 0x14e);
            P(uint8_t, u, 0x14e) = 0xff;
        } else {
            P(uint8_t, u, 0x14d) = 0xff;
        }
    }
    if (P(uint8_t, u, 0x147) && P(uint8_t, u, 0x147) != 0x45)
        P(uint8_t, u, 0x2d) = 4;
    goto L501e;

L4ffb:
    /* still in the middle of an action: its next frame */
    P(uint16_t, u, 0x145) = P(uint16_t, P(uint8_t *, u, 0x163), 0xc + P(uint8_t, u, 0x157) * 2);
L501e:
    if (P(int32_t, u, 0xc) >= 0x2c && P(int32_t, u, 0xc) <= 0x2f)
        fn_100c697c(unit);
    if (P(uint8_t, u, 0x147) == 0x52 || P(uint8_t, u, 0x147) == 0x51) {
        P(uint8_t, u, 0x158) = 0;
        return;
    }
    {
        /* one step of the action's movement */
        int32_t turning = 0;
        if (P(uint8_t, u, 0x147) >= 0xd && P(uint8_t, u, 0x147) <= 0x10) {
            turning = 1;
            d1c = TURN5A0(P(uint8_t, UNIT(unit & 0xffff), 0x7a), P(uint8_t, UNIT(unit & 0xffff), 0x79));
            if (d1c < 0)
                d1c = 0;
        }
        uint8_t dir = P(uint8_t, u, 0x79);
        const int8_t *anim = P(const int8_t *, u, 0x163);
        if (!turning) {
            uint32_t tx = P(uint8_t, UNIT(unit & 0xffff), 0x15a) ? 0x1015b590 : 0x1015b580;
            uint32_t ty = P(uint8_t, UNIT(unit & 0xffff), 0x15a) ? 0x1015b598 : 0x1015b588;
            STEP_DX = anim[G(int8_t, tx + dir) + 4] * G(int8_t, 0x1015b568 + dir);
            STEP_DY = anim[G(int8_t, ty + dir) + 4] * G(int8_t, 0x1015b570 + dir);
        } else {
            uint32_t at = dir * 24 + P(uint8_t, u, 0x157) * 2 + d1c * 12;
            STEP_DX = G(int8_t, 0x101589d8 + at);
            STEP_DY = G(int8_t, 0x101589d9 + at);
        }
        P(uint8_t, u, 0x157)++;
        if (P(uint8_t, u, 0x158))
            P(uint8_t, u, 0x158)--;
        P(int16_t, u, 0x52) = (int16_t)(P(int16_t, u, 0x52) + STEP_DX);
        P(int16_t, u, 0x54) = (int16_t)(P(int16_t, u, 0x54) + STEP_DY);
        if (STEP_DX || STEP_DY) {
            path_move((int16_t *)(u + 0x127), (int16_t *)(u + 0x133), (int16_t *)(u + 0x12b), (int16_t *)(u + 0x12f),
                      (int16_t *)(u + 0x52), (int16_t *)(u + 0x54), &STEP_DX, &STEP_DY);
            P(uint8_t, u, 0x15a) ^= 1;
        }
        P(int32_t, u, 0x13d) = STEP_DX;
        P(int32_t, u, 0x141) = STEP_DY;
        P(uint8_t, u, 0x159) = 0;
    }
    if (P(uint8_t, u, 0x147) < 0x26 || P(uint8_t, u, 0x147) > 0x48) {
        int32_t per = P(int32_t, P(uint8_t *, u, 0x32), 0x7c);
        P(uint16_t, u, 0x145) = (uint16_t)(per * P(uint8_t, u, 0x79) + P(uint16_t, u, 0x145));
        if (per * 8 - 1 < P(uint16_t, u, 0x145))
            P(uint16_t, u, 0x145) = (uint16_t)(P(uint16_t, u, 0x145) - per * 8);
    }
    if (P(int32_t, P(uint8_t *, u, 0x32), 0xb4) && !(F >> 12 & 1)) {
        /* a turret turns towards its target, or with the body */
        if (P(uint16_t, u, 0x17d) && UNIT(P(uint16_t, u, 0x17d)) && P(uint8_t, u, 0x196) != 0x13) {
            int32_t want = 0;
            if (!fn_100d9ead(unit, &want, 0)) {
                P(uint8_t, u, 0x7c) = (uint8_t)(fn_100d9fa9(P(uint8_t, u, 0x7c), want, 0) + P(uint8_t, u, 0x7c));
                if (P(uint8_t, u, 0x2d) == 1)
                    P(uint8_t, u, 0x2d) = 2;
            }
        } else {
            P(uint8_t, u, 0x7c) =
                (uint8_t)(fn_100d9fa9(P(uint8_t, u, 0x7c), P(uint16_t, u, 0x145), 0) + P(uint8_t, u, 0x7c));
            if (P(uint8_t, u, 0x2d) == 1)
                P(uint8_t, u, 0x2d) = 2;
        }
        P(uint8_t, u, 0x7c) = (uint8_t)(P(uint8_t, u, 0x7c) + 0x30);
        P(uint8_t, u, 0x7c) = (uint8_t)(P(uint8_t, u, 0x7c) % 0x30);
    }
    if ((STEP_DX || STEP_DY || P(int16_t, u, 0x1a1) == -1) && !(F >> 15 & 1) && !(F3E >> 16 & 1)) {
        unit_sector_update(unit, 1, 1);
        carried_follow(unit);
    }
    typedef void(__cdecl * MovedFn)(uint32_t, int);
    if (TYPEFN(P(int32_t, u, 0xc), 0x30) && !(F >> 15 & 1) && !(F3E >> 16 & 1))
        ((MovedFn)TYPEFN(P(int32_t, u, 0xc), 0x30))(unit, 1);
}
