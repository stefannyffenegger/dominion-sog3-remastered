/* The game logic, 0x100a0285-0x100a0530: small lookups of a player's counts and the
 * difficulty; 0x100a2200-0x100a2550: a unit flag, IsoBlastText, a unit's maximum health. */
#include "iso_core.h"
#include "recon.h"
#include "playerinfo.h"

/* 0x100a0285: the player's +0x1b9b count */
int32_t __cdecl player_count_1b9b(int32_t p) { return I(int32_t, p * ISO_PLAYER_SIZE + 0x1b9b); }

/* 0x100a02b6: the player's +0x1b97 count */
int32_t __cdecl player_count_1b97(int32_t p) { return I(int32_t, p * ISO_PLAYER_SIZE + 0x1b97); }

/* 0x100a04d3: the percentage of the map the player has not seen */
uint32_t __cdecl player_unseen(int32_t p) { return (uint16_t)percent_unseen((int16_t)p); }

/* 0x100a04f4: the difficulty (0-2, iso+0x106a) */
uint32_t __cdecl game_difficulty(void) { return I(uint8_t, 0x106a); }

typedef void(__cdecl *leave_fn)(uint8_t *u);

/* 0x100a2200: sets a unit's +0x2a flag; clearing it calls its type's leave handler
 * (0x10118d90), except for kinds 0x40-0x9f */
void __cdecl unit_set_flag_2a(uint8_t *u, uint8_t v)
{
    if (!u)
        return;
    if ((P(int32_t, u, 0x10) < 0x40 || P(int32_t, u, 0x10) > 0x9f) && P(uint8_t, u, 0x2a) && !v) {
        int32_t k = P(int32_t, P(uint8_t *, u, 0x32), 4);
        if (k >= 0 && G(leave_fn, 0x10118d90 + k * 12))
            G(leave_fn, 0x10118d90 + k * 12)(u);
    }
    P(uint8_t, u, 0x2a) = v;
}

#define BF_BlastText ORIG(int(__cdecl *)(void *, void *, uint32_t, void *, int, int, const char *, int), 0x101120b6)

/* IsoBlastText: draws string `text` with the font resource `font` into `dib`, in colour set
 * 1 or 2 (0x101460f8, 0x101450f8; anything else none) */
int32_t __cdecl IsoBlastTextC(uint8_t *iso, uint32_t font, uint32_t dib, int32_t colours, uint32_t text, int x, int y)
{
    g_iso = iso;
    const char *s = RtGetString(I(void *, ISO_APP), text);
    if (!s)
        return 0;
    void **res = (void **)RtGetResource(I(void *, ISO_APP), font);
    if (!*res)
        return 0;
    void *c = colours == 1 ? ORIG_PTR(0x101460f8) : colours == 2 ? ORIG_PTR(0x101450f8) : 0;
    return BF_BlastText(I(void *, 0x1812), *res, dib, c, x, y, s, 0);
}

/* 0x100a23e0: the unit's maximum health at an upgrade level (its own, +0x30, for 0): the
 * type's base (+0x10), plus +0x14 from level 2. Only the low 16 bits count at level 0-1 */
uint32_t __cdecl unit_max_health(uint8_t *u, uint8_t level)
{
    uint8_t l = level ? level : P(uint8_t, u, 0x30);
    uint8_t *t = P(uint8_t *, u, 0x32);
    if (l < 2)
        return ((uint32_t)(uintptr_t)t & 0xffff0000) | P(uint16_t, t, 0x10);
    return (uint32_t)(P(int16_t, t, 0x10) + P(int32_t, t, 0x14));
}

/* 0x100a24d0: the type's percentage (+0x94) of the unit's maximum health */
int32_t __cdecl unit_health_share(uint8_t *u, uint8_t level)
{
    if (!u)
        return 0;
    uint8_t l = level ? level : P(uint8_t, u, 0x30);
    int32_t h = (int16_t)unit_max_health(u, l);
    long double v = (long double)P(int32_t, P(uint8_t *, u, 0x32), 0x94) / (long double)G(float, 0x101131f8);
    return (int32_t)(int64_t)((long double)h * v);
}
