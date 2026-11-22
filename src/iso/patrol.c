/* The game logic, 0x100b7a66-0x100b95dd: a unit's patrol order (the waypoints are vectors,
 * iso+0x1984, 0x12 bytes each: +4/+6 the point, +8 a count, +9 a direction, +0xa flags,
 * +0xe the next; a unit's patrol: +0x113 its first point, +0x121 the current one, +0x115 a
 * path kept for later; +0x119/+0x11b the path's first and current waypoint).
 *
 * The original reads two of its locals without having set them: the direction that
 * 0x100e663c is given to fill in (without a path it writes back an unset stack byte) and, when the patrol was already set up,
 * the step's time. Both are whatever earlier calls left on the stack; the entry stub below
 * takes them from the same stack slots, so the game behaves as it did. */
#include "iso_core.h"
#include "sector.h"
#include "patrol.h"

#define VEC(k) (I(uint8_t *, 0x1984) + (k) * 0x12)
#define FL(k) (P(uint16_t, VEC(k), 0xa))
#define F (P(uint32_t, u, 0x10e))
#define CUR (P(int16_t, u, 0x11b))
#define FIRST (P(int16_t, u, 0x119))
#define TURN(a, b) (*((const int8_t *)ORIG_PTR(0x1015b620) + (a) * 8 + (b)))
#define cell_lower ORIG(void(__cdecl *)(int32_t, int), 0x10067214)
#define vector_new ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100e98de)
#define vector_free ORIG(void(__cdecl *)(uint32_t), 0x100e9bbc)
#define vector_link ORIG(void(__cdecl *)(int32_t, int32_t, int), 0x100d4450)
#define net_delay ORIG(int(__cdecl *)(void), 0x1003f390)
#define direction_between ORIG(int(__cdecl *)(int16_t, int16_t, int16_t, int16_t), 0x1009188a)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define path_turn                                                                            \
    ORIG(uint32_t(__cdecl *)(uint32_t, char, int16_t, uint8_t, uint8_t, uint8_t *, int, void *, void *), 0x100e4c70)
#define path_plan                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int16_t, int16_t, uint8_t, uint8_t, uint8_t, int16_t, uint8_t, int,     \
                        char *, int, int16_t *, int *),                                                     \
         0x100e5c00)
#define path_step                                                                            \
    ORIG(int(__cdecl *)(uint32_t, int, int16_t *, int16_t *, int16_t *, int16_t *, int16_t *), 0x100e6328)
#define patrol_next                                                                          \
    ORIG(int32_t(__cdecl *)(uint32_t, int16_t *, uint8_t *, int32_t, int16_t *, int32_t *, int32_t *), 0x100e663c)
#define path_stop ORIG(int(__cdecl *)(uint32_t, int), 0x100d4947)
#define unit_path_set ORIG(void(__cdecl *)(uint32_t, int32_t, uint32_t, int), 0x100d559a)
#define unit_path_join ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, int), 0x100d4bfa)
#define path_append ORIG(void(__cdecl *)(uint32_t, int32_t), 0x100d4e05)
#define path_after ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t), 0x100d4f51)
#define path_insert ORIG(int32_t(__cdecl *)(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t), 0x100b54c7)
#define fn_100be084 ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, int), 0x100be084)
#define net_send_patrol_step ORIG(void(__cdecl *)(uint32_t, int32_t, int32_t, uint32_t), 0x10032cd4)

/* the two stack slots, as the entry stub found them: ebp-0x38 and ebp-0x54 of the original */
uint32_t patrol_stale[2];

__asm__(".text\n"
        ".globl _patrol_order\n"
        "_patrol_order:\n"
        "\tmovl -0x3c(%esp), %eax\n"
        "\tmovl %eax, _patrol_stale\n"
        "\tmovl -0x58(%esp), %eax\n"
        "\tmovl %eax, _patrol_stale+4\n"
        "\tjmp _patrol_order_c\n");

int32_t __cdecl patrol_order_c(uint32_t unit, uint32_t way, int32_t start, int32_t check, int32_t join, int32_t keep,
                               int32_t forced, uint32_t flag);

/* the player byte, with the record pointer's upper half above it as the original passes it */
static uint32_t owner_arg(const uint8_t *u)
{
    return ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint8_t, u, 0x14);
}

/* copies flag bit 1 of waypoint `from` to waypoint `to` */
static void copy_bit1(int32_t to, int32_t from)
{
    FL(to) = (uint16_t)((FL(from) & 2) | (FL(to) & 0xfffd));
}

/* a new one-point waypoint at waypoint `at`'s point, marked bit 6, with flag bit 8 `b8` */
static int16_t waypoint_at(uint8_t *u, int32_t at, uint16_t b8)
{
    int16_t nv = (int16_t)vector_new(owner_arg(u), P(uint32_t, u, 0xc));
    if (nv > 0) {
        P(uint8_t, VEC(nv), 8)++;
        P(int16_t, VEC(nv), 4) = P(int16_t, VEC(at), 4);
        P(int16_t, VEC(nv), 6) = P(int16_t, VEC(at), 6);
        FL(nv) |= 0x40;
        FL(nv) = (uint16_t)((FL(nv) & 0xfeff) | (b8 & 1) << 8);
    }
    return nv;
}

/* the unit stops its patrol-way state (+0x1cd/+0x1cf) and heads for waypoint `k` */
static void head_for(uint8_t *u, int32_t k, int clear)
{
    P(int16_t, u, 0x70) = P(int16_t, VEC(k), 4);
    P(int16_t, u, 0x72) = P(int16_t, VEC(k), 6);
    if (clear) {
        P(uint16_t, u, 0x1cd) |= 0x10;
        P(uint16_t, u, 0x1cd) &= 0xfff7;
        P(uint32_t, u, 0x1cf) = 0;
        P(uint16_t, u, 0x1cd) &= 0xfffd;
        P(uint16_t, u, 0x1cd) &= 0xfffe;
    }
}

/* 0x100b7a66 (entered through the stub): gives unit `unit` its patrol order. A unit on
 * patrol (+0x113) whose patrol is not set up yet (+0x10e bit 26) takes its next patrol
 * step (0x100e663c: where, how long, whether it only turns) and is set up. Then `way`, a
 * waypoint, goes into its path: with `start` as a new patrol from where it is; otherwise
 * joined (`join`), after the current waypoint or at the path's start. With `check` (and no
 * `start`) only a unit that is set up starts again from its current point. In a network
 * game only the local player's units (or `forced`); the step is sent to the others. 1, or
 * 0 when the unit takes no patrol order */
int32_t __cdecl patrol_order_c(uint32_t unit, uint32_t way, int32_t start, int32_t check, int32_t join, int32_t keep,
                               int32_t forced, uint32_t flag)
{
    int32_t step_time = (int32_t)patrol_stale[0];
    uint8_t dir = (uint8_t)patrol_stale[1];
    int16_t prev;
    int32_t at = -1, current = 0, until = 0, turn_only = 0, turn_done = 0, sent = 0;

    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    if (F >> 20 & 1)
        return 0;
    if (P(int16_t, u, 0x113) <= 0 && FIRST <= 0 && CUR <= 0)
        return 0;
    if (I(int32_t, 0x175) && !unit_is_local(unit) && !forced)
        return 1;
    if (check && (F >> 26 & 1))
        return 0;
    int32_t set_up = F >> 26 & 1;

    if (P(int16_t, u, 0x113) > 0 && !(F >> 26 & 1)) {
        /* drop the path kept for later */
        if (P(int16_t, u, 0x115) > 0) {
            int16_t k = P(int16_t, u, 0x115);
            while (k > 0) {
                if (I(int32_t, 0xe113)) {
                    int vx = P(int16_t, VEC(k), 4), vy = P(int16_t, VEC(k), 6), sx, sy;
                    int32_t cell = IsoVTilePos(g_iso, &vx, &vy, &sx, &sy);
                    if (!(FL(k) >> 9 & 1))
                        cell_lower(cell, 2);
                }
                prev = k;
                k = P(int16_t, VEC(prev), 0xe);
                /* the original passes the vector with stack leftovers above it */
                vector_free((uint16_t)prev);
            }
            P(int16_t, u, 0x115) = 0;
        }
        uint8_t *type = P(uint8_t *, u, 0x32);
        if (P(uint32_t, type, 0xc8) > (uint32_t)net_delay())
            step_time = P(int32_t, P(uint8_t *, u, 0x32), 0xc8);
        else
            step_time = net_delay();
        int16_t pt[2];
        int16_t steps;
        int32_t cell;
        until = patrol_next(unit, pt, &dir, step_time, &steps, &turn_only, &cell);
        at = P(int16_t, u, 0x113);
        if (P(int16_t, u, 0x121) == at)
            current = at;
        prev = -1;
        for (int32_t i = 0; i < steps; i++) {
            prev = (int16_t)at;
            at = P(int16_t, VEC(at), 0xe);
            if (P(int16_t, u, 0x121) == at)
                current = at;
        }
        if (current <= 0)
            P(int16_t, u, 0x121) = -1;

        if (step_time <= until && !(FL(at) >> 5 & 1)) {
            if (turn_only) {
                if (!TURN(dir, P(uint8_t, u, 0x79)) && TURN(P(uint8_t, u, 0x7a), P(uint8_t, u, 0x79)))
                    unit_set_anim(unit, 1, 1, 0);
                if (TURN(dir, P(uint8_t, u, 0x79)) && !TURN(P(uint8_t, u, 0x7a), P(uint8_t, u, 0x79))) {
                    uint8_t t78;
                    int32_t t74, t70;
                    unit_set_anim(unit,
                                  path_turn(unit, (char)P(uint8_t, u, 0x147), P(int16_t, u, 0x113), P(uint8_t, u, 0x79),
                                            P(uint8_t, u, 0x7a), &t78, 1, &t74, &t70),
                                  1, 0);
                }
                P(uint8_t, u, 0x7a) = dir;
                P(uint8_t, VEC(at), 9) = dir;
                F |= 0x1000000;
            } else if (!steps) {
                P(int16_t, u, 0x6c) = pt[0];
                P(int16_t, u, 0x6e) = pt[1];
                uint32_t *occ = I(uint32_t *, 0x1934);
                if (P(int32_t, P(uint8_t *, u, 0x32), 0xb0)) {
                    P(uint8_t, u, 0x7b) = (uint8_t)direction_between(P(int16_t, u, 0x52), P(int16_t, u, 0x54), pt[0], pt[1]);
                    P(uint8_t, u, 0x7a) = P(uint8_t, u, 0x7b);
                    P(uint8_t, u, 0x79) = P(uint8_t, u, 0x7a);
                    uint32_t bit = 1u << (F >> 28 & 3);
                    if (P(int32_t, u, 0x5a)) {
                        uint32_t e = occ[P(int32_t, u, 0x5a)];
                        occ[P(int32_t, u, 0x5a)] = ((e >> 4 & ~bit & 0xf) << 4) | (e & 0xffffff0f);
                    }
                    if (cell >= 0) {
                        uint32_t e = occ[cell];
                        occ[cell] = (((e >> 4 | bit) & 0xf) << 4) | (e & 0xffffff0f);
                        P(int32_t, u, 0x5a) = cell;
                    }
                } else {
                    if (P(int32_t, u, 0x5a))
                        occ[P(int32_t, u, 0x5a)] &= 0xffffff0f;
                    if (cell >= 0) {
                        occ[cell] |= 0xf0;
                        P(int32_t, u, 0x5a) = cell;
                    }
                }
                if (P(uint8_t, u, 0x79) != dir)
                    F |= 0x1000000;
            }
            if (P(uint8_t, u, 0x79) == P(uint8_t, u, 0x7a)) {
                P(uint8_t, u, 0x195) &= 0xfe;
                P(uint8_t, u, 0x7d) = 1;
            }
            if (I(int32_t, 0xe113) && at > 0) {
                int vx = P(int16_t, VEC(at), 4), vy = P(int16_t, VEC(at), 6), sx, sy;
                int32_t c = IsoVTilePos(g_iso, &vx, &vy, &sx, &sy);
                if (!(FL(at) >> 9 & 1))
                    cell_lower(c, 2);
            }
            /* the patrol point moves to the step's end; the old point is kept for later */
            int16_t old_x = P(int16_t, VEC(at), 4), old_y = P(int16_t, VEC(at), 6);
            uint8_t old_dir = P(uint8_t, VEC(at), 9);
            P(int16_t, VEC(at), 4) = pt[0];
            P(int16_t, VEC(at), 6) = pt[1];
            if (!P(int32_t, P(uint8_t *, u, 0x32), 0xb0) && P(uint8_t, VEC(at), 9) != dir) {
                FL(at) |= 0x10;
                P(uint8_t, VEC(at), 9) = dir;
            }
            FL(at) |= 0x20;
            if (!steps && !turn_only && P(uint8_t, u, 0x79) == dir)
                turn_done = 1;
            if (at > 0) {
                if (!keep) {
                    path_stop(unit, at);
                } else {
                    int16_t nv = (int16_t)vector_new(owner_arg(u), P(uint32_t, u, 0xc));
                    if (nv > 0) {
                        P(uint8_t, VEC(nv), 8)++;
                        P(int16_t, VEC(nv), 4) = old_x;
                        P(int16_t, VEC(nv), 6) = old_y;
                        P(uint8_t, VEC(nv), 9) = old_dir;
                        vector_link(nv, P(int16_t, VEC(at), 0xe), 1);
                        P(int16_t, u, 0x115) = nv;
                    }
                }
                vector_link(at, -3, 0);
            }
            sent = 1;
        }
        F |= 0x4000000;
    } else {
        if (check) {
            if ((F >> 26 & 1) && start && P(int16_t, VEC(FIRST), 0xe) > 0)
                unit_path_set(unit, FIRST, flag, 0);
            if ((int16_t)way <= 0)
                head_for(u, FIRST, 1);
            return 0;
        }
        turn_only = 0;
    }

    if (turn_done) {
        int32_t next;
        P(int16_t, u, 0x123) = (int16_t)path_plan(unit, P(int16_t, u, 0x52), P(int16_t, u, 0x54), P(uint8_t, u, 0x79),
                                                  P(uint8_t, u, 0x7a), P(uint8_t, u, 0x147), P(int16_t, u, 0x113),
                                                  P(uint8_t, u, 0x15a), P(uint8_t, u, 0x158), P(char *, u, 0x163),
                                                  P(int16_t, u, 0x123), (int16_t *)(u + 0x127), &next);
        if (until < next)
            until = next;
        path_step(unit, next, (int16_t *)(u + 0x123), (int16_t *)(u + 0x127), (int16_t *)(u + 0x133),
                  (int16_t *)(u + 0x12b), (int16_t *)(u + 0x12f));
        if (P(int16_t, u, 0x123) <= 0) {
            F |= 0x2000;
        } else if (action_counts(P(uint8_t, u, 0x147))) {
            P(int16_t, u, 0x123)--;
            if (!P(int16_t, u, 0x123))
                F |= 0x2000;
        }
    }

    int16_t w = (int16_t)way;
    if (w > 0)
        FL(w) &= 0xffbf;
    if (check && set_up && CUR <= 0)
        goto done;
    if (at <= 0) {
        if (CUR > 0)
            at = CUR;
        else if (FIRST > 0)
            at = FIRST;
    }

    if (start) {
        /* a new patrol from the unit's point */
        uint16_t b8 = CUR > 0 ? FL(CUR) >> 8 & 1 : 0;
        unit_path_set(unit, -1, flag, 0);
        int16_t nv = waypoint_at(u, at, b8);
        if (nv > 0) {
            path_append(unit, nv);
            CUR = nv;
            FIRST = CUR;
        }
        if (w > 0) {
            int32_t r = path_insert(unit, P(int16_t, VEC(at), 4), P(int16_t, VEC(at), 6), CUR, w, 0, 0);
            if (r > 0) {
                vector_free(way);
                way = (way & 0xffff0000) | (uint16_t)r;
            } else if (r < 0) {
                path_append(unit, w);
                head_for(u, w, 0);
            }
        } else {
            head_for(u, FIRST, 1);
        }
        w = (int16_t)way;
        if (step_time <= until) {
            if (w > 0) {
                fn_100be084(unit, P(uint16_t, VEC(w), 4), P(uint16_t, VEC(w), 6), 0);
                F |= 0x80000;
            } else {
                fn_100be084(unit, P(uint16_t, VEC(at), 4), P(uint16_t, VEC(at), 6), 0);
            }
        }
        goto done;
    }

    if (!check) {
        if (step_time <= until && !set_up) {
            uint16_t b8 = FL(w) >> 8 & 1;
            int16_t nv = waypoint_at(u, at, b8);
            if (FIRST == 0) {
                FIRST = nv;
            } else if (CUR > 0) {
                if (FL(CUR) >> 6 & 1)
                    path_after(unit, CUR, nv);
                else
                    unit_path_join(unit, CUR, nv, 1);
            } else {
                unit_path_join(unit, FIRST, nv, 1);
            }
            CUR = nv;
            fn_100be084(unit, P(uint16_t, VEC(w), 4), P(uint16_t, VEC(w), 6), 0);
            F |= 0x80000;
        }
        if (join) {
            unit_path_join(unit, CUR, w, 0);
        } else if (CUR > 0) {
            if (P(int16_t, VEC(CUR), 0xe) > 0) {
                copy_bit1(w, CUR);
                int32_t r = path_insert(unit, P(int16_t, VEC(at), 4), P(int16_t, VEC(at), 6), P(int16_t, VEC(CUR), 0xe),
                                        w, 1, 0);
                if (r > 0) {
                    vector_free(way);
                    way = (way & 0xffff0000) | (uint16_t)r;
                } else if (r < 0) {
                    path_after(unit, P(int16_t, VEC(CUR), 0xe), w);
                }
            } else {
                int32_t r = path_insert(unit, P(int16_t, VEC(at), 4), P(int16_t, VEC(at), 6), CUR, w, 0, 0);
                if (r > 0) {
                    vector_free(way);
                    way = (way & 0xffff0000) | (uint16_t)r;
                } else if (r < 0) {
                    path_append(unit, w);
                }
            }
        } else {
            int32_t r = path_insert(unit, P(int16_t, VEC(at), 4), P(int16_t, VEC(at), 6), FIRST, w, 1, 0);
            if (r > 0) {
                int16_t k = (int16_t)r;
                while (FL(k) & 1) {
                    copy_bit1(k, FIRST);
                    k = P(int16_t, VEC(k), 0xe);
                }
                copy_bit1(k, FIRST);
                vector_free(way);
                way = (way & 0xffff0000) | (uint16_t)r;
            } else if (r < 0) {
                copy_bit1(w, FIRST);
                path_after(unit, P(int16_t, VEC(FIRST), 0xe), w);
            }
        }
        w = (int16_t)way;
        head_for(u, w, 0);
        goto done;
    }

    /* `check` on a unit being set up: it starts again from its current point */
    if (step_time <= until && !set_up) {
        uint16_t b8 = CUR > 0 ? FL(CUR) >> 8 & 1 : 0;
        int16_t nv = waypoint_at(u, at, b8);
        if (FIRST <= 0) {
            CUR = nv;
            FIRST = CUR;
        } else if (CUR > 0) {
            unit_path_join(unit, CUR, nv, 1);
            CUR = nv;
        } else {
            unit_path_join(unit, FIRST, nv, 1);
            CUR = nv;
        }
        if (w > 0) {
            vector_free(way);
            way = way | 0xffff;
        }
        fn_100be084(unit, P(uint16_t, VEC(at), 4), P(uint16_t, VEC(at), 6), 0);
    }

done:
    if (I(int32_t, 0x175) && step_time <= until && sent)
        net_send_patrol_step(unit & 0xffff, at, turn_only, I(uint32_t, 0x17dd) + step_time);
    return 1;
}
