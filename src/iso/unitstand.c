/* The game logic, 0x100db98e-0x100dbcbb: a unit ending its attack. */
#include "iso_core.h"
#include "groups.h"
#include "sector.h"
#include "unitorder.h"
#include "unitstand.h"
#include "waypoints.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define NET I(int32_t, 0x175)
#define S193(u) P(uint16_t, u, 0x193)
#define fn_1008a1e1 ORIG(void(__cdecl *)(uint32_t), 0x1008a1e1)
#define fn_100c6bd6 ORIG(int32_t(__cdecl *)(uint32_t, int32_t), 0x100c6bd6)
#define fn_100dcabd ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define fn_100dd5db ORIG(void(__cdecl *)(uint32_t), 0x100dd5db)

/* 0x100db98e: unit `id` stops attacking. Hovercraft use 0x1008a1e1. A structure of types
 * 0x50-0x87 that this unit was the attacker of forgets it (+0x18b); with `clear` the target
 * goes too. The aim, burst and flags are reset, the path resumes, and the unit goes idle, or
 * back to following (+0x193 bit 2) */
void __cdecl unit_end_attack(uint32_t id, int32_t clear)
{
    uint8_t *u = UNIT(id & 0xffff);
    if (!u)
        return;
    if (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37) {
        fn_1008a1e1(id);
        return;
    }
    uint8_t *t = UNIT(P(uint16_t, u, 0x17d));
    if (t && P(uint16_t, t, 0x18b) == (id & 0xffff) && P(int32_t, t, 0xc) >= 0x50 && P(int32_t, t, 0xc) <= 0x87)
        P(uint16_t, t, 0x18b) = 0;
    if (clear)
        fn_100dcabd(id, 0);
    P(uint16_t, u, 0x183) = 0;
    P(uint16_t, u, 0x185) = 0;
    S193(u) &= 0xfffe;
    P(uint32_t, u, 0x10e) &= 0xfffffffe;
    S193(u) &= 0xffef;
    S193(u) &= 0xfeff;
    P(uint8_t, u, 0x195) &= 0xf9;
    P(uint8_t, u, 0x177) = 0xff;
    P(uint32_t, u, 0x3e) &= 0xff7fffff;
    if (P(int16_t, u, 0xe6) >= 0)
        P(uint16_t, u, 0xea) = (uint16_t)P(int32_t, P(uint8_t *, I(uint8_t *, 0x1990), 0),
                                           P(int32_t, I(uint8_t *, 0x1990), 0x2c) * P(int16_t, u, 0xe6) * 4 + 0x58);
    unit_path_suspend(id, 0, 0);
    if ((!NET || unit_is_local_u(u)) && P(int32_t, P(uint8_t *, u, 0x32), 0x74) == 1 &&
        (P(uint32_t, u, 0x10e) >> 23 & 1))
        unit_order(id, 0x4a, 1, 0);
    if (P(int32_t, u, 0x10) >= 0xc && P(int32_t, u, 0x10) <= 0xf)
        group_attack(u, 0);
    unit_set_action(id, (S193(u) >> 2 & 1) ? 0x1f : 0x1e, 1, 0);
    if (P(int32_t, u, 0xc) >= 0x2c && P(int32_t, u, 0xc) <= 0x2f && (!NET || unit_is_local(id)))
        fn_100c6bd6(id, 3);
    fn_100dd5db(id & 0xffff);
}
