/* The game logic, 0x1001b380-0x1001bd92: sounds placed on the map (volume and pan from
 * where they are on the screen), the units' voices, weapon and general sounds; and
 * IsoGetSectorCoordinates. */
#include "iso_core.h"
#include "sound.h"

/* not rewritten yet */
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
/* WAVEPLAY.R32's play function (ordinal 47), through the original's import thunk */
#define wave_play \
    ORIG(int(__cdecl *)(void *, const char *, int, int, int, int, int, int, int, int), 0x1011203e)

#define ISO_SCROLL_X 0xcd82
#define ISO_SCROLL_Y 0xcd86
#define ISO_VIEW_W 0xd716
#define ISO_VIEW_H 0xd72a

/* the voice kinds 0-5: file prefix and the unit type's field with the voice number */
#define g_voice_prefix(k) ((const char *)ORIG_PTR(0x10130ce0 + (k) * 4))
#define g_voice_field ((const uint8_t *)ORIG_PTR(0x10130d18))

static int32_t abs32(int32_t v) { return v < 0 ? -v : v; }

/* 0x1001b380: plays wave `name` at map pixel (x, y): quieter the farther it is outside
 * the middle of the view, panned by its distance from the view's left edge; not at
 * all when too quiet (7500, 9000 in multiplayer). The wave's handle, or negative. */
int __cdecl sound_at(const char *name, int16_t x, int16_t y, int a4, int a5, int a6, int a7)
{
    int32_t vol = 10000;
    int32_t dx = x - I(int32_t, ISO_SCROLL_X), dy = y - I(int32_t, ISO_SCROLL_Y);
    if (abs32(dy) < abs32(dx)) {
        if (abs32(dx) > I(int32_t, ISO_VIEW_W) / 2)
            vol = 10000 - (abs32(dx) - I(int32_t, ISO_VIEW_W) / 2) * 3;
    } else if (abs32(dy) > I(int32_t, ISO_VIEW_H) / 2) {
        vol = 10000 - (abs32(dy) - I(int32_t, ISO_VIEW_H) / 2) * 3;
    }
    int32_t pan = dx * 6;
    if (vol < 0)
        vol = 0;
    else if (vol > 10000)
        vol = 9999;
    if (pan > 10000)
        pan = 9999;
    else if (pan < -10000)
        pan = -9999;
    if ((!I(int, 0x175) && vol <= 7500) || (I(int, 0x175) && vol <= 9000))
        return -1;
    /* the original logs the three errors through its debug printf, which returns at
     * once in the shipped build */
    return wave_play(I(void *, 0x181a), name, vol, pan, a7, a6, a4, a5, 0, a7 == 0);
}

/* 0x1001babe: general sound `kind` (6-0x23) of a unit (7: the infantry's or the
 * vehicles' variant) */
int __cdecl unit_sound(uint8_t *u, int kind, int unused, int centered)
{
    char name[128];
    (void)unused;
    if (!u || kind < 6 || kind > 0x23)
        return -1;
    if (kind == 7) {
        int32_t t = P(int32_t, u, U_ID_TYPE);
        if (t >= 0 && t <= 0x13)
            kind = 0xe;
        else if (t >= 0x18 && t < 0x40)
            kind = 0xf;
    }
    wsprintfA(name, (const char *)ORIG_PTR(0x10130e90), kind + 0x1387a);
    if (centered)
        return sound_at(name, (int16_t)I(int32_t, ISO_SCROLL_X), (int16_t)I(int32_t, ISO_SCROLL_Y), 0, 0, 0, 0);
    return sound_at(name, P(int16_t, u, U_X), P(int16_t, u, U_Y), 0, 0, 0, 0);
}

/* 0x1001b5ec: a unit's voice: 0 attack, 1 selected, 2 dying, 3 moving, 4-5 ...; others
 * up to 0x23 are general sounds. Wounded units (under half health, not when dying)
 * speak their next voice. */
int __cdecl unit_voice(uint8_t *u, int kind, int unused, int centered)
{
    char name[128];
    if (!u || kind < 0 || kind > 0xd2)
        return -1;
    if (kind > 5) {
        if (kind >= 0x23)
            return -1;
        return unit_sound(u, kind, unused, centered);
    }
    int32_t n = P(int32_t, P(uint8_t *, u, 0x32), g_voice_field[kind] * 4);
    if (P(int32_t, u, U_ID_TYPE) >= 0x18 && P(int32_t, u, U_ID_TYPE) < 0x40 && kind != 2 &&
        (int16_t)unit_max_health(u, 0) / 2 > P(int16_t, u, 0xd6))
        n++;
    wsprintfA(name, (const char *)ORIG_PTR(0x10130db4), g_voice_prefix(kind), n);
    if (centered)
        return sound_at(name, (int16_t)I(int32_t, ISO_SCROLL_X), (int16_t)I(int32_t, ISO_SCROLL_Y), 0, 0, 0, 0);
    return sound_at(name, P(int16_t, u, U_X), P(int16_t, u, U_Y), 0, 0, 0, 0);
}

/* the weapon sound name: 200 firing, 0xd2 hit (the weapon's fields 0x60, 0x64) */
static void weapon_wave(char *name, const uint8_t *w, int kind)
{
    if (kind == 200)
        wsprintfA(name, (const char *)ORIG_PTR(0x10130dc0), P(int32_t, w, 0x60));
    else if (kind == 0xd2)
        wsprintfA(name, (const char *)ORIG_PTR(0x10130dd0), P(int32_t, w, 0x64));
    else
        lstrcpyA(name, (const char *)ORIG_PTR(0x10130e18)); /* "bad90000.wav" */
}

/* 0x1001b785: a projectile's weapon sound (kind 200-0xd2) */
int __cdecl projectile_sound(uint8_t *p, int kind, int unused, int centered)
{
    char name[128];
    (void)unused;
    if (!p || kind < 200 || kind > 0xd2)
        return -1;
    weapon_wave(name, P(uint8_t *, p, 0x16), kind);
    if (centered)
        return sound_at(name, (int16_t)I(int32_t, ISO_SCROLL_X), (int16_t)I(int32_t, ISO_SCROLL_Y), 0, 0, 0, 0);
    return sound_at(name, P(int16_t, p, 0x22), P(int16_t, p, 0x24), 0, 0, 0, 0);
}

/* 0x1001b90c: weapon `weapon`'s sound (kind 200-0xd2) at (x, y) */
int __cdecl weapon_sound(uint32_t weapon, int kind, int16_t x, int16_t y, int unused, int centered)
{
    char name[128];
    (void)unused;
    if (weapon >= 0x48 || kind < 200 || kind > 0xd2)
        return -1;
    int32_t *t = I(int32_t *, 0x1990);
    weapon_wave(name, (const uint8_t *)(uintptr_t)(t[0] + t[0xb] * (int32_t)weapon * 4), kind);
    if (centered) {
        x = (int16_t)I(int32_t, ISO_SCROLL_X);
        y = (int16_t)I(int32_t, ISO_SCROLL_Y);
    }
    return sound_at(name, x, y, 0, 0, 0, 0);
}

/* 0x1001bbfc: general sound `kind` (6-0x23) at (x, y) */
int __cdecl general_sound(int kind, int16_t x, int16_t y, int unused, int centered)
{
    char name[128];
    (void)unused;
    if (kind < 6 || kind > 0x23)
        return -1;
    wsprintfA(name, (const char *)ORIG_PTR(0x10130ea0), kind + 0x1387a);
    if (centered) {
        x = (int16_t)I(int32_t, ISO_SCROLL_X);
        y = (int16_t)I(int32_t, ISO_SCROLL_Y);
    }
    return sound_at(name, x, y, 0, 0, 0, 0);
}

/* 0x1001bce0: the pixel position of sector `s`'s middle */
void __stdcall IsoGetSectorCoordinates(uint8_t *iso, int s, int *x, int *y)
{
    g_iso = iso;
    if (s < 0 || s > I(int32_t, 0xc2f1))
        return;
    uint16_t per_row = I(uint16_t, 0xc2fd);
    uint16_t row = (uint16_t)(s / per_row), col = (uint16_t)(s % per_row);
    *x = col * 0x100 + 0x80 - I(int32_t, 0xd709);
    *y = row * 0x100 + 0x80 - I(int32_t, 0xd71e);
}
