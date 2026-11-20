/* The game logic, 0x100a25d0-0x100a3036: the men and materials counters on the interface,
 * a building's men production, and a player's build level per unit kind (iso+0x182 +
 * player*0x29 + kind/4: the best upgrade level of that kind the player has, which decides
 * what it can build). */
#include "iso_core.h"
#include "screen.h"
#include "vecscore.h"
#include "intfwave.h"
#include "status.h"
#include "debugtime.h"
#include "buildinfo.h"

#define bf_draw_text_shadow                                                                        \
    ORIG(int(__cdecl *)(void *, uint32_t, int32_t, int32_t, int, int, int, uint32_t, int, int), 0x101120bc)
#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))

static void counter(uint32_t var, uint32_t value, uint32_t fmt, uint32_t pos)
{
    wsprintfA(RtGetVariableString(I(void *, ISO_APP), I(uint32_t, var)), (const char *)ORIG_PTR(fmt), value);
    bf_draw_text_shadow(I(void *, 0x1812), I(uint32_t, pos), I(int32_t, 0x17bd), -1, 0, 0, 0x23, I(uint32_t, var), 0, 0);
}

/* 0x100a25d0: writes the local player's men (+0x1b97) and materials (+0x1b9b) into their
 * counters and has both counter sprites redrawn. 0 if a sprite is missing */
int32_t __cdecl counters_update(void)
{
    int32_t p = I(int32_t, 0x1b65);
    counter(0x1ab6, I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b97), 0x10153bd4, 0x1aae);
    counter(0x1aaa, I(uint32_t, I(int32_t, 0x1b65) * ISO_PLAYER_SIZE + 0x1b9b), 0x10153bdc, 0x1aa2);
    void *a = ((void **)RtGetResource(I(void *, ISO_APP), I(uint32_t, 0x1ab2)))[1];
    void *b = ((void **)RtGetResource(I(void *, ISO_APP), I(uint32_t, 0x1aa6)))[1];
    if (!a || !b)
        return 0;
    MSG[1] = MSG[2] = MSG[3] = 0;
    SpriteSendMessage(I(void *, 0x181e), a, 4, MSG);
    SpriteSendMessage(I(void *, 0x181e), b, 4, MSG);
    return 1;
}

/* 0x100a27f1: updates the player's build level for unit kind `kind`: 0 sets it, 1 raises it
 * (units of kind 0x7c are also counted at +0x2e01), 2 recounts it from the player's units
 * (kind 0x7c is uncounted; for the local player, losing the last of kind 0x54 hides the
 * radar). Then
 * the interface options are updated */
void __cdecl build_level_update(uint32_t kind, uint8_t player, uint8_t level, uint8_t how)
{
    DebugStr((const char *)ORIG_PTR(0x10153be4), kind, (uint32_t)player, (uint32_t)level, (uint32_t)how);
    uint8_t *slot = &I(uint8_t, player * 0x29 + (kind >> 2) + 0x182);
    if (how == 0) {
        *slot = level;
    } else if (how == 1) {
        if (*slot < level)
            *slot = level;
        if (kind == 0x7c)
            I(int32_t, player * ISO_PLAYER_SIZE + 0x2e01)++;
    } else if (how == 2) {
        uint8_t best = 0;
        for (int32_t k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
            uint8_t *u = I(uint8_t **, ISO_UNITS)[I(int32_t *, ISO_UNIT_LIST)[k]];
            if (u && P(uint8_t, u, 0x14) == player && P(uint32_t, u, 0x10) == kind && best < P(uint8_t, u, 0x30))
                best = P(uint8_t, u, 0x30);
        }
        *slot = best;
        if (kind == 0x54 && (uint32_t)player == I(uint32_t, 0x1b65) && !I(uint8_t, player * 0x29 + 0x197) &&
            I(int32_t, 300))
            radar_show(0);
        if (kind == 0x7c)
            I(int32_t, player * ISO_PLAYER_SIZE + 0x2e01)--;
    }
    int32_t rc = IsoUpdateIntfOptionsC(g_iso, player);
    DebugStr((const char *)ORIG_PTR(0x10153c2c), rc);
}

/* 0x100a2ab0: sends the script message 4 to the interface (iso+0x182e); RtRunMsg's result */
int32_t __cdecl interface_message4(void)
{
    uint8_t msg[0x22];
    memset(msg, 0, sizeof msg);
    *(uint32_t *)(msg + 2) = I(uint32_t, 0x182e);
    *(uint32_t *)(msg + 0xa) = 4;
    msg[9] = 1;
    return RtRunMsg(I(void *, ISO_APP), msg);
}

#define production ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x100a36d6)
#define TYPE_VALUE(u, off) P(uint32_t, P(uint8_t *, I(uint8_t *, 0x18ec), 0), (off) + P(int32_t, I(uint8_t *, 0x18ec), 0x2c) * P(int32_t, u, 0xc) * 4)

/* 0x100a2d71: a building's men production, when its time (+0xa0) has come and it is
 * working (+0x42): its output (type value +0x18, through 0x100a36d6; four times from upgrade
 * level 2) is added to the player's men, up to its limit (type value +0x34 times +0x2e01)
 * and at most 9999, and scored. The next time is set from type value +0x30. 1 if it
 * produced */
int32_t __cdecl building_produce_men(uint32_t unit)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (P(uint32_t, u, 0xa0) >= I(uint32_t, 0x17dd) || !P(int16_t, u, 0x42))
        return 0;
    uint8_t p = P(uint8_t, u, 0x14);
    uint32_t n = production(unit, TYPE_VALUE(u, 0x18));
    if (P(uint8_t, u, 0x30) > 1)
        n <<= 2;
    uint32_t limit = TYPE_VALUE(u, 0x34) * I(uint32_t, p * ISO_PLAYER_SIZE + 0x2e01);
    uint32_t *men = &I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b97);
    if (*men < limit) {
        *men += n;
        score_add_men(p, (int32_t)n);
        if (limit < *men)
            *men = limit;
        if (*men > 9999)
            *men = 9999;
    }
    P(uint32_t, u, 0xa0) = TYPE_VALUE(u, 0x30) + I(uint32_t, 0x17dd);
    if (I(int32_t, p * ISO_PLAYER_SIZE + 0x1b69) == 1 && I(uint32_t, 0x1b65) == p)
        I(uint8_t, 0xd696) = 1;
    return 1;
}

#define intf_button_off ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100a3ef9)
#define player_find_1008bd1f ORIG(uint32_t(__cdecl *)(uint32_t, int32_t *, int32_t *, int), 0x1008bd1f)
#define player_message ORIG(void(__cdecl *)(uint32_t, const char *, int), 0x1003ba11)
#define TABLE_ROW(off, k) (P(int32_t *, I(uint8_t *, off), 0) + P(int32_t, I(uint8_t *, off), 0x2c) * (k))

/* IsoUpdateIntfOptions: which of the 40 build buttons the player may use. A button (table
 * 0x18dc: -0x29a for none, +8 its unit kind) is on when the kind is allowed for the player's
 * side (table 0x1968 by +0x1baa), the player has fewer than 250 units, and every technology
 * it needs (table 0x18e4, up to 10, kind+1000 for upgrade level 2) is at the level needed.
 * Kind 0x14 is on without its needs when the player has none yet; kinds 0x30-0x37 are off
 * unless 0x1008bd1f finds something for the player; transports (0x88-0x9f) are off when +0x2dd6 is set. For the local
 * player each button's state (iso+0x1b61: 5 off, 10 on) is set and newly available
 * buildings (40) and units (41) announced; buttons turned off are passed to 0x100a3ef9 */
int32_t __stdcall IsoUpdateIntfOptionsC(uint8_t *iso, uint32_t player)
{
    int full = I(uint32_t, player * ISO_PLAYER_SIZE + 0x1b8b) >= 0xfa; /* read before g_iso is set */
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10153c7c), player);
    int new_building = 0, new_unit = 0;
    for (int32_t b = 0; b < 0x28; b++) {
        int on = 0;
        int32_t kind;
        if (TABLE_ROW(0x18dc, b)[0] == -0x29a) {
            kind = -1;
        } else {
            kind = TABLE_ROW(0x18dc, b)[2] >> 2;
            on = 1;
            if (!TABLE_ROW(0x1968, kind)[I(uint8_t, player * ISO_PLAYER_SIZE + 0x1baa)] || full) {
                on = 0;
            } else {
                for (int k = 0; k < 10; k++) {
                    int32_t need = TABLE_ROW(0x18e4, kind)[k];
                    if (need == -1)
                        break;
                    uint8_t level = 1;
                    if (need >= 1000) {
                        need -= 1000;
                        level = 2;
                    }
                    need >>= 2;
                    if (I(uint8_t, player * 0x29 + need + 0x182) < level) {
                        on = 0;
                        break;
                    }
                }
                if (!on && ((uint32_t)kind & 0x3fffffff) == 0x14 && !I(uint8_t, player * 0x29 + kind + 0x182))
                    on = 1;
            }
        }
        if (on && kind * 4 >= 0x30 && kind * 4 <= 0x37) {
            int32_t a, c;
            if (!(player_find_1008bd1f(player, &a, &c, 0) & 0xffff)) {
                iso_debug_print();
                on = 0;
            }
        }
        if (on && kind * 4 >= 0x88 && kind * 4 <= 0x9f && I(uint8_t, player * ISO_PLAYER_SIZE + 0x2dd6))
            on = 0;
        if (I(uint32_t, 0x1b65) == player) {
            int32_t *state = I(int32_t *, 0x1b61) + b;
            if (on) {
                if (*state == 5) {
                    if (kind * 4 < 0x40)
                        new_unit = 1;
                    else
                        new_building = 1;
                }
                if (*state == 5 || *state == 0x14)
                    *state = 10;
            } else {
                *state = 5;
                intf_button_off(player, (uint32_t)kind);
            }
        } else if (!on) {
            intf_button_off(player, (uint32_t)kind);
        }
    }
    if (I(uint32_t, 0x1b65) != player)
        return 1;
    if (I(int32_t, 0x17f5) != 1) {
        if (new_building)
            player_announce(player, 0x28, 1);
        if (new_unit)
            player_announce(player, 0x29, 1);
    }
    if (full) {
        player_message(player, (const char *)ORIG_PTR(0x10153cd4), 0);
        status_text((const char *)ORIG_PTR(0x10153cf4), 0);
    }
    return interface_message4();
}
