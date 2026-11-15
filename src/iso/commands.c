/* The game logic, 0x1000ec9e-0x1000f2e9: stances and commands for the selected units
 * (IsoSetSelectedUnitState). */
#include "iso_core.h"
#include "sector.h"
#include "commands.h"

/* not rewritten yet (names ours, provisional) */
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define unit_cmd_3000 ORIG(void(__cdecl *)(uint32_t), 0x100dbe86)
#define unit_set_behaviour ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x100b6e66)
#define unit_fire ORIG(int(__cdecl *)(uint16_t), 0x10016fee)
#define net_unit_order ORIG(void(__cdecl *)(uint16_t, uint16_t, uint8_t), 0x10040706)

#define U_FLAGS2 0x10e
#define CRAWLING (1u << 0x11)
#define KNEELING (1u << 3)
#define PRONE (1u << 0x17)

/* infantry: types 0-7 and 0xc-0xf */
static int is_infantry(int32_t t) { return (t >= 0 && t <= 7) || (t >= 0xc && t <= 0xf); }

/* 0x1000ec9e: command `cmd` to a unit: 0 stand, 1 kneel, 2 crawl (infantry: through the
 * stance between when needed), 4 a vehicle's behaviour `value`, 3000 ...; 0 when it
 * doesn't apply */
int __stdcall unit_command(uint32_t id, uint32_t cmd, uint32_t value)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!(id & 0xffff) || !u || cmd >= 0xbb9)
        return 1;
    if (cmd == 3000) {
        unit_cmd_3000(id);
        return 1;
    }
    uint32_t f = P(uint32_t, u, U_FLAGS2);
    int32_t t = P(int32_t, u, U_ID_TYPE);
    switch (cmd) {
    case 0:
        if (!is_infantry(t))
            return 0;
        if (f & CRAWLING)
            unit_set_anim(id, 0xc, 1, 0);
        else if (f & KNEELING)
            unit_set_anim(id, 9, 1, 0);
        else if (f & PRONE)
            unit_set_anim(id, 0x4a, 1, 0);
        break;
    case 1:
        if (!is_infantry(t))
            return 0;
        if (f & CRAWLING)
            break;
        if (f & PRONE) {
            unit_set_anim(id, 0x4a, 1, 0);
            P(uint8_t, u, 0x14d) = 10;
        } else if (f & KNEELING) {
            unit_set_anim(id, 9, 1, 0);
            P(uint8_t, u, 0x14d) = 10;
        } else {
            unit_set_anim(id, 10, 1, 0);
        }
        break;
    case 2:
        if (!is_infantry(t))
            return 0;
        if (f & KNEELING)
            break;
        if (f & PRONE) {
            unit_set_anim(id, 0x4a, 1, 0);
            P(uint8_t, u, 0x14d) = 7;
        } else if (f & CRAWLING) {
            unit_set_anim(id, 0xc, 1, 0);
            P(uint8_t, u, 0x14d) = 7;
        } else {
            unit_set_anim(id, 7, 1, 0);
        }
        break;
    case 4: {
        int vehicle = t >= 0x40 && t <= 0x4f;
        if (t > 0x3f && !vehicle)
            return 0;
        if (vehicle || (!P(uint8_t, u, 0x147) && !P(uint8_t, u, 0x148)))
            unit_set_behaviour(id, value, !vehicle, 0);
        break;
    }
    }
    return 1;
}

/* 0x1000f0fc: the command to every selected unit of the local player (2000-2007: a
 * vehicle behaviour, command 4); returns how many took it */
int __stdcall IsoSetSelectedUnitState(uint8_t *iso, uint32_t state)
{
    g_iso = iso;
    /* original: the value's upper 24 bits are left over on the stack; only the low byte
     * (the behaviour) is meant */
    uint32_t value = 0;
    if (state > 1999 && state < 0x7d8) {
        value = state & 7;
        state = 4;
    }
    int n = 0;
    uint32_t local = I(uint32_t, ISO_LOCAL_PLAYER);
    for (int i = 0; i < SELECTED_N(local); i++) {
        uint16_t id = selected(local)[i];
        uint8_t *u = iso_unit(id);
        if (u && !(P(uint32_t, u, U_FLAGS) >> 0x19 & 1) && unit_command(id, state, value))
            n++;
    }
    return n;
}

/* 0x1000f207: a unit's fire button: deploying (types with 0xf3) or firing its weapon */
int __cdecl unit_fire_button(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!P(uint8_t, u, 0xf3) || P(uint8_t, u, 0x147)) {
        if (P(int16_t, u, 0xe6) >= 0)
            unit_fire((uint16_t)id);
    } else {
        unit_set_action(id, 0x3b, 1, 0);
        if (I(int, 0x175) && unit_is_local(id))
            net_unit_order(0, (uint16_t)id, 2);
    }
    return 1;
}
