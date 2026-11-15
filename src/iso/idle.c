/* The game logic, 0x100118ad-0x100120c0: infantry standing about. Every so often a unit
 * at rest plays one of its idle animations (chosen by the original's C library rand(),
 * seeded from the unit's facing and id); a group's leader can also stir the group. */
#include "iso_core.h"
#include "idle.h"

/* the original's C library: the same random sequence */
#define orig_srand ORIG(void(__cdecl *)(unsigned), 0x100f4d60)
#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define group_busy ORIG(int(__cdecl *)(uint32_t), 0x1009514b)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)

#define U_FACING 0x79
#define U_IDLE_WAIT 0x16b /* ticks until the next idle animation */
#define U_ANIMS 0x15f     /* the unit's action records, 0x2c bytes each */
#define U_LIST_ID 0x395
#define U_KIND 0x196
#define U_KIND2 0x197
#define ISO_TICK 0x17dd
typedef void(__cdecl *KindReset)(uint8_t *);
#define g_kind0_reset G(KindReset, 0x10118d90)

/* an action record with any steps */
static int has_anim(uint8_t *u, uint32_t a) { return P(int8_t, P(uint8_t *, u, U_ANIMS), a * 0x2c) > 0; }

static void seed(uint8_t *u) { orig_srand(P(uint8_t, u, U_FACING) + (unsigned)P(uint16_t, u, 0x1cb)); }

/* standing still in the "at ease" kind 0x1e */
static int at_ease(uint8_t *u)
{
    return !P(uint8_t, u, 0x147) && !P(uint8_t, u, 0x148) && P(uint8_t, u, U_KIND) == 0x1e && P(uint8_t, u, U_KIND2) == 0x1e;
}

/* the unit's id as the original passes it: with the upper half of its address */
static uint32_t unit_id_hi(uint8_t *u) { return ((uint32_t)(uintptr_t)u & 0xffff0000) | P(uint16_t, u, U_LIST_ID); }

/* one of the two idle animations for its facing (0x28 + facing * 3) */
static void idle_anim(uint8_t *u)
{
    seed(u);
    uint8_t a = (uint8_t)(orig_rand() % 2 + P(uint8_t, u, U_FACING) * 3 + 0x28);
    if (has_anim(u, a))
        unit_set_anim(unit_id_hi(u), a, 1, 0); /* original: the byte with stack leftovers above */
    idle_wait(u);
}

/* 0x100119d0: the next idle animation in 21-35 periods of 16 ticks, at a period start */
void __cdecl idle_wait(uint8_t *u)
{
    seed(u);
    int r = orig_rand();
    P(uint32_t, u, U_IDLE_WAIT) = (uint32_t)((r % 0xf + 0x15) * 0x10) + ((I(uint32_t, ISO_TICK) & 0xfffffff0) - I(uint32_t, ISO_TICK));
}

/* 0x100118ad: a unit on its own */
int __cdecl idle_solo(uint8_t *u)
{
    if (!at_ease(u) || (P(uint32_t, u, 0x10e) >> 0xc & 1))
        return 0;
    if (!P(int32_t, u, U_IDLE_WAIT))
        idle_anim(u);
    else
        P(int32_t, u, U_IDLE_WAIT)--;
    return 0;
}

/* 0x10011a40: a group member (1 while its group is busy) */
int __cdecl idle_member(uint8_t *u)
{
    if (P(uint8_t, u, U_GROUP) && group_busy(P(uint8_t, u, U_GROUP)))
        return 1;
    if (!at_ease(u))
        return 0;
    if (!P(int32_t, u, U_IDLE_WAIT))
        idle_anim(u);
    else
        P(int32_t, u, U_IDLE_WAIT)--;
    return 0;
}

/* 0x10011b82: a group's leader: the infantry of its group at ease play their 0x2a animation
 * (the most hurt sets how long until the next time), those busy with their own action stop
 * it; then the leader's own animation (0x2c when the group stirred, else 0x29) */
int __cdecl idle_leader(uint8_t *u)
{
    if (!at_ease(u))
        return 0;
    if (P(int32_t, u, U_IDLE_WAIT)) {
        P(int32_t, u, U_IDLE_WAIT)--;
        return 0;
    }
    int stirred = 0, most_hurt = 0;
    if (P(uint8_t, u, U_GROUP)) {
        int8_t group = P(int8_t, u, U_GROUP), player = P(int8_t, u, U_PLAYER);
        for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
            uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
            uint8_t *w = I(uint8_t **, ISO_UNITS)[v];
            if (!w || P(int8_t, w, U_GROUP) != group || P(int8_t, w, U_PLAYER) != player ||
                P(int32_t, w, U_CLASS) < 0 || P(int32_t, w, U_CLASS) >= 8)
                continue;
            int hurt = (int16_t)unit_max_health(w, 0) - P(int16_t, w, 0xd6);
            uint8_t k = P(uint8_t, w, U_KIND), k2 = P(uint8_t, w, U_KIND2);
            if (((k == 0x1e && k2 == 0x1e) || (k == 0x1f && k2 == 0x1f)) && !P(uint8_t, w, 0x147) &&
                !P(uint8_t, w, 0x148)) {
                uint32_t a = P(uint8_t, w, U_FACING) * 3u + 0x2a;
                if (has_anim(w, a)) {
                    unit_set_anim(v, a, 1, 0);
                    if (most_hurt < hurt)
                        most_hurt = hurt;
                    if (hurt)
                        stirred = 1;
                }
            } else if (P(uint8_t, w, 0x147) > 0x29 && P(uint8_t, w, 0x147) < 0x40) {
                unit_set_anim(v, 0, 1, 0);
                g_kind0_reset(w);
            }
        }
    }
    if (stirred) {
        uint32_t a = P(uint8_t, u, U_FACING) * 3u + 0x2c;
        if (has_anim(u, a))
            unit_set_anim(unit_id_hi(u), a, 1, 0);
        seed(u);
        int wait = (orig_rand() % 5 + 5) * 0x10;
        int by_hurt = most_hurt < 1 ? 0 : (most_hurt / 2 + 1) * 0x10;
        P(int32_t, u, U_IDLE_WAIT) = wait <= by_hurt ? by_hurt : wait;
        return 1;
    }
    uint32_t a = P(uint8_t, u, U_FACING) * 3u + 0x29;
    if (has_anim(u, a))
        unit_set_anim(unit_id_hi(u), a, 1, 0);
    idle_wait(u);
    return 0;
}

/* 0x10011f48: the leader's wait (9-18 periods); its group's infantry stop their own
 * actions */
void __cdecl idle_leader_wait(uint8_t *u)
{
    seed(u);
    int r = orig_rand();
    P(uint32_t, u, U_IDLE_WAIT) = (uint32_t)((r % 10 + 9) * 0x10) + ((I(uint32_t, ISO_TICK) & 0xfffffff0) - I(uint32_t, ISO_TICK));
    if (!P(uint8_t, u, U_GROUP))
        return;
    int8_t group = P(int8_t, u, U_GROUP), player = P(int8_t, u, U_PLAYER);
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *w = I(uint8_t **, ISO_UNITS)[v];
        if (w && P(int8_t, w, U_GROUP) == group && P(int8_t, w, U_PLAYER) == player && P(int32_t, w, U_CLASS) >= 0 &&
            P(int32_t, w, U_CLASS) < 8 && P(uint8_t, w, 0x147) > 0x29 && P(uint8_t, w, 0x147) < 0x40) {
            unit_set_anim(v, 0, 1, 0);
            g_kind0_reset(w);
        }
    }
}
