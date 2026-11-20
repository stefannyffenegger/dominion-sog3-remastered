/* The game logic, 0x1008ceac-0x1008dc1b: the movement process: the game's speed, and
 * IsoMovementLoop, which runs the game ticks that are due: the network and the tick's logic
 * (net_pump), timers, the AI, every unit's handlers and movement, and the power and
 * structure updates. */
#include "iso_core.h"
#include "power.h"
#include "netflags.h"
#include "relay.h"
#include "sector.h"
#include "events.h"
#include "statelog.h"
#include "movement.h"

#define net_message ORIG(void(__cdecl *)(uint32_t, const char *, int), 0x1003ba11)
#define net_status_check ORIG(void(__cdecl *)(void), 0x1003a24e)
#define ai_tick ORIG(void(__cdecl *)(void), 0x100cc1be)
#define unit_scan_targets ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x100da072)
#define turret_step ORIG(void(__cdecl *)(uint32_t), 0x1008c643)
#define power_tick ORIG(void(__cdecl *)(void), 0x100a4a14)
#define structures_tick ORIG(void(__cdecl *)(void), 0x100a621e)
#define net_send_sync ORIG(void(__cdecl *)(int, uint8_t), 0x10034aef)
#define net_send_state ORIG(void(__cdecl *)(int, int, int, uint8_t), 0x10034dcf)
#define UNIT_TICK ((uint8_t *)ORIG_PTR(0x101545f0))  /* per unit type, 0x44 apart: the type's tick */
#define UNIT_AFTER ((uint8_t *)ORIG_PTR(0x101545bc)) /* and its after-move step */

typedef void(__cdecl *unit_fn)(uint32_t);

/* IsoSetMoveDelay: the milliseconds per game tick (trails last half as long). In a network
 * game only the host may change it, to at least 40 ms, once at a time; all players are told
 * the game speeds up (`dir` 1) or down (0). 0 if refused */
int32_t __stdcall IsoSetMoveDelay(uint8_t *iso, uint32_t delay, int dir)
{
    g_iso = iso;
    if (I(int32_t, 0x175) && !I(int32_t, 0x148))
        return 1;
    if (!I(int32_t, 0x175)) {
        I(uint32_t, 0xd863) = delay;
        I(uint32_t, 0xd897) = I(uint32_t, 0xd863) >> 1;
        return 1;
    }
    if (I(int32_t, 0xd8dc) || delay < 0x28)
        return 0;
    if (delay < 6)
        delay = 5; /* never: the check above already refused it */
    I(uint32_t, 0xd8e4) = delay;
    I(uint32_t, 0xd8e8) = I(uint32_t, 0xd8e4) >> 1;
    if (I(int32_t, 0xd863) == I(int32_t, 0xd8e4) && I(int32_t, 0xd897) == I(int32_t, 0xd8e8))
        return 1;
    I(int32_t, 0xd8dc) = 1;
    for (uint8_t p = 0; p < 8; p++) {
        if (dir == 1)
            net_message(p, (const char *)ORIG_PTR(0x101493f4), 0);
        else if (dir == 0)
            net_message(p, (const char *)ORIG_PTR(0x1014940c), 0);
    }
    return 1;
}

/* IsoInitMoveProcess: 60 ms per tick, the tick counters reset */
int32_t __stdcall IsoInitMoveProcess(uint8_t *iso)
{
    g_iso = iso;
    I(uint32_t, 0xd863) = 0x3c;
    I(uint32_t, 0xd897) = I(uint32_t, 0xd863) >> 1;
    static const uint32_t zero[] = { 0x17dd, 0x1093, 0xd877, 0xd8c3, 0xd87b, 0x17e5, 0x17e1, 0xd87f, 0x17f1, 0x17ed };
    for (unsigned k = 0; k < sizeof zero / sizeof *zero; k++)
        I(uint32_t, zero[k]) = 0;
    I(uint32_t, 0xd7fb) = timeGetTime();
    return 1;
}

/* IsoStopMoveProcess */
void __stdcall IsoStopMoveProcess(uint8_t *iso)
{
    g_iso = iso;
    P(uint32_t, iso, 0xd863) = 0;
}

/* IsoPauseGame: pauses (0x17e) or resumes the game (single player, after the first frames) */
void __stdcall IsoPauseGame(uint8_t *iso, uint32_t paused, uint32_t unused)
{
    (void)unused;
    g_iso = iso;
    if (P(uint8_t, iso, 0x17e) == paused) {
        int quiet = paused == 0 ? P(int32_t, iso, 0xd8b7) == 0 : P(int32_t, iso, 0xd8bb) == 0;
        if (quiet && P(uint8_t, iso, 0x181) == paused)
            return;
    }
    if (P(uint32_t, iso, 0x17cd) > 1 && !P(int32_t, iso, 0x175)) {
        P(uint8_t, iso, 0x181) = 0;
        I(uint8_t, 0x17e) = (uint8_t)paused;
        I(uint32_t, 0xd7fb) = timeGetTime();
    }
}

/* one unit's part of a tick */
static void unit_tick(uint32_t id, uint8_t *u)
{
    int32_t type = P(int32_t, u, 0xc);
    unit_fn tick = P(unit_fn, UNIT_TICK, type * 0x44);
    if (I(int32_t, 0x114) && tick) {
        if (type < 0x40 && P(int8_t, u, 0x198) > 0) {
            P(int8_t, u, 0x198)--;
        } else if ((!I(int32_t, 0x175) || unit_is_local_u(u)) && !P(int16_t, u, 0x17d)) {
            /* units at rest look for targets: not busy (action 0xf, a path cell flagged,
             * +0x193 bit 7, +0x1cd bit 3), not carried, in a sector, not of kind 0x10-0x13,
             * not a turret, not of action 0x23 */
            int busy;
            if (P(uint8_t, u, 0x196) == 0xf || P(uint8_t, u, 0x197) == 0xf) {
                busy = 1;
            } else {
                int flagged;
                if (P(int16_t, u, 0x11b) > 0)
                    flagged = P(uint16_t, I(uint8_t *, 0x1984), P(int16_t, u, 0x11b) * 0x12 + 10) >> 8 & 1;
                else
                    flagged = P(int16_t, u, 0x119) > 0 &&
                              (P(uint16_t, I(uint8_t *, 0x1984), P(int16_t, u, 0x119) * 0x12 + 10) >> 8 & 1);
                busy = flagged || (P(uint16_t, u, 0x193) >> 7 & 1);
            }
            if (!busy && !(P(uint16_t, u, 0x1cd) >> 3 & 1) && !P(uint16_t, u, 0x9a) && !P(uint16_t, u, 0xf4) &&
                P(int16_t, u, 0x1a1) != -1 && (type < 0x10 || type > 0x13) && (type < 0x30 || type > 0x37) &&
                P(uint8_t, u, 0x196) != 0x23) {
                int32_t range = 0;
                if (P(int16_t, u, 0xe6) >= 0) {
                    uint8_t *w = I(uint8_t *, 0x1990);
                    range = P(int32_t, P(uint8_t *, w, 0), P(int32_t, w, 0x2c) * P(int16_t, u, 0xe6) * 4 + 4);
                }
                uint32_t sector = ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1a1);
                unit_scan_targets(id, sector, range < 0x140 ? 9 : 0x11, 0);
            }
        }
        if (P(uint8_t, u, 0x196) != 0x1a)
            tick(id);
    }
    /* the type is read again: the tick may have changed it */
    /* reloading: while the weapon's timer (+0xdc) is below its type's full time (+0x28) */
    if ((int32_t)P(uint16_t, u, 0xdc) < P(int32_t, P(uint8_t *, u, 0x32), 0x28)) {
        if (!P(uint8_t, u, 0xed) || !P(int16_t, u, 0x17d)) {
            if (P(uint8_t, u, 0xef)) {
                P(uint8_t, u, 0xef)--;
            } else if (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37) {
                turret_step(id);
            } else if (P(int32_t, u, 0x10) == 0x48 || P(int32_t, u, 0x10) == 0x4c) {
                /* powered vehicles slow down with their power */
                int16_t left = (int16_t)(100 - power_percent((int16_t)id));
                if (left < 0)
                    left = 0;
                int go = 1;
                uint8_t every = 1;
                if (left < 0x18)
                    go = 0;
                else if (left >= 0x19 && left <= 0x31)
                    every = 2;
                else if (left >= 0x32 && left <= 0x4a && !(I(uint8_t, 0x17dd) & 3))
                    go = 0;
                if (go && I(uint32_t, 0x17dd) % every == 0)
                    unit_reload_step(id);
            } else {
                unit_reload_step(id);
            }
        }
    } else {
        P(uint8_t, u, 0xed) = 1;
    }
    if ((P(int32_t, u, 0xc) < 0x84 || P(int32_t, u, 0xc) > 0x87) && (P(uint32_t, u, 0x3e) >> 21 & 1))
        ring_tick(id);
    unit_fn after = P(unit_fn, UNIT_AFTER, P(int32_t, u, 0xc) * 0x44);
    if (after)
        after(id);
}

/* IsoMovementLoop: runs the game ticks that are due (0x17e5 - 0x17e1), unless paused. The
 * `IsoKaboom` flag (0xe020) crashes the game here on purpose, as in the original */
void __stdcall IsoMovementLoop(uint8_t *iso)
{
    g_iso = iso;
    if (P(uint8_t, iso, 0x17e) || P(uint8_t, iso, 0x180))
        return;
    if (P(int32_t, iso, 0xe020))
        ORIG(void *(__cdecl *)(void *, const void *, uint32_t), 0x100f5530)(0, 0, 1);
    int32_t due = I(int32_t, 0x17e5) - I(int32_t, 0x17e1);
    if (I(uint8_t, 0xd947)) {
        net_message(0, (const char *)ORIG_PTR(0x10149454), 0);
        I(uint8_t, 0xd947) = 0;
    }
    while (due) {
        for (uint16_t k = 0; k < 8; k++)
            G(uint32_t, 0x1017df88 + k * 4) = 0;
        uint32_t t0 = timeGetTime();
        if (statelog_tick)
            statelog_tick(iso);
        else
            ORIG(void(__stdcall *)(uint8_t *), 0x10045c72)(iso);
        uint32_t t1 = timeGetTime();
        I(uint32_t, 0xbee2) = (I(uint32_t, 0xbee2) + (t1 - t0)) >> 1;
        if (I(int32_t, 0xd94a) && I(uint32_t, 0x17dd) % 5 == 0)
            net_status_check();
        if (I(uint32_t *, 0xcd9e))
            for (uint32_t at = *I(uint32_t *, 0xcd9e); at && at <= I(uint32_t, 0x17dd); at = *I(uint32_t *, 0xcd9e))
                timer_fire(0);
        I(uint32_t, 0x17dd)++;
        I(int32_t, 0x1093) += I(int32_t, 0xd863);
        if (!I(int32_t, 0x110)) {
            t0 = timeGetTime();
            ai_tick();
            t1 = timeGetTime();
            I(uint32_t, 0xbed2) = (I(uint32_t, 0xbed2) + (t1 - t0)) >> 1;
        }
        for (uint16_t k = 0; (int32_t)k < I(int32_t, ISO_UNIT_COUNT); k++) {
            uint32_t id = (uint16_t)I(uint32_t *, ISO_UNIT_LIST)[k];
            uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
            if (u)
                unit_tick(id, u);
        }
        power_tick();
        structures_tick();
        if (I(int32_t, 0x175) && I(int32_t, 0x148)) {
            if (I(uint8_t, 0xd91d))
                net_send_sync(0, 2);
            if (I(uint8_t, 0xd91c))
                net_send_state(0, 0, 0, 1);
        }
        due--;
        I(int32_t, 0x17e1)++;
    }
}

/* 0x1008dd2b: a unit's reload step: its timer (+0xdc) goes up by its rate (+0xe0), to the
 * type's full time (+0x28), and the type's pause (+0xfc) starts (+0xef) */
void __cdecl unit_reload_step(uint32_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    uint8_t *type = P(uint8_t *, u, 0x32);
    if ((int32_t)(P(uint16_t, u, 0xe0) + P(uint16_t, u, 0xdc)) < P(int32_t, type, 0x28))
        P(int16_t, u, 0xdc) = (int16_t)(P(int16_t, u, 0xe0) + P(int16_t, u, 0xdc));
    else
        P(int16_t, u, 0xdc) = (int16_t)P(int32_t, type, 0x28);
    P(uint8_t, u, 0xef) = P(uint8_t, type, 0xfc);
}

/* 0x1008de00: allocates the 4000-byte buffer at 0x1014946c (once). 1 if it did */
int32_t __cdecl move_buffer_alloc(void)
{
    if (G(void *, 0x1014946c))
        return 0;
    G(void *, 0x1014946c) = winmalloc(4000, 0);
    if (!G(void *, 0x1014946c)) {
        OutputDebugStringA((const char *)ORIG_PTR(0x1014948c));
        return 0;
    }
    memset(G(void *, 0x1014946c), 0, 4000);
    return 1;
}
