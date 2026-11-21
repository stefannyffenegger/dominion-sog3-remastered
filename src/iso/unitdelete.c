/* The game logic: IsoDeleteUnit (0x100ab4aa-0x100ac51d), removing a unit from the game:
 * everything that refers to it is cleared, its effects shown (a death sound and blast for
 * units of types with one), its sprites, objects and memory freed, and the counts, the AI
 * and the scores told. */
#include "iso_core.h"
#include "debugtime.h"
#include "power.h"
#include "view.h"
#include "unit.h"
#include "sector.h"
#include "sound.h"
#include "events.h"
#include "radarmap.h"
#include "players.h"
#include "eject.h"
#include "combat.h"
#include "relay.h"
#include "misc.h"
#include "weapons.h"
#include "groups.h"
#include "buildinfo.h"
#include "unitlists.h"
#include "unitdelete.h"

#define unit_free_links ORIG(void(__cdecl *)(uint32_t), 0x100dd5db)
#define plant_lost ORIG(void(__cdecl *)(uint32_t, int), 0x100bd586)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, uint32_t), 0x100e42a6)
#define unit_stop_targeting ORIG(void(__cdecl *)(uint32_t, int), 0x100dcabd)
#define IsoDeleteObject ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t), 0x100adbde)
#define IsoUpdateFootprint ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t, int, int32_t), 0x100ef91a)
#define transport_release ORIG(void(__cdecl *)(uint32_t, int, int), 0x1008c129)
#define unit_forget_1 ORIG(void(__cdecl *)(uint32_t), 0x100e9c85)
#define unit_forget_2 ORIG(void(__cdecl *)(uint32_t), 0x100e9e85)
#define unit_forget_3 ORIG(void(__cdecl *)(uint32_t, int, int, int), 0x100d559a)
#define unit_forget_4 ORIG(void(__cdecl *)(uint32_t, int), 0x100b56aa)
#define turret_free ORIG(void(__cdecl *)(uint32_t), 0x10085ec1)
#define player_unit_lost ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100b1e2c)
#define player_check_lost ORIG(void(__cdecl *)(uint32_t), 0x100b2340)
#define KILLED(p) G(int32_t, 0x1017df88 + (p) * 4)
typedef void(__cdecl *kind_fn2)(uint32_t, int);
typedef void(__cdecl *kind_fn1)(uint32_t);

static uint8_t *U(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id]; }

int32_t __stdcall IsoDeleteUnitC(uint8_t *iso, uint32_t id, int32_t mark)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x101579cc), id, mark);
    iso_debug_print();
    unit_free_links(id);
    uint32_t slot = 0xffffffff;
    int32_t refresh = 0, small = 0;
    RECT r;
    uint8_t *u = U(id);
    int32_t was_seen = !(P(uint32_t, u, 0x3e) >> 13 & 1);
    int32_t building = P(uint8_t, u, 0x196) == '#';
    u = U(id);
    I(uint32_t, 0xd67e) = id;
    I(uint32_t, 0x1ad1) = unit_id_free() & 0xffff;
    if (!u)
        return 0;
    uint32_t p = P(uint8_t, u, 0x14);
    int out = I(uint16_t, p * ISO_PLAYER_SIZE + 0x2fc7) >> 8 & 1;
    if (I(int32_t, ISO_HAVE_SPRITES) && (!I(int32_t, 0x175) || I(int32_t, 0x1b17) > 1) && !out)
        unit_eject(id, mark);
    if (P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x9f && !(I(uint16_t, P(uint8_t, u, 0x14) * ISO_PLAYER_SIZE + 0x2fc7) >> 8 & 1)) {
        /* the player lost a building of a kind it had */
        if (P(uint8_t, u, 0x30) > 1) {
            if (P(int32_t, P(uint8_t *, u, 0x32), 0xc))
                KILLED(P(uint8_t, u, 0x14)) = 1;
        } else if (P(int32_t, P(uint8_t *, u, 0x32), 0x2c)) {
            KILLED(P(uint8_t, u, 0x14)) = 1;
        }
    }
    if (P(uint32_t, u, 0x3e) >> 12 & 1) {
        iso_debug_print();
        plant_lost(id, 1);
    }
    if (!I(int32_t, 0x175) && I(int32_t, ISO_HAVE_SPRITES)) {
        iso_debug_print();
        ai_event(P(uint8_t, u, 0x14), 1, id, ~(P(uint32_t, u, 0x3e) >> 17) & 1);
    }
    if (P(uint16_t, u, 0x9a) && (P(int32_t, u, 0xc) > 0x4f || P(int32_t, u, 0xc) < 0x40)) {
        if (U(P(uint16_t, u, 0x9a)))
            P(uint16_t, U(P(uint16_t, u, 0x9a)), 0x9a) = 0;
        P(uint16_t, u, 0x9a) = 0;
    }
    if (P(uint16_t, u, 0x17d) && U(P(uint16_t, u, 0x17d)) && P(uint16_t, U(P(uint16_t, u, 0x17d)), 0x9a) == id)
        P(uint16_t, U(P(uint16_t, u, 0x17d)), 0x9a) = 0;
    if (P(int32_t, u, 0xc) >= 0x40 && P(int32_t, u, 0xc) <= 0x9f) {
        if (P(int16_t, u, 0x4a) > -1 && P(int32_t, u, 0x10) != 0x50) {
            power_icon_hide(P(uint16_t, u, 0x4a));
            P(int16_t, u, 0x4a) = -1;
        }
        if (P(int16_t, u, 0x4c) > -1) {
            iso_debug_print();
            power_icon_hide(P(uint16_t, u, 0x4c));
            P(int16_t, u, 0x4c) = -1;
        }
        if (P(int16_t, u, 0x4e) > -1) {
            power_icon_hide(P(uint16_t, u, 0x4e));
            P(int16_t, u, 0x4e) = -1;
        }
        if (P(int16_t, u, 0x50) > -1) {
            power_icon_hide(P(uint16_t, u, 0x50));
            P(int16_t, u, 0x50) = -1;
        }
    }
    if ((int32_t)G(int16_t, 0x10118d84) == (int32_t)id)
        ring_hide(G(int16_t, 0x10118d84));
    for (int32_t k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        int32_t o = I(int32_t *, ISO_UNIT_LIST)[k];
        uint8_t *ou = I(uint8_t **, ISO_UNITS)[o];
        if (o <= 0 || !ou || (uint32_t)o == id)
            continue;
        if (P(uint16_t, ou, 0x17d) == id)
            unit_stop_targeting((uint32_t)o, 0);
        if (P(uint16_t, ou, 0x18b) == id)
            P(uint16_t, ou, 0x18b) = 0;
        if (P(uint16_t, ou, 0xf4) == id)
            P(uint16_t, ou, 0xf4) = 0;
    }
    unit_stop_targeting(id, 0);
    unit_groups_drop((int32_t)id);
    player_replace_d6cb(id, P(uint8_t, u, 0x14));
    player_replace_d6f9(id, P(uint8_t, u, 0x14));
    uint8_t *t = P(uint8_t *, u, 0x32);
    if (I(int32_t, ISO_HAVE_SPRITES) && !(P(uint32_t, u, 0x3e) >> 17 & 1) && P(int32_t, t, 0x6c) > 0 && P(int32_t, t, 0x74) < 4) {
        if (P(int32_t, u, 0xc) >= 0 && P(int32_t, u, 0xc) < 0x10)
            small = 1;
        if (P(uint8_t, u, 0x147) == ' ' || P(uint8_t, u, 0x147) == '!') {
            unit_voice(u, P(uint8_t, u, 0x147) == '!' ? 6 : 2, 0, 0);
            slot = effect_slot_take(0, 0);
            int32_t x = P(int16_t, u, 0x52), y = P(int16_t, u, 0x54);
            if ((int32_t)slot >= 0 && (int32_t)slot < I(uint16_t, 0xd6e7)) {
                if (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37) {
                    /* a turret's blast is raised by its height */
                    x = P(int16_t, u, 0x52);
                    int32_t sy = P(int16_t, u, 0x54);
                    int16_t h = (int16_t)(int32_t)(int64_t)(long double)P(float, P(uint8_t *, u, 0x39d), 0x20);
                    y = sy - h;
                }
                effect_start(P(void *, I(uint8_t *, 0xd6e9), 6 + slot * 0xe), P(int16_t, u, 0x66) + 1, x, y,
                             P(uint32_t, t, 0x68), P(int32_t, t, 0x6c), 0, 0, 0, 0x30, 0);
            }
            if (I(int32_t, 0x120) && P(int32_t, u, 0xc) >= 0x18 && P(int32_t, u, 0xc) < 0x40) {
                slot = effect_slot_take(0, 0);
                if ((int32_t)slot >= 0 && (int32_t)slot < I(uint16_t, 0xd6e7))
                    effect_start(P(void *, I(uint8_t *, 0xd6e9), 6 + slot * 0xe), P(int16_t, u, 0x66) - 1, x, y, 0x6d7,
                                 0xf, 0, 0, 0, 0x30, 0x194);
            }
        }
    }
    if (P(int32_t, u, 0x10) == 0x5c && I(int32_t, ISO_HAVE_SPRITES)) {
        if (P(void *, u, 0x102)) {
            SpriteHide(I(void *, 0x181e), P(void *, u, 0x102), 1);
            RtDelete(I(void *, ISO_APP), P(uint32_t, u, 0xfe));
            P(uint32_t, u, 0xfe) = 0;
            P(void *, u, 0x102) = 0;
        }
        uint8_t *w = g_iso + P(int32_t, u, 0xfa) * 0x2b + 0x1097;
        P(int32_t, w, 0x27) = -1;
        radar_plot_object(w, 0, 1);
    }
    SpriteHide(I(void *, 0x181e), P(void *, u, 4), 1);
    if (P(uint8_t *, u, 0x39d)) {
        SpriteHide(I(void *, 0x181e), P(void *, P(uint8_t *, u, 0x39d), 4), 1);
        RtDelete(I(void *, ISO_APP), P(uint32_t, P(uint8_t *, u, 0x39d), 0));
        P(uint32_t, P(uint8_t *, u, 0x39d), 0) = 0;
        P(uint32_t, P(uint8_t *, u, 0x39d), 4) = 0;
    }
    if (P(void *, u, 0x36)) {
        SpriteSetExtOffsets(I(void *, 0x181e), P(void *, u, 4), 0);
        winfree(P(void *, u, 0x36));
        P(void *, u, 0x36) = 0;
    }
    if (!RtDelete(I(void *, ISO_APP), P(uint32_t, u, 0)))
        DebugStr((const char *)ORIG_PTR(0x10157aa4));
    P(uint32_t, u, 0) = 0;
    P(uint32_t, u, 4) = 0;
    if (I(int32_t, ISO_HAVE_SPRITES))
        unit_effects_clear((uint16_t)id);
    if (P(int32_t, u, 0x10) >= 0x40 && P(int32_t, u, 0x10) <= 0x9f) {
        iso_debug_print();
        if (P(int32_t, u, 0x18) != -1) {
            uint32_t m = P(uint32_t, u, 0x18);
            void *s = ((void **)RtGetResource(I(void *, ISO_APP), P(uint32_t, I(uint8_t *, 0x192c), m * 0x12)))[1];
            if (s) {
                SpriteGetBounds(I(void *, 0x181e), s, &r);
                r.left -= I(int32_t, 0xcd82) + 0x20;
                r.top -= I(int32_t, 0xcd86) + 0x10;
                r.right -= I(int32_t, 0xcd82) - 0x20;
                r.bottom -= I(int32_t, 0xcd86) - 0x10;
                refresh = 1;
            }
            IsoDeleteObject(g_iso, m);
            iso_debug_print();
            P(int32_t, u, 0x18) = -1;
        }
    }
    unit_sector_update(id, 2, 1);
    if (P(int32_t, t, 0x74) > 3 || ((P(int32_t, u, 0xc) == 0x39 || P(int32_t, u, 0xc) == 0x3b) && !P(int16_t, u, 0x66)))
        IsoUpdateFootprint(g_iso, id, 0, mark);
    else if (mark)
        ground_mark(P(int16_t, u, 0x52), P(int16_t, u, 0x54), id, small);
    if (G(kind_fn2, 0x101545ec + P(int32_t, u, 0xc) * 0x44))
        G(kind_fn2, 0x101545ec + P(int32_t, u, 0xc) * 0x44)(id, 2);
    unit_list_remove(id);
    selection_remove(id, P(uint8_t, u, 0x14), 0);
    radar_plot(P(uint16_t, u, 0x98), 0, 0);
    I(int32_t, 0xd65a + P(uint8_t, u, 0x14) * 4)--;
    if (G(kind_fn1, 0x101545e0 + P(int32_t, u, 0xc) * 0x44))
        G(kind_fn1, 0x101545e0 + P(int32_t, u, 0xc) * 0x44)(id);
    if ((P(int32_t, u, 0xc) >= 0x60 && P(int32_t, u, 0xc) <= 0x63) || (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37)) {
        if (P(int32_t, u, 0xc) >= 0x30 && P(int32_t, u, 0xc) <= 0x37 && P(uint16_t, u, 0x108))
            transport_release(P(uint16_t, u, 0x1cb), 7, 0);
        if (P(uint16_t, u, 0x108) && U(P(uint16_t, u, 0x108))) {
            uint8_t *o = U(P(uint16_t, u, 0x108));
            P(uint16_t, u, 0x108) = 0;
            P(uint16_t, o, 0x108) = 0;
        }
        P(uint16_t, u, 0x108) = 0;
    }
    if (I(int32_t, ISO_HAVE_SPRITES) && I(int32_t, 0x17f5) == 4)
        build_level_update(P(uint32_t, u, 0x10), P(uint8_t, u, 0x14), 1, 2);
    uint32_t owner = P(uint8_t, u, 0x14);
    uint8_t ev = P(uint8_t, u, 0x2b);
    uint8_t kind = (uint8_t)P(uint32_t, u, 0xc);
    unit_forget_1(id);
    unit_forget_2(id);
    unit_forget_3(id, -1, 1, 0);
    unit_forget_4(id, 1);
    if (P(void *, u, 0x39d)) {
        turret_free(id);
        P(void *, u, 0x39d) = 0;
    }
    for (int k = 0; k < 2; k++)
        if (P(void *, U(id), 0x90 + k * 4)) {
            winfree(P(void *, U(id), 0x90 + k * 4));
            P(void *, U(id), 0x90 + k * 4) = 0;
        }
    if (P(void *, U(id), 0x173))
        winfree(P(void *, U(id), 0x173));
    winfree(U(id));
    I(uint8_t **, ISO_UNITS)[id] = 0;
    if (refresh)
        SpriteRefreshRect(I(void *, 0x181e), &r);
    if (ev && was_seen && I(int32_t, ISO_HAVE_SPRITES))
        event_count(ev);
    if (building) {
        I(int32_t, owner * ISO_PLAYER_SIZE + 0x1b8f)--;
    } else {
        I(int32_t, owner * ISO_PLAYER_SIZE + 0x1b8b)--;
        /* the original passes the kind byte with a stack leftover above it */
        player_unit_lost(owner, kind);
    }
    if (!KILLED(owner) && (!I(int32_t, 0x175) || !I(int32_t, 0x164)))
        player_check_lost(owner);
    return 1;
}

/* IsoDeleteAllUnits: every unit's sprites, objects and memory freed (without the effects and
 * counts of IsoDeleteUnit), the selections cleared, and every map marker sent its last
 * message and deleted */
int32_t __stdcall IsoDeleteAllUnitsC(uint8_t *iso)
{
    g_iso = iso;
    for (int p = 0; p < 8; p++) {
        I(uint32_t, 0xd65a + p * 4) = 0;
        selection_clear(p);
    }
    for (int k = 0; k < 2000; k++) {
        uint32_t id = I(uint32_t *, ISO_UNIT_LIST)[k];
        if ((int32_t)id > 0 && U(id)) {
            if (P(int32_t, U(id), 0x10) == 0x5c && I(int32_t, ISO_HAVE_SPRITES) && P(void *, U(id), 0x102)) {
                SpriteHide(I(void *, 0x181e), P(void *, U(id), 0x102), 1);
                RtDelete(I(void *, ISO_APP), P(uint32_t, U(id), 0xfe));
                P(uint32_t, U(id), 0xfe) = 0;
                P(void *, U(id), 0x102) = 0;
            }
            SpriteHide(I(void *, 0x181e), P(void *, U(id), 4), 1);
            if (P(uint8_t *, U(id), 0x39d)) {
                SpriteHide(I(void *, 0x181e), P(void *, P(uint8_t *, U(id), 0x39d), 4), 1);
                RtDelete(I(void *, ISO_APP), P(uint32_t, P(uint8_t *, U(id), 0x39d), 0));
                P(uint32_t, P(uint8_t *, U(id), 0x39d), 0) = 0;
                P(uint32_t, P(uint8_t *, U(id), 0x39d), 4) = 0;
            }
            if (P(void *, U(id), 0x36)) {
                SpriteSetExtOffsets(I(void *, 0x181e), P(void *, U(id), 4), 0);
                winfree(P(void *, U(id), 0x36));
                P(void *, U(id), 0x36) = 0;
            }
            if (!RtDelete(I(void *, ISO_APP), P(uint32_t, U(id), 0)))
                DebugStr((const char *)ORIG_PTR(0x10157b0c));
            unit_sector_update(id, 2, 0);
            if (P(void *, U(id), 0x39d)) {
                turret_free(id);
                P(void *, U(id), 0x39d) = 0;
            }
            for (int j = 0; j < 2; j++)
                if (P(void *, U(id), 0x90 + j * 4)) {
                    winfree(P(void *, U(id), 0x90 + j * 4));
                    P(void *, U(id), 0x90 + j * 4) = 0;
                }
            winfree(U(id));
            I(uint8_t **, ISO_UNITS)[id] = 0;
        }
        I(uint32_t *, ISO_UNIT_LIST)[k] = 0;
    }
    I(uint32_t, ISO_UNIT_COUNT) = 0;
    if (I(uint8_t *, 0x192c))
        for (int k = 0; k < 0x800; k++) {
            int32_t res = P(int32_t, I(uint8_t *, 0x192c), k * 0x12);
            if (res <= 0)
                continue;
            P(int32_t, I(uint8_t *, 0x192c), k * 0x12) = 0;
            if (((void **)RtGetResource(I(void *, ISO_APP), (uint32_t)res))[1]) {
                SpriteSendMessage(I(void *, 0x181e), ((void **)RtGetResource(I(void *, ISO_APP), (uint32_t)res))[1], 1,
                                  (const uint32_t *)ORIG_PTR(0x10157324));
                if (!RtDelete(I(void *, ISO_APP), (uint32_t)res))
                    DebugStr((const char *)ORIG_PTR(0x10157b20));
            }
        }
    I(uint32_t, 0x1b4d) = 0;
    return 1;
}
