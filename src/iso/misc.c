/* The game logic, 0x10058a73-0x1005c073 (the parts every game runs): text sprites,
 * preloading a race's units, a player's special units, random numbers, the briefing
 * screen, path names. */
#include "iso_core.h"
#include "misc.h"

#define orig_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define orig_strlen ORIG(uint32_t(__cdecl *)(const char *), 0x100f5750)
#define orig_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)

/* 0x10058a73: hides the scenario's "show text" sprites (and in network games the five
 * chat sprites) */
void __cdecl IsoHideTextSprites(uint8_t *iso)
{
    g_iso = iso;
    if (I(void *, 0xd9b6)) {
        SpriteEmptyQueue(I(void *, 0x181e), I(void *, 0xd9b6));
        SpriteHide(I(void *, 0x181e), I(void *, 0xd9b6), 1);
    }
    I(int32_t, 0xd9aa) = 0;
    if (I(void *, 0xd9c2)) {
        SpriteEmptyQueue(I(void *, 0x181e), I(void *, 0xd9c2));
        SpriteHide(I(void *, 0x181e), I(void *, 0xd9c2), 1);
    }
    I(int32_t, 0xd9ae) = 0;
    if (I(int32_t, 0x175))
        for (int i = 0; i < 5; i++) {
            if (I(void *, i * 0x10 + 0xd952)) {
                SpriteEmptyQueue(I(void *, 0x181e), I(void *, i * 0x10 + 0xd952));
                SpriteHide(I(void *, 0x181e), I(void *, i * 0x10 + 0xd952), 1);
            }
            I(int32_t, i * 0x10 + 0xd956) = 0;
        }
}

/* 0x1005a67b (formerly player_tech_changed in sdlexec.c): loads the resources of the
 * units the player's race can build at its tech level (16 types, from the tables at
 * I(0x1968) and I(0x197c)). 1 */
int32_t __cdecl player_preload_units(int32_t player)
{
    if (player < 0 || player >= 8 || !I(int32_t, player * 0x1461 + 0x1b69))
        return 1;
    for (int i = 0; i < 0x40; i += 4) {
        int32_t t = I(uint8_t, player * 0x1461 + 0x1b9f) + i;
        uint8_t *techs = I(uint8_t *, 0x1968), *res = I(uint8_t *, 0x197c);
        if (!P(int32_t, *(uint8_t **)techs,
               P(int32_t, techs, 0x2c) * (t >> 2) * 4 + I(uint8_t, player * 0x1461 + 0x1baa) * 4))
            continue;
        int32_t *r = (int32_t *)(*(uint8_t **)res + P(int32_t, res, 0x2c) * t * 4);
        if (r && *r > 0)
            RtLoad(I(void *, 0x180e), (uint32_t)*r);
    }
    return 1;
}

/* the player's special unit at `slot` (+ player * 2) replaced by another of its units
 * that fits, when it is `id` */
static void special_replace(uint32_t id, uint32_t player, int slot, int32_t lo, int32_t hi, int action_e)
{
    uint16_t *s = &I(uint16_t, slot + player * 2);
    if (*s != id)
        return;
    for (int i = 0; i < I(int32_t, 0x1b43); i++) {
        uint32_t o = I(uint32_t *, 0xd67a)[i];
        uint8_t *u;
        if ((int32_t)o > 0 && (u = iso_unit((int)o)) && P(uint8_t, u, U_PLAYER) == player &&
            P(int32_t, u, U_ID_TYPE) > lo && P(int32_t, u, U_ID_TYPE) < hi &&
            (!action_e || P(uint8_t, u, 0x147) == 'E') && o != id) {
            *s = (uint16_t)o;
            break;
        }
    }
    if (*s == id)
        *s = 0;
}

/* 0x1005ab95 (formerly player_unset_d6cb in buttons.c): the player's unit at 0xd6cb
 * (types 0x50-0x53) */
void __cdecl player_replace_d6cb(uint32_t id, uint32_t player)
{
    special_replace(id, player, 0xd6cb, 0x4f, 0x54, 0);
}

/* 0x1005acf2 (formerly player_unset_d6f9): the player's unit at 0xd6f9 (types
 * 0x38-0x3a, in action 'E') */
void __cdecl player_replace_d6f9(uint32_t id, uint32_t player)
{
    special_replace(id, player, 0xd6f9, 0x37, 0x3b, 1);
}

/* 0x1005b404: a random number from lo to hi */
int32_t __cdecl IsoRand(uint8_t *iso, int32_t lo, int32_t hi)
{
    g_iso = iso;
    uint32_t r = (uint32_t)orig_rand();
    return lo + (int32_t)(r % ((uint32_t)(hi - lo) + 1u));
}

/* 0x1005b438 */
void __cdecl IsoBriefingScreen(uint8_t *iso, int32_t on)
{
    g_iso = iso;
    I(uint8_t, 0xd90c) = (uint8_t)on;
    I(uint8_t, 0x181) = (uint8_t)on;
    RtSetBriefingScreenStatus(I(void *, 0x180e), on);
}

/* 0x1005b486 */
uint8_t __cdecl IsoInBriefingScreen(uint8_t *iso)
{
    g_iso = iso;
    return I(uint8_t, 0xd90c);
}

/* 0x1005bfc2 (formerly path_fix in filelist.c): "\\" made "\" in place */
void __cdecl path_unescape(char *path)
{
    int32_t n = (int32_t)orig_strlen(path);
    char *t = winmalloc((SIZE_T)(n + 1), 1);
    int32_t i = 0, j = 0;
    while (i < n) {
        t[j] = path[i];
        if (path[i] == '\\' && path[i + 1] == '\\')
            i += 2;
        else
            i++;
        j++;
    }
    orig_strcpy(path, t);
    winfree(t);
}
