/* The game logic, 0x10059be0-0x1005a56d: what comes out of a destroyed unit: its
 * passengers (a vehicle with seats), or the survivors a structure leaves (its type's
 * list of up to four kinds and counts). */
#include "iso_core.h"
#include "sector.h"
#include "combat.h"
#include "eject.h"

/* not rewritten yet (names ours, provisional) */
#define unit_place_out ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1005a56d)
#define unit_set_target ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x100dcabd)
#define unit_face ORIG(void(__cdecl *)(uint8_t *, char), 0x100a2200)
#define action_set ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009cc60)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cc91)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, uint8_t), 0x1009ce9f)
#define unit_set_action ORIG(void(__cdecl *)(uint32_t, uint32_t, int, int), 0x1009cf5b)
#define score_unit_lost ORIG(void(__cdecl *)(uint32_t, uint8_t, uint8_t), 0x1007e870)
#define building_exit ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x10040c27)
#define unit_name ORIG(const char *(__cdecl *)(int32_t, void *), 0x1009ddd0)
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define unit_dir_from ORIG(void(__cdecl *)(uint32_t, uint32_t, int32_t *), 0x100d9cb5)
#define ai_player_hold ORIG(void(__cdecl *)(uint32_t, int), 0x100e48de)
#define ai_group_done ORIG(void(__cdecl *)(uint32_t), 0x100e4969)
#define IsoAddUnit                                                                                     \
    ORIG(int(__stdcall *)(uint8_t *, uint32_t, uint32_t, uint32_t, int32_t, int32_t, uint32_t, int, int, int), \
         0x100aa140)
#define IsoRequestAddUnit                                                                              \
    ORIG(int(__stdcall *)(uint8_t *, uint32_t, uint32_t, uint32_t, int32_t, int32_t, uint32_t, int, int, int), \
         0x10042d4f)
#define debug_printf ORIG(void(__cdecl *)(int, const char *, ...), 0x10095e98)
#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)

#define ALLIES(p) I(uint8_t, (p) * 0x1461 + 0x1ba0)

/* 0x10059be0: 1 when something came out (the second argument is tested by the
 * original, but either way the same code follows) */
int32_t __cdecl unit_eject(uint32_t id, int32_t unused)
{
    (void)unused;
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint32_t hi16 = (uint32_t)(uintptr_t)u & 0xffff0000;
    int32_t rc = 0;
    if (!u)
        return 0;
    if (P(int32_t, P(uint8_t *, u, 0x32), 0xe0) && P(uint8_t, u, 0xf3)) {
        /* the passengers: put out around the vehicle, in turning directions */
        int16_t p = P(int16_t, iso_unit((int)(id & 0xffff)), 0xf6);
        uint8_t dir = (uint8_t)(orig_rand() & 7);
        if (p > 0 && iso_unit(p))
            rc = 1;
        uint8_t *v;
        while (p > 0 && iso_unit(p) && (v = iso_unit(p))) {
            int32_t t = P(int32_t, u, U_ID_TYPE);
            if (t < 0x30 || t > 0x37 || P(int32_t, P(uint8_t *, u, 0x39d), 0xc4) == 10) {
                P(int16_t, v, U_X) = P(int16_t, iso_unit((int)(id & 0xffff)), U_X);
                P(int16_t, v, U_Y) = P(int16_t, iso_unit((int)(id & 0xffff)), U_Y);
                if (!I(int32_t, 0x175) || unit_is_local(id))
                    unit_place_out((uint16_t)p, dir, 0, 0);
                P(int16_t, v, 0x9a) = 0;
                unit_set_target((uint16_t)p, 0);
                P(int16_t, v, 0x18b) = 0;
                P(int16_t, v, 0x183) = 0;
                P(int16_t, v, 0x185) = 0;
                unit_face(v, 0);
                P(int32_t, v, 0x56) = 0;
                P(uint32_t, v, 0x10e) &= 0xffff7fff;
                if (P(int32_t, u, 0x56) && (I(uint8_t *, 0x1934)[P(int32_t, u, 0x56) * 4] & 1)) {
                    /* no room: it dies */
                    unit_kill((uint32_t)(int32_t)p, 0x20, 1, 0);
                    if (!(P(uint32_t, u, U_FLAGS) >> 0x1e & 1)) {
                        score_unit_lost(P(uint32_t, u, 0x10), P(uint8_t, u, U_PLAYER), P(uint8_t, u, U_PLAYER));
                        P(uint32_t, u, U_FLAGS) |= 0x40000000;
                    }
                } else {
                    action_set((uint16_t)p, 0);
                    unit_set_anim((uint16_t)p, 0, 1, 0);
                    unit_set_mode((uint16_t)p, 0x1e);
                    unit_set_action((uint16_t)p, 0x1e, 1, 0);
                    P(int16_t, v, 0xf4) = 0;
                    uint8_t *killer = iso_unit(P(uint16_t, u, 0x18b));
                    if (killer && !((1 << (P(uint8_t, v, U_PLAYER) & 0x1f)) & ALLIES(P(uint8_t, killer, U_PLAYER)))) {
                        unit_set_target((uint16_t)p, hi16 | P(uint16_t, u, 0x18b));
                        if (P(uint16_t, v, 0x17d) && I(int32_t, 0x175))
                            building_exit((uint16_t)p, ((uint32_t)(uintptr_t)v & 0xffff0000) | P(uint16_t, v, 0x17d), 2);
                    }
                }
                p = P(int16_t, v, 0xf6);
                dir = dir < 7 ? (uint8_t)(dir + 1) : 0;
            } else {
                /* in a turret: they die with it */
                if (!I(int32_t, 0x175) || I(int32_t, 0x148)) {
                    unit_kill((uint32_t)(int32_t)p, 0x20, 1, 1);
                    if (!(P(uint32_t, u, U_FLAGS) >> 0x1e & 1)) {
                        score_unit_lost(P(uint32_t, u, 0x10), P(uint8_t, u, U_PLAYER), P(uint8_t, u, U_PLAYER));
                        P(uint32_t, u, U_FLAGS) |= 0x40000000;
                    }
                }
                p = P(int16_t, v, 0xf6);
            }
        }
        return rc;
    }
    int32_t t = P(int32_t, u, U_ID_TYPE);
    if (t < 0x40 || t > 0x87 || (t >= 0x84 && t <= 0x87) || (I(int32_t, 0x175) && !I(int32_t, 0x148)))
        return 0;
    if (P(int32_t, u, 0x10) == 0x5c)
        return 0;
    /* the survivors of a structure */
    uint8_t name[0x1c];
    debug_printf(0x10, (const char *)ORIG_PTR(0x1013eef8), P(uint8_t, u, U_PLAYER),
                 id & 0xffff, unit_name(P(int32_t, u, 0x10) / 4, name));
    uint8_t dir = (uint8_t)(orig_rand() & 7);
    unit_max_health(u, 0);
    uint32_t player = P(uint8_t, u, U_PLAYER);
    int32_t x = P(int16_t, u, U_X), y = P(int16_t, u, U_Y), px = x, py = y;
    int32_t cell = IsoVTilePos(g_iso, &px, &py, &x, &y);
    uint32_t old = I(uint32_t *, 0x1934)[cell];
    uint32_t list = P(uint8_t, u, 0x30) > 1 ? P(uint32_t, P(uint8_t *, u, 0x32), 0xc)
                                            : P(uint32_t, P(uint8_t *, u, 0x32), 0x2c);
    if (list)
        rc = 1;
    for (int k = 0; k < 4; k++) {
        uint8_t count = list & 0xf;
        list >>= 4;
        uint8_t kind = list & 0xf;
        list >>= 4;
        uint8_t *killer = iso_unit(P(uint16_t, u, 0x18b));
        for (uint8_t n = 0; n < count; n++) {
            if (killer) {
                int32_t d;
                unit_dir_from(hi16 | P(uint16_t, u, 0x18b), id, &d);
                if (d >= 0)
                    dir = (uint8_t)d;
            } else {
                dir = (uint8_t)(orig_rand() & 7);
            }
            uint32_t type = I(uint8_t, P(uint8_t, u, U_PLAYER) * 0x1461 + 0x1b9f) + G(uint32_t, 0x1013d708 + kind * 4);
            int16_t s;
            if (I(int32_t, 0x175)) {
                s = (int16_t)IsoRequestAddUnit(g_iso, P(uint8_t, u, U_PLAYER), type, P(uint8_t, u, 0x16),
                                               P(int16_t, u, U_X), P(int16_t, u, U_Y), dir, 0, 0, 4);
            } else {
                ai_player_hold(P(uint8_t, u, U_PLAYER), 1);
                s = (int16_t)IsoAddUnit(g_iso, P(uint8_t, u, U_PLAYER), type, P(uint8_t, u, 0x16),
                                        P(int16_t, u, U_X), P(int16_t, u, U_Y), dir, 0, 0, 4);
            }
            uint8_t *w = iso_unit(s);
            if (w) {
                P(uint32_t, w, 0x10e) |= 0x40;
                P(uint32_t, w, U_FLAGS) &= 0xfffbffff;
                if (killer && !((1 << (P(uint8_t, w, U_PLAYER) & 0x1f)) & ALLIES(P(uint8_t, killer, U_PLAYER)))) {
                    unit_set_target((uint16_t)s, hi16 | P(uint16_t, u, 0x18b));
                    if (P(uint16_t, w, 0x17d) && I(int32_t, 0x175))
                        building_exit((uint16_t)s, ((uint32_t)(uintptr_t)w & 0xffff0000) | P(uint16_t, w, 0x17d), 2);
                }
            }
            I(uint32_t *, 0x1934)[cell] |= 0xf00;
        }
    }
    I(uint32_t *, 0x1934)[cell] &= 0xfffff0ff;
    ai_group_done(player);
    I(uint32_t *, 0x1934)[cell] = old;
    debug_printf(0x10, (const char *)ORIG_PTR(0x1013ef3c), player);
    return rc;
}
