/* The game logic, 0x100a3036-0x100a37fa: mining. A refinery takes materials from its energy
 * well (iso+0x1097, 0x2b bytes each: +0 the sprite, +0xc what is left, +0x10 what it had,
 * +0x14 its state, 2 being mined, 3 empty; +0x15 and +0x16 the animation and fill frame;
 * +0x1f/+0x23 an extra sprite and its object; +0x27 its owner). */
#include "iso_core.h"
#include "playerinfo.h"
#include "power.h"
#include "vecscore.h"
#include "intfwave.h"
#include "radarmap.h"
#include "mining.h"

#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))
#define TYPE_VALUE(u, off) P(uint32_t, P(uint8_t *, I(uint8_t *, 0x18ec), 0), (off) + P(int32_t, I(uint8_t *, 0x18ec), 0x2c) * P(int32_t, u, 0xc) * 4)
#define well_info_update ORIG(void(__cdecl *)(uint8_t *, uint8_t *), 0x100a67c0)
#define unit_set_mode ORIG(void(__cdecl *)(uint32_t, int), 0x100ed2ba)
#define ai_event ORIG(void(__cdecl *)(uint32_t, int, uint32_t, int), 0x100e42a6)

/* 0x100a36d6: a building's output: `max` scaled by its health (+0xd6 of its maximum, rounded
 * to a double as in the original) and by its grid's spare power (100 minus the used
 * percentage, at least 0), plus 1, at most `max`; 0 when it is not working (+0x42) */
uint32_t __cdecl building_output(uint32_t unit, uint32_t max)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    int32_t h = (int16_t)unit_max_health(u, 0);
    volatile double share = (long double)P(int16_t, u, 0xd6) / (long double)h;
    uint32_t n = (uint32_t)(int64_t)((long double)(uint64_t)max * (long double)share);
    int16_t spare = (int16_t)(100 - power_percent((int16_t)unit));
    if (spare < 0)
        spare = 0;
    volatile double f = (long double)spare / (long double)G(double, 0x10113208);
    n = (uint32_t)(int64_t)((long double)(uint64_t)n * (long double)f) + 1;
    if (max < n)
        n = max;
    if (!P(int16_t, I(uint8_t **, ISO_UNITS)[unit & 0xffff], 0x42))
        n = 0;
    return n;
}

/* 0x100a35ab: the well's fill frame (1-4, from what is left); with `query` only returned.
 * When it changed (or `force`), the sprite is sent to its new frames */
uint32_t __cdecl well_fill_frame(uint8_t *w, int32_t force, int32_t query)
{
    uint32_t pct = (uint32_t)(int64_t)((long double)P(int32_t, w, 0xc) / (long double)P(int32_t, w, 0x10) *
                                       (long double)G(double, 0x10113200));
    uint32_t f = pct / 0x19 + 1;
    if (query)
        return f;
    if (P(uint8_t, w, 0x16) != f || force) {
        uint32_t first = f * 7 - 1;
        SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0));
        P(uint8_t, w, 0x16) = (uint8_t)f;
        SpriteSendMessage(I(void *, 0x181e), P(void *, w, 0), 1, (uint32_t *)ORIG_PTR(0x10157334));
        MSG[1] = first;
        MSG[2] = first + 6;
        MSG[3] = 0x60;
        SpritePostMessage(I(void *, 0x181e), P(void *, w, 0), 4, MSG);
        SpritePostMessage(I(void *, 0x181e), P(void *, w, 0), 4, (uint32_t *)ORIG_PTR(0x10157338));
    }
    return f;
}

static void add_materials(uint32_t p, uint32_t n)
{
    I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b) += n;
    score_add_materials(p, (int32_t)n);
}

static void cap_materials(uint32_t p)
{
    if (I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b) > 99999)
        I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b) = 99999;
    if (I(int32_t, p * ISO_PLAYER_SIZE + 0x1b69) == 1 && I(uint32_t, 0x1b65) == p)
        I(uint8_t, 0xd696) = 1;
}

/* 0x100a3036: a refinery's mining, when its time (+0xa0) has come and it is working: its
 * output (type value +0x1c) goes to its player, or with +0x3e bit 12 half of it (at least 1)
 * to its player and half to the player at +0x2deb; at most 99999 each. The well loses what
 * was taken; an empty well is marked, announced to its owner, its sprite and extra sprite
 * stopped, and the refinery set idle. 1 if it mined */
int32_t __cdecl refinery_mine(uint32_t unit)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (P(uint32_t, u, 0xa0) >= I(uint32_t, 0x17dd) || !P(int16_t, u, 0x42))
        return 0;
    uint8_t p = P(uint8_t, u, 0x14);
    uint8_t *w = P(int32_t, u, 0xfa) < 0 ? 0 : g_iso + P(int32_t, u, 0xfa) * 0x2b + 0x1097;
    if (!w || P(uint8_t, w, 0x14) != 2)
        return 0;
    uint32_t n = building_output(unit, TYPE_VALUE(u, 0x1c));
    if (!(P(uint32_t, u, 0x3e) >> 12 & 1)) {
        add_materials(p, n);
    } else {
        uint8_t q = I(uint8_t, p * ISO_PLAYER_SIZE + 0x2deb);
        n >>= 1;
        if (n < 1)
            n = 1;
        add_materials(p, n);
        add_materials(q, n);
        cap_materials(q);
    }
    if (I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b) > 99999)
        I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b9b) = 99999;
    P(uint32_t, u, 0xa0) = TYPE_VALUE(u, 0x30) + I(uint32_t, 0x17dd);
    if (I(int32_t, p * ISO_PLAYER_SIZE + 0x1b69) == 1 && I(uint32_t, 0x1b65) == p)
        I(uint8_t, 0xd696) = 1;
    if (P(int32_t, w, 0xc) <= 0)
        return 1;
    P(int32_t, w, 0xc) -= (int32_t)n;
    if (P(int32_t, w, 0xc) < 0)
        P(int32_t, w, 0xc) = 0;
    if ((uint32_t)P(uint8_t, u, 0x14) == I(uint32_t, 0x1b65))
        well_info_update(u, w);
    well_fill_frame(w, 0, 0);
    if (P(int32_t, w, 0xc))
        return 1;
    P(uint8_t, w, 0x14) = 3;
    if (I(int32_t, 0x1b65) == P(int32_t, w, 0x27))
        player_announce(I(uint32_t, 0x1b65), 0x50, 1);
    if (P(uint8_t, w, 0x16)) {
        SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0));
        P(uint8_t, w, 0x15) = 5;
        P(uint8_t, w, 0x16) = 0;
        MSG[1] = 5;
        MSG[2] = 5;
        MSG[3] = 0;
        SpriteSendMessage(I(void *, 0x181e), P(void *, w, 0), 4, MSG);
        unit_set_mode(unit, 2);
        radar_plot_object(w, 1, 1);
        if (!I(int32_t, 0x175) && I(int32_t, ISO_HAVE_SPRITES))
            ai_event(P(uint8_t, u, 0x14), 0xe, unit & 0xffff, 0);
    }
    if (P(void *, w, 0x1f)) {
        SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0x1f));
        SpriteHide(I(void *, 0x181e), P(void *, w, 0x1f), 1);
        RtDelete(I(void *, ISO_APP), P(uint32_t, w, 0x23));
        P(uint32_t, w, 0x23) = 0;
        P(void *, w, 0x1f) = 0;
    }
    return 1;
}
