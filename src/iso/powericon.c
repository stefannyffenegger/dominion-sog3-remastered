/* The game logic, 0x100f306f-0x100f33a0: a unit's power tick and its "no power" icon. */
#include "iso_core.h"
#include "power.h"
#include "powericon.h"

#define HSL I(uint32_t, 0x181e)
#define sprite_send ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48bc)
#define sprite_post ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, void *), 0x100f48ce)
#define sprite_level ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t), 0x100f48d4)
#define sprite_process ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t), 0x100f48da)

/* 0x100f306f */
void __cdecl power_tick_of(uint32_t id)
{
    power_unit_tick(id);
}

/* the icon's sprite, placed over the unit, blinking between frame 2 and nothing */
static void icon_start(uint8_t *u)
{
    uint32_t s = I(uint32_t, P(int16_t, u, 0x4c) * 14 + 0xcf5c);
    sprite_process(HSL, s, 1);
    G(int32_t, 0x1015727c) = P(int16_t, u, 0x52);
    G(int32_t, 0x10157280) = P(int16_t, u, 0x54) + 0x12;
    sprite_send(HSL, s, 4, ORIG_PTR(0x10157278));
    sprite_level(HSL, s, P(int16_t, u, 0x66) + 0x64);
    sprite_send(HSL, s, 1, ORIG_PTR(0x10157334));
    G(int32_t, 0x101572c4) = 2;
    G(int32_t, 0x101572c8) = 2;
    G(int32_t, 0x101572cc) = 0;
    sprite_post(HSL, s, 4, ORIG_PTR(0x101572c0));
    G(int32_t, 0x1015734c) = 0xfa;
    sprite_post(HSL, s, 2, ORIG_PTR(0x10157348));
    sprite_post(HSL, s, 1, ORIG_PTR(0x10157324));
    sprite_post(HSL, s, 2, ORIG_PTR(0x10157348));
    sprite_post(HSL, s, 4, ORIG_PTR(0x10157338));
}

/* 0x100f308b: keeps the icon (+0x4c) in step with the unit's flag 0x1000 at +0x3e. Returns
 * 1 while the flag is set */
int32_t __cdecl unit_power_icon(uint32_t id)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    if (!(P(uint32_t, u, 0x3e) >> 12 & 1)) {
        if (P(int16_t, u, 0x4c) > -1) {
            power_icon_hide(P(uint16_t, u, 0x4c));
            P(uint16_t, u, 0x4c) = 0xffff;
        }
        return 0;
    }
    if (P(int16_t, u, 0x4c) == -1) {
        P(uint16_t, u, 0x4c) = (uint16_t)power_icon_alloc();
        if (P(int16_t, u, 0x4c) > -1)
            icon_start(u);
    }
    return 1;
}
