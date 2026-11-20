/* The game logic, 0x100a3ef9-0x100a43cd: the players' build slots (iso+0x2ca + player*0xc4
 * + slot*0x31, four per player: +0 the unit kind or -1, +4 its time, +0x1c a flag for the
 * interface), and the men and materials counters' setup. */
#include "iso_core.h"
#include "debugtime.h"
#include "buildinfo.h"
#include "buildqueue.h"

#define slot_interface_update ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x100a48d2)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)

/* 0x100a3ef9: cancels the player's build slot of unit kind index `kind` (slots 2-3 only when
 * started, +4): the interface (local player) or the AI is told, and the slot cleared */
void __cdecl build_slot_cancel(uint8_t player, int32_t kind)
{
    uint8_t s;
    int32_t *slot = 0;
    for (s = 0; s < 4; s++) {
        slot = (int32_t *)(g_iso + s * 0x31 + player * 0xc4 + 0x2ca);
        if (slot[0] > -1 && (s < 2 || slot[1]) && slot[0] >> 2 == kind)
            break;
    }
    if (s >= 4)
        return;
    for (int k = 0; k < 10; k++)
        iso_debug_print();
    if (I(uint8_t, 0x1b65) == player) {
        if (slot[7])
            slot_interface_update(s, 2, player);
    } else if (!I(int32_t, 0x175)) {
        ai_event(player, 2, (uint32_t)slot[0], 0);
    }
    memset(g_iso + s * 0x31 + player * 0xc4 + 0x2ca, 0, 0x31);
    slot[0] = -1;
}

/* IsoUpdateMMString */
void __stdcall IsoUpdateMMStringC(uint8_t *iso)
{
    g_iso = iso;
    counters_update();
}

/* IsoInitMenMatStrings: the script variables, sprites and positions of the counters */
void __stdcall IsoInitMenMatStringsC(uint8_t *iso, uint32_t men_var, uint32_t mat_var, uint32_t men_sprite,
                                     uint32_t men_pos, uint32_t mat_sprite, uint32_t mat_pos, uint32_t font,
                                     uint32_t c34a, uint32_t c346, uint32_t c342)
{
    g_iso = iso;
    I(uint32_t, 0x1ab6) = men_var;
    I(uint32_t, 0x1aaa) = mat_var;
    I(uint32_t, 0x1ab2) = men_sprite;
    I(uint32_t, 0x1aae) = men_pos;
    I(uint32_t, 0x1aa6) = mat_sprite;
    I(uint32_t, 0x1aa2) = mat_pos;
    I(uint32_t, 0x17bd) = font;
    I(uint32_t, 0xc346) = c346;
    I(uint32_t, 0xc342) = c342;
    I(uint32_t, 0xc34a) = c34a;
}
