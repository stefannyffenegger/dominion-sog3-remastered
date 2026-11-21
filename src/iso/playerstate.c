/* The game logic, 0x100b1dcb-0x100b2953: a player's unit count (+0x1b93, without kinds
 * 0x84-0x87 and transports), the defeat check, and the objects linked into the sector table
 * (each sector's first object at +4, the objects' links at +0xe/+0x10). */
#include "iso_core.h"
#include "debugtime.h"
#include "status.h"
#include "playerstate.h"

#define OBJ(k) (I(uint8_t *, 0x192c) + (k) * 0x12)
#define game_over ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100562e2)
#define player_units_free ORIG(void(__cdecl *)(uint32_t), 0x1007f73e)
#define players_left ORIG(uint32_t(__cdecl *)(void), 0x100b1e8d)
#define net_send_defeat ORIG(void(__cdecl *)(uint32_t, int, int), 0x100350a3)
#define net_player_out ORIG(void(__cdecl *)(uint32_t, int, uint32_t), 0x100422eb)
#define object_unlink ORIG(void(__cdecl *)(uint8_t *, uint32_t), 0x100b2715)

static int counted(uint8_t kind) { return (kind < 0x84 || kind > 0x87) && kind < 0x88; }

/* 0x100b1dcb, 0x100b1e2c: a unit of `kind` built / lost by the player */
void __cdecl player_unit_built(uint32_t p, uint8_t kind)
{
    if (counted(kind))
        I(int32_t, p * ISO_PLAYER_SIZE + 0x1b93)++;
}
void __cdecl player_unit_lost(uint32_t p, uint8_t kind)
{
    if (counted(kind))
        I(int32_t, p * ISO_PLAYER_SIZE + 0x1b93)--;
}

/* 0x100b2340: a player without units (or, with iso+0x158, without any) is defeated: in a
 * single player game the game ends (lost for the local player, won for the others); in a
 * network game the host announces it, frees the player's units and tells the others */
void __cdecl player_check_lost(uint8_t p)
{
    if (I(int32_t, 0x150) && (uint32_t)p != I(uint32_t, 0x1b65))
        return;
    int32_t n = I(int32_t, p * ISO_PLAYER_SIZE + 0x1b93);
    if (I(int32_t, 0x158))
        n = I(int32_t, p * ISO_PLAYER_SIZE + 0x1b8b);
    if (!I(int32_t, ISO_HAVE_SPRITES) || n)
        return;
    if (!I(int32_t, 0x175)) {
        game_over((uint32_t)p != I(uint32_t, 0x1b65), 0);
        return;
    }
    if (!I(int32_t, 0x148))
        return;
    for (uint8_t k = 0; k < 8; k++)
        iso_debug_print();
    char text[0x50];
    wsprintfA(text, (const char *)ORIG_PTR(0x10158124), g_iso + p * ISO_PLAYER_SIZE + 0x1b6d);
    status_text(text, 0);
    iso_debug_print();
    iso_debug_print();
    iso_debug_print();
    I(uint16_t, p * ISO_PLAYER_SIZE + 0x2fc7) |= 0x100;
    iso_debug_print();
    player_units_free(p);
    uint32_t left = players_left();
    iso_debug_print();
    net_send_defeat(p, 1, 0);
    net_player_out(p, I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b93) != 0, left);
    if ((uint32_t)p == I(uint32_t, 0x1b65))
        game_over(0, left);
}

/* 0x100b2883: puts object `o` first in the sector list */
void __cdecl object_link(uint8_t *o, uint32_t sector)
{
    uint8_t *s = I(uint8_t *, 0xc2eb) + (sector & 0xffff) * 0x1e;
    int16_t first = P(int16_t, s, 4);
    iso_debug_print();
    P(int16_t, o, 0xe) = -1;
    P(int16_t, o, 0x10) = P(int16_t, o, 0xe);
    if (first == -1) {
        P(int16_t, s, 4) = P(int16_t, o, 0xc);
    } else {
        P(int16_t, s, 4) = P(int16_t, o, 0xc);
        P(int16_t, OBJ(first), 0x10) = P(int16_t, o, 0xc);
        P(int16_t, o, 0xe) = first;
    }
}

/* 0x100b2623: links object k into (or with `how` 2 out of) the list of its sector */
void __cdecl object_sector(uint32_t k, int16_t how)
{
    uint8_t *o = OBJ(k & 0xffff);
    uint32_t s = (uint32_t)(((P(int16_t, o, 6) + I(int32_t, 0xd71e)) >> 8) << (I(uint8_t, 0xc2f5) & 0x1f)) +
                 (uint32_t)((P(int16_t, o, 4) + I(int32_t, 0xd709)) >> 8);
    if ((int32_t)s < 0 || (int32_t)s >= I(int32_t, 0xc2f1))
        return;
    if (how == 2)
        object_unlink(o, s);
    else
        object_link(o, s);
}
