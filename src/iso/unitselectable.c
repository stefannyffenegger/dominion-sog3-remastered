/* The game logic, 0x100f4167-0x100f4301: whether a unit can be selected. */
#include "iso_core.h"
#include "unitselectable.h"

#define wall_chain_head ORIG(uint16_t(__cdecl *)(uint32_t), 0x100ee92a)

/* a mobile unit (below type 0x40) always, a structure once it stands (poses 1-3) */
static int standing(uint8_t *u)
{
    uint8_t pose = P(uint8_t, u, 0x147);
    return P(int32_t, u, 0xc) < 0x40 || pose == 1 || pose == 2 || pose == 3;
}

/* 0x100f4167: whether unit `id` can be selected. A wall piece must be linked (+0xf6 or
 * +0xf8), and every piece of its chain must stand */
int32_t __cdecl unit_selectable(uint32_t id)
{
    uint8_t **units = I(uint8_t **, ISO_UNITS);
    uint8_t *u = units[id & 0xffff];
    if (!u)
        return 0;
    if (P(int32_t, u, 0xc) < 0x88 || P(int32_t, u, 0xc) > 0x9f)
        return standing(u);
    if (!P(uint16_t, u, 0xf6) && !P(uint16_t, u, 0xf8))
        return 0;
    for (uint16_t w = wall_chain_head(id); w; w = P(uint16_t, u, 0xf6)) {
        u = I(uint8_t **, ISO_UNITS)[w];
        if (!u || !standing(u))
            return 0;
    }
    return 1;
}
