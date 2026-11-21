/* The game logic: IsoAddUnitType (0x100b2d8c-0x100b31e0), setting a unit record up as its
 * type: its type record (+0x32, from the types table iso+0x197c), its sprite (a copy of the
 * unit sprite iso+0xc33b at +0, its object at +4, the type's animation at +8), the unit
 * list, and its type's setup handler (0x101545b8). */
#include "iso_core.h"
#include "unitlists.h"
#include "unittype.h"

#define FAIL G(uint8_t, 0x1017df58)
#define net_unit_owner ORIG(void(__stdcall *)(uint8_t *, uint32_t, uint32_t, uint32_t), 0x100b1a78)
#define building_footprint_set ORIG(void(__cdecl *)(uint8_t *, uint32_t), 0x100f3c43)
typedef void(__cdecl *setup_fn)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t);

/* `fresh` 0 re-applies a unit's type (keeping its place, look and level); 0, or 0x20 without
 * a record, or a negative code (the reason in 0x1017df58) */
uint32_t __stdcall IsoAddUnitTypeC(uint8_t *iso, int32_t fresh, uint32_t id, uint32_t player, uint32_t kind,
                                   uint32_t a6, int32_t x, int32_t y, uint32_t look, uint32_t a10, uint32_t level,
                                   uint32_t a12, uint32_t flags)
{
    (void)a10;
    (void)a12;
    g_iso = iso;
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0x20;
    if (!fresh) {
        x = P(int16_t, u, 0x52);
        y = P(int16_t, u, 0x54);
        look = P(uint8_t, u, 0x79);
        level = P(uint8_t, u, 0x30);
        flags = 0;
    } else {
        P(int32_t, u, 0x18) = -1;
    }
    P(uint32_t, u, 0xc) = kind;
    P(uint32_t, u, 0x10) = kind & 0xfffffffc;
    P(uint8_t *, u, 0x32) = P(uint8_t *, I(uint8_t *, 0x197c), 0) + P(int32_t, I(uint8_t *, 0x197c), 0x2c) * P(int32_t, u, 0xc) * 4;
    int32_t anim = P(int32_t, P(uint8_t *, u, 0x32), 0);
    if (anim < 1 || !RtLoad(I(void *, ISO_APP), (uint32_t)anim)) {
        FAIL = 4;
        return 0xffffffb0;
    }
    if (!I(uint32_t, 0xc33b) || !RtLoad(I(void *, ISO_APP), I(uint32_t, 0xc33b))) {
        FAIL = 5;
        return 0xffffffb0;
    }
    if (!I(int32_t, ISO_HAVE_SPRITES) && P(uint32_t, u, 0))
        RtDelete(I(void *, ISO_APP), P(uint32_t, u, 0));
    P(uint32_t, u, 0) = RtCopy(I(void *, ISO_APP), I(uint32_t, 0xc33b));
    RtLoad(I(void *, ISO_APP), P(uint32_t, u, 0));
    P(void *, u, 8) = ((void **)RtGetResource(I(void *, ISO_APP), (uint32_t)anim))[1];
    P(void *, u, 4) = ((void **)RtGetResource(I(void *, ISO_APP), P(uint32_t, u, 0)))[1];
    unsigned type = RtGetResType(I(void *, ISO_APP), P(uint32_t, u, 0));
    if (!P(uint32_t, u, 0) || !P(void *, u, 4) || !P(void *, u, 8) || type != 0x14) {
        FAIL = 6;
        return 0xffffffa6;
    }
    I(uint32_t, 0x1858) = P(uint32_t, u, 0);
    if (I(int32_t, 0x175))
        net_unit_owner(g_iso, id & 0xffff, I(uint32_t, player * ISO_PLAYER_SIZE + 0x1ba2), I(uint8_t, player * ISO_PLAYER_SIZE + 0x1b9f));
    if (I(int32_t, ISO_HAVE_SPRITES) && (flags & 1) && (int32_t)kind < 0x40) {
        /* a unit built in the main plant is paired with it while it comes out */
        P(uint16_t, u, 0x9a) = I(uint16_t, 0xd6cb + player * 2);
        P(uint16_t, I(uint8_t **, ISO_UNITS)[I(uint16_t, 0xd6cb + player * 2)], 0x9a) = (uint16_t)id;
    }
    if (fresh) {
        unit_list_add(id);
        I(int32_t, 0xd65a + player * 4)++;
    }
    P(uint8_t, u, 0xf1) = !(flags & 1);
    setup_fn setup = G(setup_fn, 0x101545b8 + kind * 0x44);
    if (setup)
        setup(id, a6, player, x, y, look, level, flags & 2);
    if (P(int32_t, u, 0x10) > 0x3f)
        building_footprint_set(u, fresh == 0);
    return 0;
}
