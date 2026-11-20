/* The game logic, 0x1009cc60-0x1009cf5b: a unit's order (+0x148, a character code; +0x147 a
 * lock: 'L' and ' ' block new orders, '!' lets only 'L' through) and its state (+0x196). */
#include "iso_core.h"
#include "sector.h"
#include "netflags.h"
#include "unitorder.h"

typedef void(__cdecl *order_fn)(uint32_t unit, uint32_t order, uint32_t arg, uint32_t flag);
#define ORDER_FN(o) G(order_fn, 0x10136610 + (o) * 8)
typedef void(__cdecl *leave_fn)(uint8_t *u);
#define LEAVE_FN(k) G(leave_fn, 0x10118d90 + (k) * 12)

static uint8_t *unit_ptr(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id & 0xffff]; }

/* 0x1009cc60: sets the unit's order lock */
void __cdecl unit_set_order_lock(uint32_t unit, uint8_t lock) { P(uint8_t, unit_ptr(unit), 0x147) = lock; }

/* 0x1009cc91: gives the unit an order unless it is busy (+0x3e bit 22) or locked. In a
 * network game, for the local player's units, when the order has a handler and `send` is
 * set: the order's time (+0x149) is set if none yet (now + the network delay + `delay`), and
 * a changed order is passed to its handler */
void __cdecl unit_order(uint32_t unit, uint32_t order, int32_t send, int32_t delay)
{
    uint8_t *u = unit_ptr(unit);
    uint8_t o = (uint8_t)order;
    if ((P(uint32_t, u, 0x3e) >> 22 & 1) || P(uint8_t, u, 0x148) == ' ' || (P(uint8_t, u, 0x148) == '!' && o != 'L') ||
        P(uint8_t, u, 0x147) == 'L' || P(uint8_t, u, 0x147) == ' ' || (P(uint8_t, u, 0x147) == '!' && o != 'L'))
        return;
    uint8_t was = P(uint8_t, u, 0x148);
    P(uint8_t, u, 0x148) = o;
    if (!I(int32_t, 0x175) || !unit_is_local(unit) || !ORDER_FN(o) || !send)
        return;
    if (!P(int32_t, unit_ptr(unit), 0x149)) {
        int32_t d = net_delay();
        P(int32_t, unit_ptr(unit), 0x149) = I(int32_t, 0x17dd) + d + delay;
    }
    if (was != o)
        ORDER_FN(o)(unit, order, P(uint16_t, unit_ptr(unit), 0x17d), P(uint32_t, unit_ptr(unit), 0x3e) >> 16 & 1);
}

/* 0x1009ce9f: sets the unit's state; leaving state 0x1e calls its type's leave handler
 * (not for kinds 0x40-0x9f) */
void __cdecl unit_set_state(uint32_t unit, uint8_t state)
{
    uint8_t *u = unit_ptr(unit);
    if (P(uint8_t, u, 0x196) == 0x1e && state != 0x1e && (P(int32_t, u, 0x10) < 0x40 || P(int32_t, u, 0x10) > 0x9f)) {
        int32_t k = P(int32_t, P(uint8_t *, u, 0x32), 4);
        if (k >= 0 && LEAVE_FN(k))
            LEAVE_FN(k)(u);
    }
    P(uint8_t, u, 0x196) = state;
}

typedef void(__cdecl *action_fn)(uint32_t action, uint32_t unit, uint32_t arg, int, int, int, int, uint32_t, int);
#define ACTION_FN(a) G(action_fn, 0x101368a8 + (a) * 8)
#define player_unit_built ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100b1dcb)

/* 0x1009cf5b: sets the unit's action (+0x197), unless in a network game an action 0xb or
 * 0x18 is still waiting for its time (+0x199). A unit leaving state '#' (being built) counts
 * as built for its player. For the local player's units in a network game, with a handler
 * and `send` set, the action's time is set if none yet and the handler is called */
void __cdecl unit_set_action(uint32_t unit, uint32_t action, int32_t send, int32_t delay)
{
    uint8_t *u = unit_ptr(unit);
    if (I(int32_t, 0x175) && P(int32_t, u, 0x199) && (P(uint8_t, u, 0x197) == 0xb || P(uint8_t, u, 0x197) == 0x18))
        return;
    uint8_t a = (uint8_t)action;
    P(uint8_t, u, 0x197) = a;
    if (P(uint8_t, u, 0x196) == '#' && a != '#') {
        uint32_t p = P(uint8_t, u, 0x14);
        I(int32_t, p * ISO_PLAYER_SIZE + 0x1b8b)++;
        player_unit_built(P(uint8_t, unit_ptr(unit), 0x14), P(uint32_t, unit_ptr(unit), 0xc));
        I(int32_t, P(uint8_t, unit_ptr(unit), 0x14) * ISO_PLAYER_SIZE + 0x1b8f)--;
    }
    if (!I(int32_t, 0x175) || !unit_is_local(unit) || !ACTION_FN(a) || !send)
        return;
    if (!P(int32_t, unit_ptr(unit), 0x199)) {
        int32_t d = net_delay();
        P(int32_t, unit_ptr(unit), 0x199) = I(int32_t, 0x17dd) + d + delay;
    }
    uint8_t *v = unit_ptr(unit);
    ACTION_FN(a)(action, unit, ((uint32_t)(uintptr_t)v & 0xffff0000) | P(uint16_t, v, 0x17d), 0, 0, 0, 0,
                 P(uint32_t, unit_ptr(unit), 0x10e) >> 10 & 3, 0);
}

