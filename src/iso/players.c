/* The game logic, 0x10055def-0x10056feb (the parts every game runs): sprite merging,
 * the mission end, unit-kill callbacks, ground marks, difficulty, players, alliances. */
#include "iso_core.h"
#include "combat.h"
#include "players.h"

/* not rewritten yet (names ours, provisional) */
#define game_over ORIG(void(__cdecl *)(int32_t, uint8_t), 0x100562e2)
#include "tilemarks.h"
#define orig_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define debug_log ORIG(void(__cdecl *)(int, const char *, ...), 0x10095f64)

#define TEXT_BUF ((char *)(g_iso + 0xdea))

/* 0x10055def: the engine's default merge mode (always on with I(0xd79f)) */
void __cdecl IsoSetSpriteMerge(uint8_t *iso, int32_t merge)
{
    g_iso = iso;
    if (I(int32_t, 0xd79f))
        merge = 1;
    SpriteSetMergeDefault(I(void *, 0x181e), merge);
}

/* 0x1005624f */
void __cdecl IsoSetMissionEndCallback(uint8_t *iso, int32_t handler)
{
    g_iso = iso;
    I(int32_t, 0x1aba) = handler;
}

/* 0x10056276 and 0x1005629c: callbacks that kill a unit (unit_kill without "quiet") */
void __cdecl kill_cb(uint32_t id, uint32_t anim, int32_t sync)
{
    unit_kill(id, anim, sync, 0);
}
void __cdecl kill_cb2(uint32_t id, uint32_t anim, int32_t sync)
{
    unit_kill(id, anim, sync, 0);
}

/* 0x100562c2: the timer callback that ends the game */
void __cdecl game_over_timer(int32_t a, uint8_t b)
{
    game_over(a, b);
}

/* 0x10056837: a ground mark (a crater: 0x3d-0x41 when `big`, else 0x37-0x3b, chosen
 * by the seed) on the free cell at map point x, y, and the screen redrawn there */
void __cdecl ground_mark(int32_t x, int32_t y, uint32_t seed, int32_t big)
{
    int32_t px, py;
    int done = 0;
    uint32_t cell = (uint32_t)IsoVTilePos(g_iso, &x, &y, &px, &py);
    if (x < 0 || y < 0)
        return;
    uint32_t cells = (uint32_t)(I(int32_t, 0x1a9c) * I(int32_t, 0x1a98));
    uint8_t *marks = P(uint8_t *, I(uint8_t *, 0x1960), 0x10);
    if (cell < cells && !(I(uint8_t *, 0x1934)[cell * 4] & 1) && !marks[cell] &&
        !tile_takes_no_marks(I(uint32_t *, 0x1904)[cell] & 0x7fff)) {
        orig_srand(seed);
        int r = orig_rand();
        marks[cell] = (uint8_t)((big ? 0 : 0xfa) + 0x3d + r % 5);
        done = 1;
    }
    if (done) {
        RECT rc = {px - I(int32_t, 0xcd82), py - I(int32_t, 0xcd86), px - (I(int32_t, 0xcd82) - 0x40),
                   py - (I(int32_t, 0xcd86) - 0x20)};
        SpriteRefreshRect(I(void *, 0x181e), &rc);
    }
}

/* 0x100569c3: 0-2, anything else is 1 */
void __cdecl IsoSetDifficulty(uint8_t *iso, uint32_t d)
{
    g_iso = iso;
    I(uint8_t, 0x106a) = d < 3 ? (uint8_t)d : 1;
}

/* 0x10056e4d */
void __cdecl IsoSetNumPlayers(uint8_t *iso, int32_t n)
{
    g_iso = iso;
    I(int32_t, 0x1b1f) = n;
    I(int32_t, 0x1b1b) = I(int32_t, 0x1b1f);
}

/* 0x10056e8b */
int32_t __cdecl IsoGetNumPlayers(uint8_t *iso)
{
    g_iso = iso;
    return I(int32_t, 0x1b1b);
}

/* 0x10056eae: alliances on or off; off in a network game leaves every player allied with
 * itself only */
void __cdecl IsoEnableAlliances(uint8_t *iso, int32_t on, int32_t value)
{
    g_iso = iso;
    I(int32_t, 0x118) = on;
    I(int32_t, 0xd9d2) = value;
    wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x1013e954), (const char *)ORIG_PTR(on ? 0x1013e940 : 0x1013e948));
    debug_log(6, TEXT_BUF);
    if (!on && I(int32_t, 0x175)) {
        I(int32_t, 0xd9d2) = 0;
        for (int p = 0; p < 8; p++) {
            I(uint8_t, p * 0x1461 + 0x1ba0) = 0;
            I(uint8_t, p * 0x1461 + 0x1ba0) |= (uint8_t)(1 << (p & 0x1f));
        }
    }
}
