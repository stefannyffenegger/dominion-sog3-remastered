/* The game logic, 0x100da072-0x100da582: a unit looking for a target in the sectors around
 * it (30-byte records at iso+0xc2eb: the first unit at +2, the neighbours from +6). */
#include "iso_core.h"
#include "groups.h"
#include "unitfacing.h"
#include "unitscan.h"
#include "weapons.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define SECTORS I(uint8_t *, 0xc2eb)
#define SECTOR_COUNT I(int32_t, 0xc2f1)
#define scan_group ORIG(int32_t(__cdecl *)(uint32_t, uint32_t, uint32_t, int32_t), 0x100da582)
#define set_target ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100dc75c)

/* whether unit `u` may and can attack unit `o` (`oid`); `*full` is set for a player of kind
 * 2 (+0x1b69), who also attacks structures */
static int may_attack(uint32_t id, uint8_t *u, uint32_t oid, uint8_t *o, int32_t *full)
{
    uint32_t player = P(uint8_t, u, 0x14);
    int32_t allied = I(uint8_t, 0x1ba0 + player * 0x1461) & (1u << (P(uint8_t, o, 0x14) & 0x1f));
    int visible = (P(uint8_t, o, 0x2f) & (1u << (player & 0x1f))) && P(int32_t, o, 0x84) != 1;
    *full = I(int32_t, 0x1b69 + player * 0x1461) == 2;
    int structure = P(uint16_t, o, 0x28) == 4 || (P(int32_t, o, 0xc) >= 0x40 && P(int32_t, o, 0xc) <= 0x43);
    int dying = P(uint8_t, o, 0x196) == 0x23;
    int32_t can = unit_can_attack(id, oid, 0, 0);
    if (P(int32_t, u, 0x10) >= 0x40 && P(int32_t, u, 0x10) <= 0x4f && (P(uint32_t, u, 0x3e) >> 12 & 1) &&
        (id & 0xffff) != oid) {
        allied = 0;
        *full = 1;
        can = 1;
    }
    return !allied && visible && (!structure || *full) && !dying && can && unit_weapon_reach(id, oid);
}

/* 0x100da072: unit `id` looks through `count` sectors around `sector` (0x11: the 9 near
 * ones and those one further out, along table 0x10159248) for a unit to attack, and targets
 * the first. With a target already, only with `flags` bit 0, and then only an armed unit in
 * place of an unarmed one. Grouped units and types 0x44-0x47 use 0x100da582. Returns the new
 * target, or 0; a mobile unit without one waits 2 ticks (+0x198). */
int32_t __cdecl unit_scan_targets(uint32_t id, uint32_t sector, uint32_t count, int32_t flags)
{
    int wide = 0;
    uint32_t n = count & 0xffff;
    if (n == 0x11) {
        wide = 1;
        n = 9;
    }
    uint8_t *u = UNIT(id & 0xffff);
    uint32_t cur = P(uint16_t, u, 0x17d);
    if (cur && !(flags & 1))
        return 0;
    if (unit_group_entry(id & 0xffff) || (P(int32_t, u, 0xc) >= 0x44 && P(int32_t, u, 0xc) <= 0x47))
        return scan_group(id, sector, wide ? 0x11 : 9, flags);
    for (int32_t i = 0; i < (int32_t)n; i++) {
        int16_t s = i ? P(int16_t, SECTORS, (int16_t)sector * 30 + i * 2 + 4) : (int16_t)sector;
        if (s <= 0 || SECTOR_COUNT <= s)
            continue;
        for (int32_t j = 0; j < (wide ? 2 : 1); j++) {
            if (j > 0)
                s = P(int16_t, SECTORS, G(int16_t, 0x10159248 + i * 2) * 2 + s * 30 + 6);
            if (s <= 0 || SECTOR_COUNT <= s)
                continue;
            uint16_t oid = P(uint16_t, SECTORS, s * 30 + 2);
            while (oid) {
                uint8_t *o = UNIT(oid);
                if (!o)
                    break;
                int32_t full;
                if (may_attack(id, u, oid, o, &full) &&
                    (!cur || (P(int32_t, P(uint8_t *, UNIT(cur), 0x32), 0x20) < 0 &&
                              P(int32_t, P(uint8_t *, o, 0x32), 0x20) >= 0 && oid != cur))) {
                    set_target(id, oid, 0, 1, 0);
                    P(uint16_t, u, 0x193) |= 0x100;
                    if (full)
                        P(uint16_t, u, 0x193) |= 1;
                    return oid;
                }
                oid = P(uint16_t, o, 0x393);
            }
        }
    }
    if (P(int32_t, u, 0xc) < 0x40)
        P(uint8_t, u, 0x198) = 2;
    return 0;
}
