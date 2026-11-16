/* The game logic, 0x10016aa0-0x1001782a: the action buttons of the interface
 * (IsoProcessAction) and the check state of the toggle buttons. */
#include "iso_core.h"
#include "commands.h"
#include "relay.h"
#include "sector.h"
#include "buttons.h"

/* not rewritten yet (names ours, provisional) */
#define unit_locked ORIG(int(__cdecl *)(uint32_t), 0x10042ef3)
#define status_text ORIG(int(__cdecl *)(const char *, int), 0x10053ef5)
#define player_announce ORIG(void(__cdecl *)(uint32_t, int, int), 0x1005810f)
#define unit_anim_start ORIG(void(__cdecl *)(uint16_t, uint32_t, int, int), 0x100f365b)
#define unit_set_repair ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x1005af3f)
#define unit_auto_repair_anim ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100eea00)
#define unit_sell ORIG(int(__cdecl *)(uint32_t, int), 0x1005ae71)
#define unit_repair ORIG(void(__cdecl *)(uint32_t, int), 0x1005b007)
#define unit_1005b314 ORIG(int(__cdecl *)(uint32_t, int), 0x1005b314)
#define unit_10076dbb ORIG(void(__cdecl *)(uint32_t), 0x10076dbb)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define net_unit_order ORIG(void(__cdecl *)(uint16_t, uint16_t, uint8_t), 0x10040706)
#define player_unset_d6cb ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1005ab95)
#define player_unset_d6f9 ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1005acf2)
#define unit_set_d6cb ORIG(void(__cdecl *)(uint32_t, int, uint32_t), 0x100e90da)
#define unit_set_d6f9 ORIG(void(__cdecl *)(uint32_t, int), 0x100e8f48)
#define unit_set_flag1a ORIG(void(__cdecl *)(uint32_t, int, uint32_t), 0x100e8d86)
#define player_100bfc73 ORIG(void(__cdecl *)(int), 0x100bfc73)
#define unit_100a6903 ORIG(void(__cdecl *)(uint8_t, uint16_t), 0x100a6903)

#define IsoActivateSelectionMode ORIG(void(__cdecl *)(uint8_t *, int, uint32_t, uint32_t), 0x10073acc)
#define IsoDetonateSelection ORIG(void(__cdecl *)(uint8_t *), 0x1005972f)
#define IsoTogglePlayerColorMode ORIG(void(__cdecl *)(uint8_t *), 0x10070c3b)
#define IsoAutoRetreat ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x100a0e7c)
#define IsoDeleteCommandPoint ORIG(int(__stdcall *)(uint8_t *, uint32_t, uint32_t), 0x100a0996)
#define IsoGotoCommandPoint ORIG(int(__stdcall *)(uint8_t *, uint32_t, uint32_t), 0x100a0b9d)
#define IsoSendToCommandPoint ORIG(int(__stdcall *)(uint8_t *, uint32_t, uint32_t), 0x100a0c88)
#define IsoToggleCommanderThumper ORIG(int(__stdcall *)(uint8_t *, uint32_t), 0x10095809)

/* per unit type: what its "special" button does (entry 13 of the type's 17) */
typedef void(__cdecl *SpecialHandler)(uint32_t id);
#define unit_special_handler(type) G(SpecialHandler, 0x101545f4 + (type) * 0x44)

#define ISO_FOCUS_UNIT 0xdac6 /* short: the unit the interface shows */
#define ISO_WEAPONS 0x1990    /* weapon table: +0 rows, +0x2c row width in ints */
#define U_WEAPON 0xe6         /* short: index into the weapon table, or -1 */
#define U_RING_SLOT 0x9e      /* short */

#define F_REPAIRING (1u << 0)
#define F_AUTO_REPAIR (1u << 1)
#define F_THUMPER (1u << 0x14)
#define F_1A (1u << 0x1a)
#define F_1B (1u << 0x1b)

/* 0x10016aa0: a toggle button (actions 0x30-0x37) for the unit the interface shows:
 * with `apply` toggles it; returns whether it is on. The original also logs through
 * its debug printf (compiled out: 0x10095e98 returns at once), left out here. */
uint32_t __cdecl iso_check_state(uint32_t action, int apply)
{
    uint16_t id = I(uint16_t, ISO_FOCUS_UNIT);
    uint8_t *u = iso_unit(id);
    uint32_t player = I(uint32_t, ISO_LOCAL_PLAYER);
    switch (action) {
    case 0x30:
        if (u)
            return 0;
        break;
    case 0x31: /* automatic repair */
        if (!u)
            break;
        if (apply) {
            P(uint32_t, u, U_FLAGS) ^= F_AUTO_REPAIR;
            if (!(P(uint32_t, u, U_FLAGS) & F_AUTO_REPAIR)) {
                P(uint32_t, u, U_FLAGS) &= ~F_REPAIRING;
                unit_anim_start(id, (P(uint32_t, u, U_FLAGS) >> 1) & 1, 0, 1);
                player_announce(P(uint8_t, u, U_PLAYER), 0x56, 1);
            } else {
                player_announce(P(uint8_t, u, U_PLAYER), 0x37, 1);
            }
            unit_set_repair(id, P(uint32_t, u, U_FLAGS) & 1, (P(uint32_t, u, U_FLAGS) >> 1) & 1);
            if (P(int32_t, u, U_CLASS) >= 0x88 && P(int32_t, u, U_CLASS) <= 0x9f)
                unit_auto_repair_anim(id, (P(uint32_t, u, U_FLAGS) >> 1) & 1);
        }
        return (P(uint32_t, u, U_FLAGS) >> 1) & 1;
    case 0x32: /* the commander's thumper */
        if (!u)
            break;
        if (apply) {
            IsoToggleCommanderThumper(g_iso, player);
            player_announce(P(uint8_t, u, U_PLAYER), (P(uint32_t, u, U_FLAGS) & F_THUMPER) ? 0x60 : 0x61, 1);
        }
        return (P(uint32_t, u, U_FLAGS) >> 0x14) & 1;
    case 0x33:
        if (!u)
            break;
        if (!apply)
            return I(uint16_t, 0xd6cb + player * 2) == id;
        if (I(uint16_t, 0xd6cb + player * 2) != id) {
            unit_set_d6cb(id, 1, 0xffffffffu);
            player_announce(P(uint8_t, u, U_PLAYER), 0x5c, 1);
            P(uint32_t, u, U_FLAGS) &= ~F_1B;
            return 1;
        }
        player_unset_d6cb(id, player);
        return 0;
    case 0x34:
        if (!u)
            break;
        if (!apply)
            return I(uint16_t, 0xd6f9 + player * 2) == id;
        if (I(uint16_t, 0xd6f9 + player * 2) != id) {
            unit_set_d6f9(id, 1);
            player_announce(P(uint8_t, u, U_PLAYER), 0x5d, 1);
            return 1;
        }
        player_unset_d6f9(id, player);
        return 0;
    case 0x35:
        if (!u)
            break;
        if (apply) {
            unit_set_flag1a(id, 1, ((P(uint32_t, u, U_FLAGS) >> 0x1a) & 1) ^ 1);
            if (P(uint32_t, u, U_FLAGS) & F_1A) {
                player_announce(P(uint8_t, u, U_PLAYER), 0x5a, 1);
                P(uint32_t, u, U_FLAGS) &= ~F_1B;
            }
        }
        return (P(uint32_t, u, U_FLAGS) >> 0x1a) & 1;
    case 0x36:
        if (!u)
            break;
        if (apply) {
            P(uint32_t, u, U_FLAGS) ^= F_1B;
            if (P(uint32_t, u, U_FLAGS) & F_1B) {
                player_announce(P(uint8_t, u, U_PLAYER), 0x59, 1);
                unit_set_flag1a(id, 1, 0);
            }
        }
        return (P(uint32_t, u, U_FLAGS) >> 0x1b) & 1;
    case 0x37: /* player colours */
        if (apply)
            IsoTogglePlayerColorMode(g_iso);
        return I(uint8_t, 0xe098) == 0;
    }
    return 0;
}

/* 0x10016fee: the fire button of one unit: a relay shows its ring; another unit
 * shows its weapon's range as a ring. 0 when it has no weapon or no slot is free. */
int __cdecl unit_show_range(uint16_t id)
{
    uint8_t *u = iso_unit(id);
    int32_t cls = P(int32_t, u, U_CLASS);
    if (cls >= 0x84 && cls <= 0x87) {
        relay_selected((int16_t)id);
        return 1;
    }
    int16_t slot = player_slot_alloc((uint8_t)I(uint32_t, ISO_LOCAL_PLAYER));
    int16_t weapon = P(int16_t, u, U_WEAPON);
    if (slot == -1 || weapon == -1)
        return 0;
    int32_t *w = I(int32_t *, ISO_WEAPONS);
    int32_t range = ((int32_t *)(uintptr_t)w[0])[w[0xb] * weapon + 1];
    P(int16_t, u, U_RING_SLOT) = slot;
    ring_show((int16_t)id, 0xfffffffeu, (int16_t)range);
    return 1;
}

/* 0x10017124: an action button `action` of the interface, for the local player's
 * selected units */
void __stdcall IsoProcessAction(uint8_t *iso, int action)
{
    int announce = 0;
    g_iso = iso;
    if (action >= 0x30) {
        iso_check_state((uint32_t)action, 1);
        return;
    }
    uint32_t player = I(uint32_t, ISO_LOCAL_PLAYER);
    switch (action) {
    case 0:
        IsoDeleteCommandPoint(g_iso, player, 0);
        IsoActivateSelectionMode(g_iso, 0x42e, I(uint32_t, ISO_LOCAL_PLAYER), 0);
        break;
    case 1:
    case 2:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x14:
    case 0x17:
    case 0x18:
    case 0x1b:
        for (int16_t i = 0; i < SELECTED_N(I(uint32_t, ISO_LOCAL_PLAYER)); i++) {
            uint16_t id = selected(I(uint32_t, ISO_LOCAL_PLAYER))[i];
            uint8_t *u = iso_unit(id);
            if (!id || id >= 2000 || !u || unit_locked(id))
                continue;
            if (action == 0x18) {
                if (P(uint8_t, u, 0xf3) && !P(uint8_t, u, 0x147)) {
                    unit_set_action(id, 0x3b, 1, 0);
                    if (I(int, 0x175) && unit_is_local(id))
                        net_unit_order(0, id, 2);
                }
            } else if (action == 0x14 || action == 0x17) {
                SpecialHandler h = unit_special_handler(P(int32_t, u, U_ID_TYPE));
                if (h)
                    h(id);
            } else if (action == 0x11) {
                unit_repair(id, 1);
                announce = 0x22;
            } else if (action == 0x12) {
                unit_1005b314(id, 1);
            } else if (action == 0x10) {
                unit_sell(id, 1);
            } else if (action == 0x1b) {
                if (P(int32_t, u, 0x84) == 1) {
                    unit_10076dbb(id);
                    announce = 0x5e;
                }
            } else { /* 1, 2: automatic repair on, off */
                unit_set_repair(id, P(uint32_t, u, U_FLAGS) & 1, action == 1);
                announce = action == 1 ? 0x37 : 0x56;
                if (P(int32_t, u, U_CLASS) >= 0x88 && P(int32_t, u, U_CLASS) <= 0x9f)
                    unit_auto_repair_anim(id, (P(uint32_t, u, U_FLAGS) >> 1) & 1);
            }
        }
        if (announce)
            player_announce(I(uint32_t, ISO_LOCAL_PLAYER), announce, 1);
        break;
    case 3:
        IsoActivateSelectionMode(iso, 0x424, 0, 0);
        break;
    case 4:
        IsoDetonateSelection(iso);
        break;
    case 5:
        IsoActivateSelectionMode(iso, 0x3fc, 0, 0);
        break;
    case 6:
        IsoActivateSelectionMode(iso, 0x3f2, 0, 0);
        break;
    case 7:
        IsoActivateSelectionMode(iso, 0x3fc, 0, 1);
        break;
    case 8:
        IsoSetSelectedUnitState(iso, 2);
        break;
    case 9:
        IsoSetSelectedUnitState(iso, 1);
        break;
    case 10:
        IsoSetSelectedUnitState(iso, 0);
        break;
    case 0xb:
        if (SELECTED_N(player) == 1) {
            uint16_t id = selected(player)[0];
            if (id && id < 2000 && iso_unit(id))
                unit_show_range(id);
        }
        break;
    case 0xc:
        if (I(int32_t, 0x2db3 + player * ISO_PLAYER_SIZE))
            IsoSendToCommandPoint(iso, player, 0);
        break;
    case 0xd:
        IsoGotoCommandPoint(iso, player, 0);
        break;
    case 0xe:
        IsoDeleteCommandPoint(iso, player, 0);
        status_text(ORIG_PTR(0x1013061c), 2000);
        break;
    case 0xf:
        IsoDeleteCommandPoint(iso, player, 1);
        IsoActivateSelectionMode(g_iso, 0x438, I(uint32_t, ISO_LOCAL_PLAYER), 1);
        break;
    case 0x13:
        IsoAutoRetreat(iso, player);
        break;
    case 0x15:
    case 0x16:
        IsoActivateSelectionMode(iso, 0x442, action == 0x16, 0);
        break;
    case 0x19:
        break;
    case 0x1a:
    case 0x1d:
        IsoSetSelectedUnitState(iso, 3000);
        break;
    case 0x1c:
        player_100bfc73((int)player);
        break;
    case 0x1e:
        IsoGotoCommandPoint(iso, player, 1);
        break;
    case 0x1f:
        IsoDeleteCommandPoint(iso, player, 1);
        status_text(ORIG_PTR(0x10130638), 2000);
        break;
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
        unit_100a6903((uint8_t)(action - 0x20), I(uint16_t, ISO_FOCUS_UNIT));
        break;
    }
}
