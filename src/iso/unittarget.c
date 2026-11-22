/* The game logic, 0x100dc75c-0x100dcabd: a unit being given a target. */
#include "iso_core.h"
#include "netflags.h"
#include "sector.h"
#include "unitorder.h"
#include "unitstand.h"
#include "unittarget.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define S193(u) P(uint16_t, u, 0x193)
#define TYPE(u) P(int32_t, u, 0xc)
#define IS_HOVER(u) (TYPE(u) >= 0x30 && TYPE(u) <= 0x37)
#define fn_1008a1e1 ORIG(void(__cdecl *)(uint32_t), 0x1008a1e1)
#define fn_100dcabd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define net_40c27 ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x10040c27)

/* 0x100dc75c: unit `id` gets target `tid`, attacking after `delay` ticks (in a network game
 * after the network delay as well). An old target is dropped first. Types 0x10-0x13 work on
 * structures instead (state 0x41; +0x3e bit 12 for an enemy's). Nothing while in state 0x10. */
void __cdecl unit_attack_target(uint32_t id, uint32_t tid, int32_t delay, uint32_t flag)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (u && P(uint8_t, u, 0x196) == 0x10)
        return;
    if (P(uint16_t, u, 0x17d) && P(uint16_t, u, 0x17d) != (id & 0xffff)) {
        if (IS_HOVER(u))
            fn_1008a1e1(id);
        else
            unit_end_attack(id, !NET);
    }
    P(uint16_t, u, 0x181) = (uint16_t)((tid & 0xffff) + (id & 0xffff));
    P(int32_t, u, 0xc2) = 0;
    S193(u) &= 0xffef;
    S193(u) = (uint16_t)((S193(u) & 0xfbff) | ((flag & 1) << 10));
    if (TYPE(u) >= 0x10 && TYPE(u) <= 0x13) {
        if (P(uint32_t, u, 0x10e) >> 4 & 1)
            return;
        fn_100dcabd(id, tid);
        uint8_t *t = UNIT(tid & 0xffff);
        if (TYPE(t) >= 0x40 && TYPE(t) <= 0x9f) {
            uint32_t mine = P(uint8_t, u, 0x14), its = P(uint8_t, t, 0x14);
            if (its != mine && !(I(uint8_t, 0x1ba0 + mine * 0x1461) & (1u << (its & 0x1f))))
                P(uint32_t, u, 0x3e) |= 0x1000;
            else
                P(uint32_t, u, 0x3e) &= 0xffffefff;
            unit_set_action(id, 0x41, 1, 0);
        }
        return;
    }
    if (!NET || IS_HOVER(u))
        fn_100dcabd(id, tid);
    unit_set_action(id, 0xb, 1, 0);
    if (NET && unit_is_local(id)) {
        int32_t wait = net_delay();
        P(uint32_t, u, 0x199) = I(uint32_t, 0x17dd) + wait + delay;
        if (IS_HOVER(u)) {
            net_40c27(id, tid, 0);
        } else {
            P(uint16_t, u, 0x17f) = (uint16_t)tid;
            S193(u) |= 0x800;
        }
    } else {
        P(uint32_t, u, 0x199) = I(uint32_t, 0x17dd) + delay;
    }
}
