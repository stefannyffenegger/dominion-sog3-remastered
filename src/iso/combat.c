/* The game logic, 0x1000d96c-0x1001065b (without 0x1000ec9e-0x1000f2e9): a unit hit by a
 * weapon or an effect (damage, death and its score, fighting back), its reaction and the
 * hit's sprite, allies called for help, killing a unit, destroyed structures and what is
 * in, on or under them. */
#include "iso_core.h"
#include "sound.h"
#include "sector.h"
#include "damage.h"
#include "combat.h"
#include "screen.h"

/* not rewritten yet (names ours, provisional) */
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, char), 0x1009ce9f)
#define player_announce ORIG(void(__cdecl *)(uint32_t, int, int), 0x1005810f)
#define hint_show ORIG(int(__cdecl *)(int, int16_t, int16_t, int, int), 0x1001bbfc)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define health_badly_hurt ORIG(int(__cdecl *)(uint8_t *, int), 0x100a2550)
#define health_hurt ORIG(int(__cdecl *)(uint8_t *, int), 0x100a24d0)
#define score_unit_lost ORIG(void(__cdecl *)(uint32_t, uint8_t, uint8_t), 0x1007e870)
#define reveal_around ORIG(void(__cdecl *)(int, int, uint32_t, int, uint32_t, uint16_t, int), 0x10090223)
#define net_unit_hit ORIG(int(__cdecl *)(uint32_t, char), 0x10034aef)
#define net_unit_killed ORIG(int(__cdecl *)(uint32_t, int, char, char), 0x10034dcf)
#define team_lost_leader ORIG(void(__cdecl *)(int), 0x1002dc7f)
#define unit_set_target ORIG(void(__cdecl *)(uint16_t, uint16_t), 0x100dcabd)
#define unit_can_reach ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x1006b6d3)
#define unit_attack_target ORIG(void(__cdecl *)(uint32_t, uint16_t, int, uint16_t), 0x100dc75c)
#define player_is_ai ORIG(int(__cdecl *)(int), 0x100e2df0)
#define effect_slot_free ORIG(int(__cdecl *)(uint16_t), 0x1006cdc0)
#define effect_slot_take ORIG(uint32_t(__cdecl *)(uint8_t, uint16_t), 0x1006bbee)
#define effect_start                                                                         \
    ORIG(void(__cdecl *)(void *, int, int, int, uint32_t, int, int, int, int, uint32_t, uint32_t), 0x1006d9bd)
#define unit_selected ORIG(void(__cdecl *)(uint32_t, int), 0x1004ebe6)
#define path_stop ORIG(int(__cdecl *)(uint32_t, int), 0x100d4947)
#define path_release ORIG(int(__cdecl *)(uint32_t, int, int, int), 0x100d559a)
#define structure_wreck ORIG(void(__cdecl *)(uint32_t, int), 0x100b2953)
#define IsoHidePopupMenu ORIG(void(__stdcall *)(uint8_t *), 0x100d4263)

#define U_FLAGS2 0x10e
#define U_TYPEINFO 0x32
#define U_KIND 0x196
#define U_KIND2 0x197
#define U_CARRIER 0x17d
#define U_LAST_ATTACKER 0x18b
#define U_SECTOR 0x1a1
#define U_SECTOR_NEXT 0x393
#define ISO_TICK 0x17dd
#define g_no_damage G(int32_t, 0x1013d6f4) /* cheat: the local player's units take no damage */

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

static uint8_t *weapon_record(int16_t w)
{
    int32_t *t = I(int32_t *, 0x1990);
    return (uint8_t *)(uintptr_t)(t[0] + t[0xb] * w * 4);
}

/* 0x1000e7e2: a unit that has just been hit: infantry duck, vehicles and structures turn
 * to their hurt and badly hurt states; then the hit's sprite */
void __cdecl unit_hit(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return;
    P(uint32_t, u, 0x1c5) = I(uint32_t, ISO_TICK) + 6;
    switch (P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x74)) {
    case 3:
        if ((P(int32_t, u, U_ID_TYPE) < 0x30 || P(int32_t, u, U_ID_TYPE) > 0x37) && !P(uint8_t, u, 0x147))
            unit_set_anim(id, 0x1f, 1, 0);
        break;
    case 4:
    case 5: {
        int busy = P(uint8_t, u, 0x147) == 0x14 || P(uint8_t, u, 0x148) == 0x14 || P(uint8_t, u, 0x147) == 'O' ||
                   P(uint8_t, u, 0x148) == 'O';
        if ((int16_t)health_badly_hurt(u, 0) < P(int16_t, u, 0xd6)) {
            if (P(int16_t, u, 0xd6) <= (int16_t)health_hurt(u, 0) && !busy)
                P(uint8_t, u, 0x148) = 2;
        } else if (!busy) {
            P(uint8_t, u, 0x148) = 3;
        }
        break;
    }
    }
    hit_effect(id);
}

/* 0x1000ead8: the hit's sprite on the unit, if its type has one */
void __cdecl hit_effect(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t *ti = P(uint8_t *, u, U_TYPEINFO);
    int level = 0, x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y);
    switch (P(int16_t, u, 0x28)) {
    case 1:
    case 3: level = 1; break;
    case 4: level = 0; break;
    case 5: level = -1; break;
    }
    if (P(int32_t, ti, 0x5c) > 0 && level) {
        uint32_t slot;
        if (effect_slot_free((uint16_t)id) && (int)(slot = effect_slot_take(0, (uint16_t)id)) >= 0 &&
            (int)slot < (int)I(uint16_t, 0xd6e7)) {
            P(int16_t, u, 0x1c3) = (int16_t)slot;
            if (P(int32_t, u, U_ID_TYPE) > 0x2f && P(int32_t, u, U_ID_TYPE) < 0x38) {
                x = P(int16_t, u, U_X);
                y = P(int16_t, u, U_Y) - (int16_t)ftol(P(float, P(uint8_t *, u, 0x39d), 0x20));
            }
            effect_start(P(void *, I(uint8_t *, 0xd6e9) + 6 + slot * 0xe, 0), P(int16_t, u, 0x66) + level, x, y,
                         P(uint32_t, ti, 0x58), P(int32_t, ti, 0x5c), 0, 0, 0, 0x40, 0);
        }
    }
    unit_selected(id, 1);
}

/* 0x1000f2e9: the player's units in the sector, not already busy, take on the attacker
 * (at most once a second for a unit) */
void __cdecl call_for_help(uint32_t id, uint32_t attacker)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u || P(uint32_t, u, 0x10a) > I(uint32_t, ISO_TICK))
        return;
    P(uint32_t, u, 0x10a) = I(uint32_t, ISO_TICK) + 0x3c;
    uint8_t *a = iso_unit((int)(attacker & 0xffff));
    int16_t sector = P(int16_t, u, U_SECTOR);
    if (!a || (1u << (P(uint8_t, a, U_PLAYER) & 0x1f) & I(uint8_t, P(uint8_t, u, U_PLAYER) * ISO_PLAYER_SIZE + 0x1ba0)) ||
        sector == -1 || (uint32_t)(int)sector >= I(uint32_t, 0xc2f1))
        return;
    uint16_t v = P(uint16_t, I(uint8_t *, 0xc2eb) + sector * 0x1e, 2);
    while (v) {
        uint8_t *w = iso_unit(v);
        if (w && v != (id & 0xffff) && P(uint8_t, u, U_PLAYER) == P(uint8_t, w, U_PLAYER) &&
            !iso_unit(P(uint16_t, w, U_CARRIER))) {
            uint8_t k = P(uint8_t, w, U_KIND);
            if (((player_is_ai(P(uint8_t, w, U_PLAYER)) && k != 0x14) || k == 0x1e || k == 0x1f || k == 0x17) &&
                k != 0xf && P(uint8_t, w, U_KIND2) != 0xf && k != 0x1a && !P(int16_t, w, 0x9a) &&
                P(int32_t, P(uint8_t *, w, U_TYPEINFO), 0x74) != 4 && k != 0x23 && unit_can_reach(v, attacker, 0, 0))
                unit_attack_target(v, (uint16_t)attacker, 0, 1);
        }
        v = w ? P(uint16_t, iso_unit(v), U_SECTOR_NEXT) : 0;
    }
}

/* 0x1000d96c: unit `id` hit by `attacker`'s weapon, or (attacker 0) by effect `effect`, at
 * `distance` from the centre (0: direct) */
void __cdecl unit_hit_by(uint32_t id, uint16_t attacker, uint32_t effect, int16_t distance)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if ((P(uint32_t, u, U_FLAGS2) >> 4 & 1) || P(uint8_t, u, U_KIND) == 0x1a || P(uint8_t, u, U_KIND2) == 0x1a ||
        !P(int16_t, u, 0xd6))
        return;
    int32_t type = P(int32_t, u, U_ID_TYPE);
    if (type > 0x3f && type < 0xa0 && (P(uint8_t, u, 0x147) == 0x14 || P(uint8_t, u, 0x148) == 0x14))
        return;
    int16_t was = P(int16_t, u, 0xd6);
    uint8_t *w;
    uint16_t from;
    if (!attacker) {
        uint8_t *e = I(uint8_t *, 0xd6db) + (effect & 0xffff) * 0x44;
        w = P(uint8_t *, e, 0x16);
        from = P(uint16_t, e, 8);
    } else {
        w = weapon_record(P(int16_t, iso_unit(attacker), 0xe6));
        from = attacker;
    }
    int32_t armour = P(uint16_t, u, 0xd8), dmg;
    if (!(uint16_t)distance) {
        dmg = P(int32_t, w, 0x38) - armour;
    } else {
        int32_t radius = P(int32_t, w, 0x40), least = P(int32_t, w, 0x4c);
        if (attacker) {
            /* weapons: double range; a healthy attacker does half again, a wounded one half */
            radius *= 2;
            int32_t d = P(int32_t, w, 0x5c);
            uint8_t *a = iso_unit(from);
            int32_t most = P(int32_t, P(uint8_t *, a, U_TYPEINFO), 0x10);
            if (most / 2 < P(int16_t, a, 0xd6))
                d += d / 2;
            else if (most / 4 >= P(int16_t, a, 0xd6))
                d /= 2;
            dmg = ftol((long double)d / (long double)radius * (long double)((radius - (uint16_t)distance) * 2)) - armour;
        } else {
            int32_t full = P(int32_t, w, 0x38);
            dmg = ftol((long double)(full - least) / (long double)radius * (long double)(radius - (uint16_t)distance)) +
                  (least - armour);
        }
    }
    if (P(uint32_t, u, U_FLAGS2) >> 0x11 & 1)
        dmg = ftol((long double)dmg * (long double)G(double, 0x10113060)); /* 0.25 */
    else if (P(uint32_t, u, U_FLAGS2) >> 3 & 1)
        dmg /= 2;
    if (g_no_damage && P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER))
        dmg = 0;
    call_for_help(id, from);
    if (dmg < 1) {
        dmg = 0;
        if (P(int32_t, w, 0x50) == 3)
            hint_show(0x22, P(int16_t, u, U_X), P(int16_t, u, U_Y), 0, 0);
    } else {
        if (!(P(uint32_t, u, U_FLAGS) >> 0xf & 1)) {
            int32_t cls = P(int32_t, u, U_CLASS);
            player_announce(P(uint8_t, u, U_PLAYER), cls < 0x40 || cls > 0x9f ? 0x2f : 0x2e, 1);
            P(uint32_t, u, U_FLAGS) |= 0x8000;
        }
        if (P(int16_t, u, 0xd6) < (int16_t)dmg)
            P(int16_t, u, 0xd6) = 0;
        else
            P(int16_t, u, 0xd6) = (int16_t)(P(int16_t, u, 0xd6) - (int16_t)dmg);
    }

    if (P(int16_t, u, 0xd6) < 1) {
        /* destroyed */
        P(int16_t, u, 0xd6) = 0;
        if (I(int, 0x175) && !I(int, 0x148)) { /* a joined player waits for the host's word */
            P(int16_t, u, 0xd6) = 1;
            return;
        }
        uint8_t *a = iso_unit(from);
        if (a) {
            P(uint16_t, u, U_LAST_ATTACKER) = from;
            if (P(uint8_t, u, 0x2c) && P(uint8_t, a, 0x2c) == P(uint8_t, u, 0x2c)) {
                team_lost_leader(P(uint8_t, u, 0x2c));
                P(uint32_t, u, U_FLAGS) |= 0x2000;
            }
            if (P(uint16_t, a, U_CARRIER) == (id & 0xffff)) {
                unit_set_target(from, 0);
                P(uint16_t, iso_unit(from), 0x193) &= 0xfffe;
            }
            if (P(uint8_t, u, U_KIND) != 0x1a && !(P(uint32_t, u, U_FLAGS) >> 0x1e & 1)) {
                score_unit_lost(P(uint32_t, u, U_CLASS), P(uint8_t, u, U_PLAYER), P(uint8_t, iso_unit(from), U_PLAYER));
                P(uint32_t, u, U_FLAGS) |= 0x40000000;
            }
            a = iso_unit(from);
            /* original: the player byte with the upper half of the attacker's address */
            reveal_around(P(int16_t, u, U_X), P(int16_t, u, U_Y),
                          ((uint32_t)(uintptr_t)a & 0xffff0000) | P(uint8_t, a, U_PLAYER),
                          P(int32_t, P(uint8_t *, u, U_TYPEINFO), 0x74) < 4 ? 2 : 5, 0xffffffff, 0, 0);
        }
        if (P(uint8_t, u, 0x148) == 'L' || P(uint8_t, u, 0x148) == ' ')
            return;
        unit_kill(id & 0xffff, 0x20, 1, 0);
        uint8_t *v = iso_unit((int)(id & 0xffff));
        if (P(uint32_t, v, U_FLAGS) >> 0x1e & 1)
            return;
        score_unit_lost(P(uint32_t, v, U_CLASS), P(uint8_t, v, U_PLAYER), P(uint8_t, v, U_PLAYER));
        P(uint32_t, iso_unit((int)(id & 0xffff)), U_FLAGS) |= 0x40000000;
        return;
    }
    if (I(int, 0x175) && I(int, 0x148) && dmg > 0) {
        /* the host tells the others when a unit falls below half or a quarter */
        int16_t most = (int16_t)unit_max_health(u, 0);
        int half = most >> 1, quarter = most >> 2;
        if ((half < was && P(int16_t, u, 0xd6) < half) || (quarter < was && P(int16_t, u, 0xd6) < quarter))
            net_unit_hit(id & 0xffff, 0);
    }
    if (dmg > 0)
        unit_hit(id);

    /* fighting back */
    int busy = iso_unit(P(uint16_t, u, U_CARRIER)) != NULL;
    if (busy && P(int16_t, u, 0xe6) >= 0 && (P(uint16_t, u, 0x193) & 1)) {
        int pierces = weapon_pierces(id, from), pierces_carrier = weapon_pierces_carried(id, from);
        if (pierces) {
            if (P(uint8_t, u, U_KIND) == 0xb) {
                busy = 0;
                P(uint16_t, u, U_LAST_ATTACKER) = from;
            }
            if (P(uint8_t, u, U_KIND) == 0x14 && pierces_carrier) {
                int32_t ct = P(int32_t, iso_unit(P(uint16_t, u, U_CARRIER)), U_ID_TYPE);
                if ((ct >= 0x50 && ct <= 0x9f) || (ct >= 0x40 && ct <= 0x43)) {
                    busy = 0;
                    P(uint16_t, u, U_LAST_ATTACKER) = from;
                }
            }
        }
    }
    if (I(int, 0x175) && !unit_is_local_u(u))
        return;
    if (P(int32_t, u, U_ID_TYPE) < 0x40 && !unit_can_reach(id, from, 0, 0)) {
        /* can't reach the attacker: an enemy's unarmed unit runs */
        type = P(int32_t, u, U_ID_TYPE);
        if (P(uint8_t, u, U_PLAYER) != I(uint32_t, ISO_LOCAL_PLAYER) && P(int16_t, u, 0xe6) < 0 &&
            (type < 0x3c || type > 0x3f) && (type < 0x30 || type > 0x37))
            unit_set_action(id, 0xd, 1, 0);
        return;
    }
    uint8_t k = P(uint8_t, u, U_KIND);
    if (iso_unit(from) && !busy && !(P(uint16_t, u, 0x1cd) >> 1 & 1) && !(P(uint16_t, u, 0x193) >> 7 & 1) && k != 0x13 &&
        k != 0xf && P(uint8_t, u, U_KIND2) != 0xf && k != '8' && k != '9' && k != ':' && k != ';' &&
        (P(int16_t, u, 0x11b) < 1 ||
         !(P(uint16_t, I(uint8_t *, 0x1984) + P(int16_t, u, 0x11b) * 0x12, 10) >> 8 & 1)) &&
        P(uint8_t, iso_unit(from), U_PLAYER) != P(uint8_t, u, U_PLAYER)) {
        type = P(int32_t, u, U_ID_TYPE);
        if ((type < 0x50 || type > 0x9f) && (type < 0x40 || type > 0x43)) {
            unit_attack_target(id, from, 0, 1);
            P(uint16_t, u, 0x193) |= 0x100;
        }
        P(uint16_t, u, U_LAST_ATTACKER) = from;
    }
}

/* 0x1000f5e5: kills a unit with the dying animation `anim`; sync: tell the other players;
 * quiet: no score (kept in flags bit 17). Structures are wrecked instead. */
void __cdecl unit_kill(uint32_t id, uint32_t anim, int sync, uint32_t quiet)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    DebugStr((const char *)ORIG_PTR(0x10118c60), id, anim, sync, quiet);
    if (!u || (I(int, 0x175) && !I(int, 0x148) && sync))
        return;
    LPCRITICAL_SECTION lock = (LPCRITICAL_SECTION)ORIG_PTR(0x1017bcd8);
    if (I(int, 0xd813))
        EnterCriticalSection(lock);
    P(uint32_t, u, U_FLAGS) = (P(uint32_t, u, U_FLAGS) & 0xfffdffff) | (quiet & 1) << 0x11;
    P(uint32_t, u, U_FLAGS) &= 0xfffffffd;
    P(uint32_t, u, U_FLAGS) &= 0xfffffffe;
    P(uint8_t, u, 0x158) = 0;
    P(int16_t, u, 0xd6) = 0;
    if (I(int, 0xdaf0) && P(uint8_t, u, U_SELECTED))
        IsoHidePopupMenu(g_iso);
    int32_t cls = P(int32_t, u, U_CLASS);
    if (cls < 0x88 || cls > 0x9c) {
        if (P(int16_t, u, 0x113) > 0 && P(uint8_t, u, 0x7d) == 8) {
            path_stop(id, -1);
            path_release(id, -1, 1, 0);
            P(uint8_t, u, 0x7d) = 1;
        }
        unit_set_anim(id, anim, 1, 0);
        unit_set_mode(id, 0x1a);
        unit_set_action(id, 0x1a, 1, 0);
        if (sync && I(int, 0x175))
            net_unit_killed((int)id, quiet == 0, (char)anim, 0);
        P(uint32_t, u, U_FLAGS2) |= 0x100000;
    } else {
        structure_wreck(id, 1);
        structure_kill(id, anim, sync);
    }
    if (I(int, 0xd813))
        LeaveCriticalSection(lock);
}

/* ---- structures and what is on or in them ---- */

#define schedule ORIG(void(__cdecl *)(uint16_t, uint32_t, uint32_t, uint32_t, uint32_t *), 0x1002de13)
#define IsoRemoveObject ORIG(void(__stdcall *)(uint8_t *, uint32_t, int, int), 0x100ad9ea)
#define IsoRedrawObject ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x100ad5f2)
#define U_INSIDE 0xf6  /* short: the first unit inside, linked through +0xf6 */
#define U_ON_TOP 0xf8  /* short: the first unit on it, linked through +0xf8 */

/* the units inside and on top of a destroyed structure: each is handled `step` ticks after
 * the one before by the original's `handler` (event `event`) */
static void occupants(uint32_t id, uint16_t event, uint32_t step, uint32_t handler, uint32_t *args, int eject)
{
    uint32_t tick = I(uint32_t, ISO_TICK);
    for (int list = 0; list < 2; list++) {
        uint16_t link = list ? U_ON_TOP : U_INSIDE;
        uint16_t v = P(uint16_t, I(uint8_t **, ISO_UNITS)[id], link);
        if (list)
            tick = I(uint32_t, ISO_TICK);
        while (v) {
            uint8_t *w = iso_unit(v);
            if (!w) {
                v = 0;
                continue;
            }
            if (list)
                P(uint32_t, w, U_FLAGS2) |= 0x100000;
            if (eject)
                P(uint8_t, w, U_KIND) = 0x14;
            args[0] = v;
            tick += step;
            schedule(event, tick, 0, handler, args);
            kill_units_on(w, 0, 0);
            v = P(uint16_t, w, link);
            P(uint16_t, w, U_INSIDE) = 0;
            P(uint16_t, w, U_ON_TOP) = 0;
        }
        P(uint16_t, I(uint8_t **, ISO_UNITS)[id], link) = 0;
    }
}

/* 0x1000f85e: a destroyed structure: its dying animation; the units in and on it are
 * killed one after another (8 ticks apart) */
void __cdecl structure_kill(uint32_t id, uint32_t anim, int sync)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    if (!u) {
        DebugStr((const char *)ORIG_PTR(0x10118cb0), id);
        return;
    }
    unit_set_anim(id, anim, 1, 0);
    unit_set_mode(id, 0x1a);
    unit_set_action(id, 0x1a, 1, 0);
    if (sync && I(int, 0x175))
        net_unit_killed((int)id, 1, (char)anim, 0);
    if (P(int16_t, u, U_INSIDE) || P(int16_t, u, U_ON_TOP)) {
        uint32_t args[3] = {0, 0x20, 1};
        kill_units_on(u, 0, 0);
        occupants(id, 0xee, 8, (uint32_t)(uintptr_t)ORIG_PTR(0x1005629c), args, 0);
    }
}

/* 0x1000fb28: a unit leaving a destroyed structure (action 0x14), and those in and on it
 * one after another (16 ticks apart) */
void __cdecl occupant_eject(int id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
    if (!u) {
        DebugStr((const char *)ORIG_PTR(0x10118cc8), id);
        return;
    }
    uint8_t a = P(uint8_t, u, 0x147), n = P(uint8_t, u, 0x148);
    if (a == 0x14 || a == 'O' || a == 'L' || a == ' ' || a == '!' || n == 'L' || n == ' ')
        return;
    P(uint8_t, u, U_KIND) = 0x14;
    P(uint8_t, u, 0x148) = P(uint8_t, u, U_KIND);
    if (P(uint8_t, u, U_PLAYER) == I(uint32_t, ISO_LOCAL_PLAYER))
        unit_voice(u, 8, 0, 0);
    if (P(int16_t, u, U_INSIDE) || P(int16_t, u, U_ON_TOP)) {
        uint32_t args[3] = {0, 0, 0};
        kill_units_on(u, 0, 0);
        occupants((uint32_t)id, 0xef, 0x10, (uint32_t)(uintptr_t)ORIG_PTR(0x1000fb28), args, 1);
    }
}

/* 0x1000fe89 */
void __cdecl kill_units_on_id(int id, int building, int keep_units)
{
    if (I(uint8_t **, ISO_UNITS)[id])
        kill_units_on(I(uint8_t **, ISO_UNITS)[id], building, keep_units);
}

/* the unit's screen bounds in +0xc6 (a RECT) */
static void unit_bounds(uint8_t *u)
{
    if (!(P(uint32_t, u, U_FLAGS) >> 6 & 1))
        unit_update_bounds(((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, 0x1cb));
    else
        SpriteGetBounds(I(void *, ISO_HSL), P(void *, u, 4), (RECT *)(u + 0xc6));
}

/* 0x1000fed3: a bridge destroyed or a structure built (`building`): the units under it
 * (192x96 pixels around it, in its sector and the eight around) are killed unless
 * keep_units; for a building, the map objects under it are removed and those near them
 * redrawn */
void __cdecl kill_units_on(uint8_t *u, int building, int keep_units)
{
    if (I(int, 0x175) && !I(int, 0x148))
        return;
    DebugStr((const char *)ORIG_PTR(0x10118ce0), building, keep_units);
    unit_bounds(u);
    RECT *area = (RECT *)(u + 0xc6);
    InflateRect(area, (0xc0 - (area->right - area->left)) / 2, (0x60 - (area->bottom - area->top)) / 2);
    int sx = (I(int32_t, 0xd6f1) >> 1) + P(int16_t, u, U_X);
    int sy = P(int16_t, u, U_Y) + (I(int32_t, 0xd6f5) >> 1);
    int sector = ((sy >> 8) << (I(uint8_t, 0xc2f5) & 0x1f)) + (sx >> 8);
    for (int k = 0; k < 9; k++) {
        int s = k == 0 ? sector : P(int16_t, I(uint8_t *, 0xc2eb), k * 2 + 4 + sector * 0x1e);
        if (s < 0)
            continue;
        RECT tmp;
        if (!keep_units) {
            uint16_t v = P(uint16_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 2);
            while (v) {
                uint8_t *w = iso_unit(v);
                if (!w) {
                    DebugStr((const char *)ORIG_PTR(0x10118d04), v);
                    return;
                }
                uint16_t next = P(uint16_t, w, U_SECTOR_NEXT);
                int16_t ws = P(int16_t, w, U_SECTOR);
                int32_t cls = P(int32_t, w, U_CLASS), t = P(int32_t, w, U_ID_TYPE);
                if ((ws == s || ws == -1) && (cls < 0x88 || cls > 0x9f)) {
                    int32_t top = t > 0x2f && t < 0x38 ? P(int32_t, P(uint8_t *, w, 0x39d), 0xc4) : 0;
                    if (!(t > 0x2f && t < 0x38) || top == 10 || top == 9 || top == 5) {
                        unit_bounds(w);
                        if (IntersectRect(&tmp, area, (RECT *)(w + 0xc6))) {
                            unit_kill(v, 0x20, 1, 0);
                            if (!(P(uint32_t, w, U_FLAGS) >> 0x1e & 1)) {
                                score_unit_lost(P(uint32_t, w, U_CLASS), P(uint8_t, w, U_PLAYER), P(uint8_t, u, U_PLAYER));
                                P(uint32_t, w, U_FLAGS) |= 0x40000000;
                            }
                        }
                    }
                }
                v = next;
            }
        }
        if (!building)
            continue;
        DebugStr((const char *)ORIG_PTR(0x10118d1c), building);
        DebugStr((const char *)ORIG_PTR(0x10118d2c), P(int32_t, u, U_CLASS));
        RECT all = {0, 0, 0, 0}, r;
        uint8_t *rec = I(uint8_t *, 0xc2eb) + s * 0x1e;
        for (int o = P(int16_t, rec, 4); o >= 0;) {
            uint8_t *ob = I(uint8_t *, 0x192c) + o * 0x12;
            int next = P(int16_t, ob, 0xe);
            uint16_t ot = P(uint16_t, ob, 8);
            if (P(uint32_t, ob, 0) && (ot < 0x6f || ot > 0x77) && (ot < 0xb4 || ot > 0x103) &&
                (ot < 0x104 || ot > 0x107) && (ot < 0x78 || ot > 0x7b) && (ot < 0xaa || ot > 0xb1) &&
                (ot < 300 || ot > 0x15d) && (ot < 0x15e || ot > 0x185)) {
                SpriteGetBounds(I(void *, ISO_HSL), RtGetResource(I(void *, ISO_APP), P(uint32_t, ob, 0))[1], &r);
                if (IntersectRect(&tmp, area, &r)) {
                    UnionRect(&all, &all, &r);
                    IsoRemoveObject(g_iso, (uint32_t)o, keep_units, 1);
                }
            }
            o = next;
        }
        InflateRect(&all, 0x20, 0x10);
        for (int o = P(int16_t, rec, 4); o >= 0; o = P(int16_t, I(uint8_t *, 0x192c) + o * 0x12, 0xe)) {
            uint8_t *ob = I(uint8_t *, 0x192c) + o * 0x12;
            if (!P(uint32_t, ob, 0))
                continue;
            SpriteGetBounds(I(void *, ISO_HSL), RtGetResource(I(void *, ISO_APP), P(uint32_t, ob, 0))[1], &r);
            RECT tmp2;
            if (IntersectRect(&tmp2, &all, &r))
                IsoRedrawObject(g_iso, (uint32_t)o);
        }
    }
}

/* 0x100105fe */
BOOL __cdecl sprites_overlap(void *a, void *b)
{
    RECT ra, rb, tmp;
    SpriteGetBounds(I(void *, ISO_HSL), a, &ra);
    SpriteGetBounds(I(void *, ISO_HSL), b, &rb);
    return IntersectRect(&tmp, &ra, &rb);
}
