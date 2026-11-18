/* The game logic, 0x1000a86b-0x1000c898: a unit's action, once per game tick. When the
 * current action's steps are used up, the next one is chosen (its animation, moving,
 * entering and leaving vehicles and buildings, turning, attacking); then the unit takes
 * one step of it. */
#include "iso_core.h"
#include "intfwave.h"
#include "sound.h"
#include "sector.h"
#include "combat.h"
#include "unit.h"
#include "action.h"

/* not rewritten yet (names ours, provisional) */
#define IsoDeleteUnit ORIG(int(__stdcall *)(uint8_t *, uint32_t, int), 0x100ab4aa)
#define unit_busy ORIG(int(__cdecl *)(uint32_t), 0x100e8836)
#define unit_near ORIG(int(__cdecl *)(uint32_t, int, int, int, uint32_t), 0x100d979f)
#define cell_claim ORIG(int(__cdecl *)(uint32_t, int), 0x100be4d2)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_board ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x10071e81)
#define unit_boarded ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100bd8e8)
#define unit_enter_building ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x100bca4c)
#define building_entered ORIG(void(__cdecl *)(uint8_t, int16_t), 0x10041277)
#define unit_remove_from_map unit_kill
#define score_unit_lost ORIG(void(__cdecl *)(uint32_t, uint8_t, uint8_t), 0x1007e870)
#define unit_leave_vehicle ORIG(void(__cdecl *)(uint32_t), 0x100bdab0)
#define unit_next_waypoint ORIG(int(__cdecl *)(uint32_t, int, int, int), 0x100b5897)
#define action_set ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009cc60)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_face ORIG(void(__cdecl *)(uint8_t *, char), 0x100a2200)
#define path_turn                                                                            \
    ORIG(uint32_t(__cdecl *)(uint32_t, char, int16_t, uint8_t, uint8_t, uint8_t *, int, void *, void *), 0x100e4c70)
#define path_plan                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int16_t, int16_t, uint8_t, uint8_t, uint8_t, int16_t, uint8_t, int,     \
                        char *, int, int16_t *, int *),                                                     \
         0x100e5c00)
#define path_step                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int, int16_t *, int16_t *, int16_t *, int16_t *, int16_t *), 0x100e6328)
#define path_move                                                                            \
    ORIG(int(__cdecl *)(int16_t *, int16_t *, int16_t *, int16_t *, int16_t *, int16_t *, int32_t *, int32_t *), \
         0x100e64f3)
#define unit_attack ORIG(int(__cdecl *)(uint32_t), 0x100e7b9b)
#define unit_guard ORIG(int(__cdecl *)(uint32_t), 0x100e80fa)
#define unit_fire_at ORIG(void(__cdecl *)(uint8_t *), 0x100dbdae)
#define unit_dying ORIG(void(__cdecl *)(uint32_t), 0x100e83f5)

#define U_FLAGS2 0x10e
#define U_TYPEINFO 0x32
#define U_CELL 0x56
#define U_CELL2 0x5a     /* a second cell it is moving into */
#define U_ACTION 0x147   /* byte */
#define U_NEXT 0x148     /* byte: the action asked for */
#define U_STEPS 0x123    /* short: steps left on the path */
#define U_FRAME 0x157    /* byte: step in the action */
#define U_FRAMES 0x158   /* byte: steps left in the action */
#define U_ANIM 0x163     /* the action's animation record (0x2c bytes) */
#define U_CARRIER 0x17d  /* short: the vehicle or building it is entering */
#define U_STATE 0x7d     /* byte */
#define ISO_TICK 0x17dd
#define g_step_x G(int32_t, 0x10118b20) /* this step's movement */
#define g_step_y G(int32_t, 0x10118b24)

/* the order record of a unit (I(0x1984), 0x12 bytes each): due tick, ... +0xe state */
#define ORDER(i) (I(uint8_t *, 0x1984) + (i) * 0x12)

/* the facing of a unit at rest: back to its stored direction (for some types) */
static void face_stored(uint8_t *u, uint32_t id, int second_too)
{
    if (P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x74) == 1 && P(uint8_t, u, 0x2a) == 1 && P(int8_t, u, 0x78) >= 0) {
        P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x78);
        if (second_too) {
            P(uint8_t, iso_unit((int)(id & 0xffff)), 0x7a) = P(uint8_t, u, 0x7b);
            P(uint8_t, u, 0x79) = P(uint8_t, iso_unit((int)(id & 0xffff)), 0x7a);
        } else {
            P(uint8_t, u, 0x7a) = P(uint8_t, u, 0x7b);
            P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7a);
        }
        P(uint8_t, u, 0x78) = 0xff;
    }
}

/* turning on the spot towards the path (actions 0, 0xb, 0x22, 0x4b when moving) */
static void turn_to_path(uint8_t *u, uint32_t id)
{
    uint8_t tmp[4];
    uint32_t tmp2;
    uint32_t anim = path_turn(id, P(char, u, U_ACTION), P(int16_t, u, 0x113), P(uint8_t, u, 0x79),
                              P(uint8_t, u, 0x7a), u + 0x7b, 0, tmp, &tmp2);
    unit_set_anim(id, anim, 1, 0); /* original: the whole register, not just the byte */
    P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
    P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
    P(uint32_t, u, U_FLAGS2) &= 0xbfffffff;
}

/* the cell's bits for the unit's place: taken (place at 10e bits 10-11, kept in 21-22) */
static void cell_take(uint8_t *u)
{
    uint32_t *c = &I(uint32_t *, 0x1934)[P(int32_t, u, U_CELL)];
    *c = ((*c >> 8 | 1u << (P(uint32_t, u, U_FLAGS2) >> 10 & 3)) & 0xf) << 8 | (*c & 0xfffff0ff);
}
static void cell2_free(uint8_t *u)
{
    if (!P(int32_t, u, U_CELL2))
        return;
    uint32_t *c = &I(uint32_t *, 0x1934)[P(int32_t, u, U_CELL2)];
    *c = (*c >> 4 & ~(1u << (P(uint32_t, u, U_FLAGS2) >> 0x1c & 3)) & 0xf) << 4 | (*c & 0xffffff0f);
    P(int32_t, u, U_CELL2) = 0;
    P(uint32_t, u, U_FLAGS2) &= 0xcfffffff;
}

/* 0x1000a86b */
void __cdecl unit_action_tick(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    /* ids passed on with the upper half of the unit's address, as the original's register held */
    uint32_t hi16 = (uint32_t)(uintptr_t)u & 0xffff0000;

    if (!P(uint8_t, u, U_FRAMES)) {
        if ((P(uint32_t, u, U_FLAGS) >> 0x16 & 1) || P(uint8_t, u, U_NEXT) == 'L') {
            IsoDeleteUnit(g_iso, id & 0xffff, ~(P(uint32_t, u, U_FLAGS) >> 0x11) & 1);
            return;
        }
        if (I(int, 0x175) && P(int16_t, u, 0x113) > 0 && P(int16_t, ORDER(P(int16_t, u, 0x113)), 0xe) == -2 &&
            P(uint8_t, u, 0x196) != 0x1a)
            return;
        if (unit_busy(id))
            return;
        if (P(uint8_t, u, U_ACTION) == 'P' && P(uint8_t, u, U_NEXT) == 'P')
            return;
        if ((P(uint32_t, u, U_FLAGS) >> 0x10 & 1) && P(uint8_t, u, U_NEXT) == 'E')
            return;
        if (P(uint32_t, u, U_FLAGS2) >> 0x1b & 1) {
            P(uint32_t, u, U_FLAGS2) &= 0xf7ffffff;
            typedef void(__cdecl * Arrived)(uint32_t, int, int);
            Arrived arrived = G(Arrived, 0x101545f8 + P(int32_t, u, U_ID_TYPE) * 0x44);
            if (!(P(uint16_t, u, 0x1cd) >> 1 & 1) && arrived) {
                int done = 0;
                if (P(int16_t, u, 0x119) > 0 && P(int16_t, u, 0x11b) > 0 &&
                    P(int16_t, ORDER(P(int16_t, u, 0x119)), 0xe) < 1 && P(int16_t, ORDER(P(int16_t, u, 0x11b)), 0xe) < 1)
                    done = 1;
                if (P(int16_t, u, 0x113) < 1 && done &&
                    unit_near(id, 0x20, P(int16_t, u, 0x70), P(int16_t, u, 0x72), 1)) {
                    arrived(id, (P(uint32_t, u, U_FLAGS) >> 0x12 & 1) ? 0x10 : 0xb, 1);
                    P(uint32_t, u, U_FLAGS) &= 0xfffbffff;
                }
            }
            if (!(P(uint32_t, u, U_FLAGS2) >> 7 & 1)) {
                cell_take(u);
                cell2_free(u);
                P(uint32_t, u, U_FLAGS2) = (P(uint32_t, u, U_FLAGS2) >> 10 & 3) << 0x15 | (P(uint32_t, u, U_FLAGS2) & 0xff9fffff);
                P(uint32_t, u, U_FLAGS2) |= 0x80;
                if (!(P(uint32_t, u, U_FLAGS2) >> 0x13 & 1))
                    cell_claim(id, 1);
            }
            if (P(uint32_t, u, U_FLAGS2) >> 4 & 1) {
                /* it has reached the vehicle or building it was entering */
                uint16_t carrier = P(uint16_t, u, U_CARRIER);
                if (!carrier || !iso_unit(carrier)) {
                    P(uint32_t, u, U_FLAGS2) &= 0xffffffdf;
                    unit_set_action(id, 0x1e, 1, 0);
                } else if (!(P(uint32_t, u, U_FLAGS2) >> 0x10 & 1)) {
                    P(uint8_t, iso_unit(carrier), 0x112) &= 0xfe;
                    unit_board(hi16 | carrier, id);
                    unit_boarded(id, hi16 | carrier);
                    P(uint32_t, u, U_FLAGS2) &= 0xffffffdf;
                } else if (!I(int, 0x175) || I(int, 0x148)) {
                    if (unit_enter_building(id, hi16 | carrier)) {
                        building_entered(P(uint8_t, u, U_PLAYER), (int16_t)carrier);
                        unit_remove_from_map(id & 0xffff, 0x20, 1, 1);
                        uint8_t *v = iso_unit((int)(id & 0xffff));
                        if (!(P(uint32_t, v, U_FLAGS) >> 0x1e & 1)) {
                            score_unit_lost(P(uint32_t, v, U_CLASS), P(uint8_t, v, U_PLAYER), P(uint8_t, v, U_PLAYER));
                            P(uint32_t, iso_unit((int)(id & 0xffff)), U_FLAGS) |= 0x40000000;
                        }
                    }
                }
                P(uint32_t, u, U_FLAGS2) &= 0xffffffef;
                P(uint32_t, u, U_FLAGS2) &= 0xfffeffff;
                P(uint8_t, u, U_STATE) = 1;
            }
        }
        if (P(uint32_t, u, U_FLAGS2) >> 6 & 1) {
            if (P(uint16_t, u, U_CARRIER) && iso_unit(P(uint16_t, u, U_CARRIER)))
                unit_leave_vehicle(id);
            P(uint32_t, u, U_FLAGS2) &= 0xffffffbf;
        }
        if (P(int16_t, u, 0x113) < 1 && P(int16_t, u, 0x119) > 0 && !P(uint8_t, u, 0x2a))
            P(uint32_t, u, U_FLAGS2) |= 0x4000;
        if ((P(uint32_t, u, U_FLAGS2) >> 0xe & 1) && (!I(int, 0x175) || unit_is_local(id))) {
            if (P(int16_t, u, 0x119) < 1) {
                P(uint32_t, u, U_FLAGS2) &= 0xffffbfff;
            } else {
                int16_t o = P(int16_t, u, 0x11b) < 1 ? P(int16_t, u, 0x119) : P(int16_t, u, 0x11b);
                if (!P(uint32_t, ORDER(o), 0) || P(uint32_t, ORDER(o), 0) <= I(uint32_t, ISO_TICK))
                    unit_next_waypoint(id, 0, 0, 0);
            }
        }
        if (P(uint32_t, u, 0x149)) { /* waiting */
            if (I(uint32_t, ISO_TICK) < P(uint32_t, u, 0x149))
                return;
            P(uint32_t, u, 0x149) = 0;
        }
        uint8_t was = P(uint8_t, u, U_ACTION);
        if (P(uint32_t, u, U_FLAGS2) >> 0xd & 1) {
            typedef int(__cdecl * Moving)(uint32_t, int);
            Moving moving = G(Moving, 0x101545d4 + P(int32_t, u, U_ID_TYPE) * 0x44);
            if (moving(id, 1)) {
                P(uint32_t, u, U_FLAGS2) |= 0x1000000;
                if (P(int16_t, u, 0x6c) == P(int16_t, u, U_X) && P(int16_t, u, 0x6e) == P(int16_t, u, U_Y)) {
                    P(uint32_t, u, U_FLAGS2) |= 0x2000;
                    P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
                    P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
                    return;
                }
            }
            P(uint32_t, u, U_FLAGS2) &= 0xffffdfff;
        }
        if (P(uint8_t, u, 0x195) & 1) {
            P(uint8_t, u, 0x195) &= 0xfe;
            if (P(int16_t, u, 0x113) < 1)
                P(uint32_t, u, U_FLAGS2) &= 0xfeffffff;
        }
        if (P(int16_t, u, 0x113) > 0 && (P(uint32_t, u, U_FLAGS2) >> 7 & 1)) {
            /* leaves its place on the cell */
            uint32_t *c = &I(uint32_t *, 0x1934)[P(int32_t, u, U_CELL)];
            *c = (*c >> 8 & ~(1u << (P(uint32_t, u, U_FLAGS2) >> 0x15 & 3)) & 0xf) << 8 | (*c & 0xfffff0ff);
            cell2_free(u);
            P(uint32_t, u, U_FLAGS2) = (P(uint32_t, u, U_FLAGS2) >> 10 & 3) << 0x15 | (P(uint32_t, u, U_FLAGS2) & 0xff9fffff);
            P(uint32_t, u, U_FLAGS2) &= 0xffffff7f;
        }
        if (P(uint8_t, u, U_STATE) == 8 && P(uint8_t, u, 0x196) != 0x1a) {
            if (P(uint8_t, u, U_ACTION) == '&' || P(uint8_t, u, U_NEXT) == '&') {
                P(uint32_t, u, U_FLAGS2) &= 0xffff7fff;
                P(uint32_t, u, 0x1a7) = 0;
            }
            if (P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x74) == 1) {
                if (P(int16_t, u, 0x6c) == P(int16_t, u, U_X) && P(int16_t, u, 0x6e) == P(int16_t, u, U_Y)) {
                    P(uint32_t, u, U_FLAGS2) |= 0x2000;
                    P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
                    P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
                    P(uint8_t, u, U_STATE) = 1;
                    P(uint8_t, u, 0x195) &= 0xfe;
                    if (P(uint8_t, u, U_SEL_STATE) == 1)
                        P(uint8_t, u, U_SEL_STATE) = 4;
                    return;
                }
                P(uint32_t, u, U_FLAGS2) &= 0xfff7ffff;
                if (P(uint8_t, u, U_ACTION) == 0xb) {
                    action_set(id, 0xc);
                    unit_set_anim(id, 1, 1, 0);
                } else if (P(uint8_t, u, U_ACTION) == 'K') {
                    action_set(id, 0x4a);
                    unit_set_anim(id, 1, 1, 0);
                } else if (!(P(uint32_t, u, U_FLAGS2) >> 3 & 1)) {
                    action_set(id, 1);
                    P(uint8_t, u, U_STATE) = 1;
                } else {
                    action_set(id, 8);
                    P(uint32_t, u, U_FLAGS2) |= 0x1000000;
                    P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
                    unit_face(u, 1);
                    P(uint8_t, u, U_STATE) = 1;
                }
            } else {
                action_set(id, 1);
                P(uint8_t, u, U_STATE) = 1;
            }
            if (P(int16_t, u, 0x113) > 0) {
                P(uint8_t, u, 0x7b) = P(uint8_t, u, 0x7a);
                P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7b);
            }
        } else {
            P(uint8_t, u, U_ACTION) = P(uint8_t, u, U_NEXT);
        }
        P(uint8_t, u, 0x79) = P(uint8_t, iso_unit((int)(id & 0xffff)), 0x7a);
        P(uint8_t *, u, U_ANIM) = P(uint8_t, u, U_ACTION) * 0x2c + P(uint8_t *, u, 0x15f);
        P(uint8_t, u, 0x156) = *P(uint8_t *, u, U_ANIM);
        P(uint8_t, u, U_FRAMES) = P(uint8_t, u, 0x156);
        P(uint8_t, u, 0x155) = P(uint8_t *, u, U_ANIM)[1];
        P(uint8_t, u, U_FRAME) = 0;
        uint16_t cell_was = P(uint16_t, u, 0x145);
        P(uint16_t, u, 0x145) = P(uint16_t, P(uint8_t *, u, U_ANIM), 0xc + P(uint8_t, u, U_FRAME) * 2);
        if ((P(uint32_t, u, U_FLAGS2) >> 0x18 & 1) && !(P(uint32_t, u, U_FLAGS2) >> 0x1e & 1)) {
            /* a new path step */
            int next;
            P(int16_t, u, U_STEPS) = (int16_t)path_plan(
                id, P(int16_t, u, U_X), P(int16_t, u, U_Y), P(uint8_t, u, 0x79), P(uint8_t, u, 0x7a),
                P(uint8_t, u, U_ACTION), P(int16_t, u, 0x113), P(uint8_t, u, 0x15a), P(uint8_t, u, U_FRAMES),
                P(char *, u, U_ANIM), P(int16_t, u, U_STEPS), (int16_t *)(u + 0x127), &next);
            path_step(id, next, (int16_t *)(u + U_STEPS), (int16_t *)(u + 0x127), (int16_t *)(u + 0x133),
                      (int16_t *)(u + 299), (int16_t *)(u + 0x12f));
            P(uint32_t, u, U_FLAGS2) &= 0xfeffffff;
            if (P(int16_t, u, U_STEPS) < 1) {
                P(uint32_t, u, U_FLAGS2) |= 0x2000;
                P(uint8_t, u, U_FRAMES) = 0;
                P(uint8_t, u, U_NEXT) = P(uint8_t, u, U_ACTION);
                P(uint8_t, u, U_ACTION) = was;
                P(uint8_t, u, 0x14d) = 0xff;
                P(uint8_t, u, 0x14e) = 0xff;
                P(uint16_t, u, 0x145) = cell_was;
                return;
            }
        }
        if (P(int16_t, u, U_STEPS) && action_counts(P(uint8_t, u, U_ACTION))) {
            P(int16_t, u, U_STEPS)--;
            if (P(int16_t, u, U_STEPS) < 1)
                P(uint32_t, u, U_FLAGS2) |= 0x2000;
        }
        if (!P(int16_t, u, U_STEPS) && action_counts(P(uint8_t, u, U_ACTION)))
            P(uint32_t, u, U_FLAGS2) |= 0x2000;

        typedef int(__cdecl * KindAction)(uint8_t *);
        uint8_t a = P(uint8_t, u, U_ACTION);
        switch (a) {
        case 0: { /* standing */
            face_stored(u, id, 1);
            unit_face(u, 0);
            unit_set_anim(id, 0, 1, 0);
            int32_t kind = P(int32_t, P(uint8_t *, u, U_TYPEINFO), 4);
            KindAction idle;
            if (kind >= 0 && (idle = G(KindAction, 0x10118d8c + kind * 12)))
                idle(u);
            if (P(uint8_t, u, U_SEL_STATE) > 1)
                P(uint8_t, u, U_SEL_STATE) = 2;
            if (!P(int16_t, u, 0x9a) || P(int32_t, u, U_FLAGS2) < 0) {
                if (!(P(uint32_t, u, U_FLAGS2) >> 0x12 & 1)) {
                    if (P(int16_t, u, 0x113) > 0 && !P(uint8_t, u, U_NEXT) && P(uint8_t, u, U_STATE) != 8)
                        turn_to_path(u, id);
                } else if (!P(int32_t, u, 0x1a7)) {
                    unit_guard(id);
                } else {
                    P(int32_t, u, 0x1a7)--;
                }
            } else if (!P(int32_t, u, 0x1a7)) {
                unit_attack(id);
            } else {
                P(int32_t, u, 0x1a7)--;
            }
            if ((P(uint32_t, u, U_FLAGS2) >> 4 & 1) || (P(uint32_t, u, U_FLAGS2) >> 6 & 1))
                P(uint32_t, u, U_FLAGS2) |= 0x8000000;
            break;
        }
        case 1:
            unit_set_anim(id, 5, 1, 0);
            P(uint32_t, u, U_FLAGS2) |= 0x1000000;
            unit_face(u, 1);
            break;
        case 2:
            unit_set_anim(id, 6, 1, 0);
            P(uint32_t, u, U_FLAGS2) |= 0x1000000;
            unit_face(u, 1);
            break;
        case 5:
            unit_set_anim(id, 6, 1, 0);
            break;
        case 6:
            unit_set_anim(id, 5, 1, 0);
            break;
        case 7:
            P(uint32_t, u, U_FLAGS2) |= 8;
            /* fall through */
        case 0x1d:
            unit_set_anim(id, 0x22, 1, 0);
            break;
        case 8:
            unit_set_anim(id, 8, 1, 0);
            break;
        case 9:
        case 0xc:
        case 0x11:
        case 0x12:
            P(uint32_t, u, U_FLAGS2) &= 0xfffdffff;
            P(uint32_t, u, U_FLAGS2) &= 0xfffffff7;
            unit_set_anim(id, P(int16_t, u, 0x113) < 1 ? 0 : 1, 1, 0);
            break;
        case 10:
            P(uint32_t, u, U_FLAGS2) |= 0x20000;
            /* fall through */
        case 0x1c:
            unit_set_anim(id, 0xb, 1, 0);
            break;
        case 0xb:
        case 0x22:
        case 0x4b: {
            uint32_t anim = a == 0xb ? 0xb : a == 0x22 ? 0x22 : 0x4b;
            face_stored(u, id, a == 0x22);
            unit_face(u, 0);
            unit_set_anim(id, anim, 1, 0);
            if (P(uint8_t, u, U_SEL_STATE) == 4)
                P(uint8_t, u, U_SEL_STATE) = 2;
            if (P(int16_t, u, 0x113) > 0 && P(uint8_t, u, U_NEXT) == a && P(uint8_t, u, U_STATE) != 8)
                turn_to_path(u, id);
            break;
        }
        case 0x15:
        case 0x17:
            unit_set_anim(id, 0x18, 1, 0);
            break;
        case 0x16:
        case 0x18:
            unit_set_anim(id, 0x17, 1, 0);
            break;
        case 0x19:
        case 0x1a:
        case 0x1f:
            unit_set_anim(id, 0, 1, 0);
            break;
        case 0x1b:
            unit_set_anim(id, 0x4b, 1, 0);
            break;
        case 0x20:
        case 0x21:
            P(uint32_t, u, U_FLAGS) |= 0x400000;
            unit_set_anim(id, 0x4c, 1, 0);
            if (!(P(uint32_t, u, U_FLAGS) >> 0x11 & 1))
                player_announce(P(uint8_t, u, U_PLAYER), 0x2c, 1);
            break;
        case 0x26: /* built */
            P(uint32_t, u, U_FLAGS2) &= 0xffff7fff;
            if (I(int, 0x17f5) == 4)
                unit_voice(u, 7, 0, 0);
            unit_set_anim(id, 0, 1, 0);
            P(uint32_t, u, 0x1a7) = 0;
            break;
        case 0x27:
            if (I(int, 0x17f5) == 4)
                unit_voice(u, 7, 0, 0);
            unit_set_anim(id, 0x4d, 1, 0);
            unit_dying(id);
            break;
        case 0x49:
            P(uint32_t, u, U_FLAGS2) |= 0x800000;
            unit_set_anim(id, 0x4b, 1, 0);
            break;
        case 0x4a:
            P(uint32_t, u, U_FLAGS2) &= 0xff7fffff;
            unit_set_anim(id, P(int16_t, u, 0x113) < 1 ? 0 : 1, 1, 0);
            break;
        case 0x50:
            unit_set_anim(id, 0x50, 1, 0);
            break;
        default:
            if (a > 0x27 && a < 0x40) { /* the unit kind's own actions */
                int32_t kind = P(int32_t, P(uint8_t *, u, U_TYPEINFO), 4);
                KindAction act;
                if (kind >= 0 && (act = G(KindAction, 0x10118d8c + kind * 12)) && !act(u))
                    unit_set_anim(id, 0, 1, 0);
                unit_fire_at(u);
            }
            break;
        }
        if (P(int8_t, u, 0x14d) != -1) {
            /* original: with the upper bytes of the unit's address */
            unit_set_anim(id, ((uint32_t)(uintptr_t)u & 0xffffff00) | P(uint8_t, u, 0x14d), 1, 0);
            if (P(int8_t, u, 0x14e) == -1) {
                P(uint8_t, u, 0x14d) = 0xff;
            } else {
                P(uint8_t, u, 0x14d) = P(uint8_t, u, 0x14e);
                P(uint8_t, u, 0x14e) = 0xff;
            }
        }
        a = P(uint8_t, u, U_ACTION);
        if (a != 0 && a != '"' && a != 0xb && a != 'K')
            P(uint8_t, u, U_SEL_STATE) = 4;
    } else {
        P(uint16_t, u, 0x145) = P(uint16_t, P(uint8_t *, u, U_ANIM), 0xc + P(uint8_t, u, U_FRAME) * 2);
    }

    /* one step of the action */
    uint8_t d = P(uint8_t, u, 0x79);
    const int8_t *anim = P(int8_t *, u, U_ANIM);
    if (!P(uint8_t, u, 0x15a)) {
        g_step_x = anim[G(int8_t, 0x1015b580 + d) + 4] * G(int8_t, 0x1015b568 + d);
        g_step_y = anim[G(int8_t, 0x1015b588 + d) + 4] * G(int8_t, 0x1015b570 + d);
    } else {
        g_step_x = anim[G(int8_t, 0x1015b590 + d) + 4] * G(int8_t, 0x1015b568 + d);
        g_step_y = anim[G(int8_t, 0x1015b598 + d) + 4] * G(int8_t, 0x1015b570 + d);
    }
    P(uint8_t, u, U_FRAME)++;
    if (P(uint8_t, u, U_FRAMES))
        P(uint8_t, u, U_FRAMES)--;
    P(int16_t, u, U_X) = (int16_t)(P(int16_t, u, U_X) + (int16_t)g_step_x);
    P(int16_t, u, U_Y) = (int16_t)(P(int16_t, u, U_Y) + (int16_t)g_step_y);
    if (g_step_x || g_step_y) {
        path_move((int16_t *)(u + 0x127), (int16_t *)(u + 0x133), (int16_t *)(u + 299), (int16_t *)(u + 0x12f),
                  (int16_t *)(u + U_X), (int16_t *)(u + U_Y), &g_step_x, &g_step_y);
        P(uint8_t, u, 0x15a) ^= 1;
    }
    P(int32_t, u, 0x13d) = g_step_x;
    P(int32_t, u, 0x141) = g_step_y;
    uint8_t act = P(uint8_t, u, U_ACTION);
    if (act < 0x26 || act > 0x48) {
        d = P(uint8_t, u, 0x79);
        if (d < 3 || d > 5) {
            if (P(uint8_t, u, 0x159))
                P(uint32_t, u, U_FLAGS2) |= 0x100;
            P(uint8_t, u, 0x159) = 0;
            P(uint16_t, u, 0x145) = (uint16_t)((int16_t)P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x7c) * (uint16_t)d +
                                               P(int16_t, u, 0x145));
        } else {
            if (!P(uint8_t, u, 0x159))
                P(uint32_t, u, U_FLAGS2) |= 0x100;
            P(uint8_t, u, 0x159) = 1;
            P(int16_t, u, 0x145) = (int16_t)(G(int8_t, 0x1015b578 + d) * (int16_t)P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x7c) +
                                             P(int16_t, u, 0x145));
        }
    }
    if ((g_step_x || g_step_y || P(int16_t, u, 0x1a1) == -1) && !(P(uint32_t, u, U_FLAGS2) >> 0xf & 1))
        unit_sector_update(id, 1, 1);
    typedef void(__cdecl * PlacedHandler)(uint32_t, int);
    PlacedHandler moved = G(PlacedHandler, 0x101545ec + P(int32_t, u, U_ID_TYPE) * 0x44);
    if (moved && !(P(uint32_t, u, U_FLAGS2) >> 0xf & 1))
        moved(id, 1);
}
