/* The game logic, 0x100a4a14-0x100a4fce: the build slots' progress, one step per tick, and the
 * interface's build tables (0x100a51a6-0x100a5bf4). A
 * slot (see buildqueue.c) has +4 its total time (0: idle), +8 its progress, +0xc/+0x10 the
 * men and materials it costs, +0x14/+0x18 what was paid so far, +0x1c set when the
 * interface finishes it, +0x20/+0x24 the tab and bar light sprites, +0x28 the light frame. */
#include "iso_core.h"
#include "intfwave.h"
#include "debugtime.h"
#include "buildinfo.h"
#include "buildtick.h"

#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))
#define slot_interface_update ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t), 0x100a48d2)
#define unit_place_new ORIG(void(__cdecl *)(uint32_t, uint32_t, uint32_t, int, int, int, int), 0x1009f6e0)
#define building_place_start ORIG(void(__cdecl *)(uint32_t, int32_t), 0x10092379)

static void light(uint32_t res, uint32_t fail)
{
    void *s = ((void **)RtGetResource(I(void *, ISO_APP), res))[1];
    if (!s || !res)
        DebugStr((const char *)ORIG_PTR(fail));
    else
        SpriteSendMessage(I(void *, 0x181e), s, 4, MSG);
}

static void finish(uint8_t p, int32_t kind)
{
    if (kind < 0x40 || kind > 0x87)
        unit_place_new(p, (uint32_t)kind, I(uint8_t, p * ISO_PLAYER_SIZE + 0x1b9f), 0, 0, 0, 1);
    else
        building_place_start(p, kind / 4);
}

/* 0x100a4a14: each player's started build slots pay for this tick's progress (cost *
 * progress / time, less what was paid) and move on. A player short of men or materials
 * stops all building this tick (the local player is told once, waves 0x24 and 0x23). For
 * the local player the lights show the progress in 32 frames. A finished slot places its
 * unit or starts its building (or is left to the interface when +0x1c is set). The step
 * ends at the first finished slot. 0 when short, else 1 */
int32_t __cdecl build_slots_tick(void)
{
    for (uint8_t p = 0; (int32_t)p < I(int32_t, 0x1b1f); p++) {
        for (uint8_t s = 0; s < 4; s++) {
            uint8_t *sl = g_iso + s * 0x31 + p * 0xc4 + 0x2ca;
            if (!P(uint32_t, sl, 4))
                continue;
            uint32_t men = P(uint32_t, sl, 8) * P(uint32_t, sl, 0xc) / P(uint32_t, sl, 4) - P(uint32_t, sl, 0x14);
            uint32_t mat = P(uint32_t, sl, 8) * P(uint32_t, sl, 0x10) / P(uint32_t, sl, 4) - P(uint32_t, sl, 0x18);
            uint32_t *have_men = &I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b97);
            uint32_t *have_mat = &I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b);
            if (*have_men < men || *have_mat < mat) {
                if (I(uint32_t, 0x1b65) == p && !(I(uint8_t, 0x174) >> 1 & 1) && *have_men < men) {
                    I(uint8_t, 0x174) |= 2;
                    player_announce(p, 0x24, 1);
                }
                if (I(uint32_t, 0x1b65) == p && !(I(uint8_t, 0x174) >> 2 & 1) && *have_mat < mat) {
                    I(uint8_t, 0x174) |= 4;
                    player_announce(p, 0x23, 1);
                }
                return 0;
            }
            *have_men -= men;
            *have_mat -= mat;
            P(uint32_t, sl, 0x14) += men;
            P(uint32_t, sl, 0x18) += mat;
            if (I(uint32_t, 0x1b65) == p) {
                I(uint8_t, 0xd696) = 1;
                uint32_t f = (P(uint32_t, sl, 8) << 5) / P(uint32_t, sl, 4);
                if (P(uint8_t, sl, 0x28) != f) {
                    P(uint8_t, sl, 0x28) = (uint8_t)f;
                    if (I(uint8_t, 0xc33f) == s)
                        player_announce(p, 2, 0);
                    {
                        /* the frame goes into the message only when the tab light exists;
                         * otherwise the bar light gets the last message, as in the original */
                        void *t = ((void **)RtGetResource(I(void *, ISO_APP), P(uint32_t, sl, 0x20)))[1];
                        if (!t || !P(uint32_t, sl, 0x20)) {
                            DebugStr((const char *)ORIG_PTR(0x10153f84));
                        } else {
                            MSG[3] = 0;
                            MSG[1] = f;
                            MSG[2] = f;
                            SpriteSendMessage(I(void *, 0x181e), t, 4, MSG);
                        }
                    }
                    if (I(uint8_t, 0xc33f) == s)
                        light(P(uint32_t, sl, 0x24), 0x10153f9c);
                }
            }
            if (P(uint32_t, sl, 4) <= P(uint32_t, sl, 8)) {
                P(uint32_t, sl, 4) = 0;
                if (I(uint32_t, 0x1b65) == p) {
                    I(uint8_t, 0x174) &= 0xfd;
                    I(uint8_t, 0x174) &= 0xfb;
                    if (P(int32_t, sl, 0x1c))
                        slot_interface_update(s, 1, p);
                    else
                        finish(p, P(int32_t, sl, 0));
                    return 1;
                }
                finish(p, P(int32_t, sl, 0));
                return 1;
            }
            P(uint32_t, sl, 8)++;
        }
    }
    return 1;
}

/* IsoSetTabset: the interface's open tab (iso+0xc33f) */
void __stdcall IsoSetTabsetC(uint8_t *iso, uint8_t tab)
{
    g_iso = iso;
    I(uint8_t, 0xc33f) = tab;
}

/* the first word of a loaded resource, or 0 when it cannot be loaded */
static int load_table(uint32_t res, uint32_t off)
{
    if (!RtLoad(I(void *, ISO_APP), res))
        return 0;
    I(uint32_t, off) = *(uint32_t *)RtGetResource(I(void *, ISO_APP), res);
    return 1;
}

/* IsoInitIntfOptions: the build buttons' table (iso+0x18dc), their states (iso+0x1b61) and
 * the interface's script (iso+0x182e) */
int32_t __stdcall IsoInitIntfOptionsC(uint8_t *iso, uint32_t table, uint32_t states, uint32_t script)
{
    g_iso = iso;
    iso_debug_print();
    if (!RtLoad(I(void *, ISO_APP), table)) {
        iso_debug_print();
        return 0;
    }
    I(uint32_t, 0x18dc) = *(uint32_t *)RtGetResource(I(void *, ISO_APP), table);
    I(uint32_t, 0x1b61) = states;
    I(uint32_t, 0x182e) = script;
    return 1;
}

/* IsoRecalcBuildInfo: recounts every player's build levels (and the units of kind 0x7c) from
 * the units on the map, then updates the player's buttons */
void __stdcall IsoRecalcBuildInfoC(uint8_t *iso, uint32_t player)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x1015404c), player);
    memset(g_iso + 0x182, 0, 0x148);
    for (int32_t k = 0; k < I(int32_t, ISO_UNIT_COUNT); k++) {
        int32_t id = I(int32_t *, ISO_UNIT_LIST)[k];
        if (!id || !I(uint8_t **, ISO_UNITS)[id])
            continue;
        uint8_t *u = I(uint8_t **, ISO_UNITS)[id];
        uint8_t *slot = &I(uint8_t, P(uint8_t, u, 0x14) * 0x29 + (P(int32_t, u, 0x10) >> 2) + 0x182);
        if (*slot < P(uint8_t, u, 0x30))
            *slot = P(uint8_t, u, 0x30);
        if (P(int32_t, u, 0x10) == 0x7c)
            I(int32_t, P(uint8_t, u, 0x14) * ISO_PLAYER_SIZE + 0x2e01)++;
    }
    IsoUpdateIntfOptionsC(iso, player);
}

/* IsoInitEconomyTable: the unit types' economy table (iso+0x18ec) */
int32_t __stdcall IsoInitEconomyTableC(uint8_t *iso, uint32_t table)
{
    g_iso = iso;
    return load_table(table, 0x18ec);
}

/* IsoInitTechDependTables: the tables of what each side may build (iso+0x1968), what each
 * kind needs (iso+0x18e4) and a third (iso+0x1980); then all build levels are cleared */
int32_t __stdcall IsoInitTechDependTablesC(uint8_t *iso, uint32_t sides, uint32_t needs, uint32_t third)
{
    g_iso = iso;
    if (!load_table(sides, 0x1968) || !load_table(needs, 0x18e4) || !load_table(third, 0x1980))
        return 0;
    memset(g_iso + 0x182, 0, 0x148);
    return 1;
}

/* IsoUnitIsBuilt: whether the player has a unit of `kind` (build level at least 1) */
int32_t __cdecl IsoUnitIsBuiltC(uint8_t *iso, uint32_t kind, int32_t player)
{
    g_iso = iso;
    return I(uint8_t, (kind >> 2) + player * 0x29 + 0x182) >= 1;
}
