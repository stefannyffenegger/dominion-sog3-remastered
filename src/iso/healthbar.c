/* The game logic, 0x1004ea01-0x1004f45a: a unit's health bar: two small 8-bit DIBs
 * appended to its sprite as cels (+0x90/+0x94, their cel numbers at +0x8c/+0x8e), drawn
 * with the health and, for the local player's units that have them, pips above (ammo
 * or the like: filled, empty). */
#include "iso_core.h"
#include "healthbar.h"

/* not rewritten yet (names ours, provisional) */
#define unit_max_health ORIG(int(__cdecl *)(uint8_t *, int), 0x100a23e0)
#define unit_report_damage ORIG(void(__cdecl *)(uint32_t), 0x1006cff3)
#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)

#define DIB_W(d) P(int32_t, d, 8)
#define DIB_H(d) P(int32_t, d, 0xc)
#define DIB_BITS(d) P(uint8_t *, d, 0x20)

/* 0x1004ebe6: the health bar drawn for the unit's health; `hit`: after damage (a unit at
 * half health or less then reports it, once every 500 ticks) */
void __cdecl unit_health_bar(uint32_t id, int32_t hit)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int16_t health = P(int16_t, u, 0xd6);
    int16_t max = (int16_t)unit_max_health(u, 0);
    if (health <= (int16_t)(max >> 1) && hit &&
        (P(int16_t, u, 0x1bd) == 0x40 || P(int16_t, u, 0x1bd) == 0x46)) {
        int32_t t = P(int32_t, u, U_ID_TYPE);
        if (P(int16_t, u, 0x1b3) < 0 && ((t > 0x4f && t < 0x88) || (t > 0x3f && t < 0x50))) {
            unit_report_damage(id);
            P(int16_t, u, 0x1b3) = 500;
        } else if (P(int16_t, u, 0x1b3) < 0 && t > 0x27 && t < 0x30) {
            unit_report_damage(id);
            P(int16_t, u, 0x1b3) = 500;
        }
    }
    if (!P(uint8_t, u, 0x2e))
        return;
    uint16_t len = (uint16_t)(DIB_W(P(uint8_t *, u, 0x90)) - 4);
    uint16_t pips, full;
    if (P(int32_t, u, 0x10) == 0x30) {
        uint8_t *lib = I(uint8_t *, 0x1990);
        int32_t per = P(int32_t, *(uint8_t **)lib, 0x1c + P(int32_t, lib, 0x2c) * P(int16_t, u, 0xe6) * 4);
        pips = (uint16_t)(P(int32_t, P(uint8_t *, u, 0x32), 0x28) / per);
        full = (uint16_t)((int32_t)P(uint16_t, u, 0xdc) / per);
    } else {
        pips = (uint16_t)P(int32_t, P(uint8_t *, u, 0x32), 0xe0);
        full = P(uint8_t, u, 0xf3);
    }
    int32_t top = pips ? 10 : 0;
    int32_t n = (int32_t)health * (int32_t)len / (int32_t)max + 1;
    if (n >= (int32_t)len)
        n = len;
    uint16_t filled = (uint16_t)n;
    int32_t frame, high, low, good;
    if (I(int32_t, 0x175)) {
        frame = 0xfc;
        high = 0xfd;
        low = 0xfe;
        good = 0xff;
    } else {
        frame = 10;
        high = 0x4f;
        low = 0x77;
        good = 0xdd;
    }
    uint32_t rest = (uint32_t)len - filled;
    int32_t bad = filled < (uint32_t)(len >> 1) ? low : high;
    int32_t w0 = DIB_W(P(uint8_t *, u, 0x90));
    int32_t r0 = w0 * top, r1 = w0 * (top + 1), r2 = w0 * (top + 2), r3 = w0 * (top + 3), r4 = w0 * (top + 4);
    for (int side = 0; side < 2; side++) {
        uint8_t *d = P(uint8_t *, u, 0x90 + side * 4);
        if (!d)
            continue;
        DIB_BITS(d)[r1] = 1;
        DIB_BITS(d)[r2] = 1;
        DIB_BITS(d)[r3] = 1;
        DIB_BITS(d)[r1 + 1] = 1;
        DIB_BITS(d)[r2 + 1] = (uint8_t)frame;
        DIB_BITS(d)[r3 + 1] = (uint8_t)frame;
        orig_memset(DIB_BITS(d) + r0 + 1, 1, len + 2u);
        orig_memset(DIB_BITS(d) + r1 + 2, frame, len);
        int32_t at_bad, at_good;
        if (side == 0) {
            at_bad = filled + 2;
            at_good = 2;
        } else {
            at_bad = 2;
            at_good = (int32_t)rest + 2;
        }
        if (filled) {
            orig_memset(DIB_BITS(d) + r2 + at_good, good, filled);
            orig_memset(DIB_BITS(d) + r3 + at_good, good, filled);
        }
        if (rest) {
            orig_memset(DIB_BITS(d) + r2 + at_bad, bad, rest);
            orig_memset(DIB_BITS(d) + r3 + at_bad, bad, rest);
        }
        orig_memset(DIB_BITS(d) + r4 + 2, frame, len);
        int32_t e = DIB_W(d) - 2;
        DIB_BITS(d)[e + r1] = 1;
        DIB_BITS(d)[r2 + e] = (uint8_t)frame;
        DIB_BITS(d)[r3 + e] = (uint8_t)frame;
        e = DIB_W(d) - 1;
        DIB_BITS(d)[e + r1] = 1;
        DIB_BITS(d)[r2 + e] = 1;
        DIB_BITS(d)[r3 + e] = 1;
        if (pips && P(uint8_t, u, U_PLAYER) == I(uint32_t, 0x1b65)) {
            int32_t x = side == 0 ? 0 : DIB_W(d) - 1;
            orig_memset(DIB_BITS(d), 0, (uint32_t)(DIB_W(d) << 3));
            SpriteDibMerge((RtDib *)d, I(RtDib *, 8), 1, 8, x, DIB_H(d) - 8, 0, 0);
            x += side == 0 ? 1 : -6;
            for (int32_t i = 0; i < (int32_t)pips; i++) {
                RtDib *pip;
                if (i < (int32_t)full)
                    pip = P(int32_t, u, 0x10) == 0x30 ? I(RtDib *, 0x18) : I(RtDib *, 0x10);
                else
                    pip = I(RtDib *, 0xc);
                SpriteDibMerge((RtDib *)d, pip, 6, 8, x, DIB_H(d) - 8, 0, 0);
                x += side == 0 ? 6 : -1;
                SpriteDibMerge((RtDib *)d, I(RtDib *, 8), 1, 8, x, DIB_H(d) - 8, 0, 0);
                x += side == 0 ? 1 : -6;
            }
        }
    }
    if (P(uint8_t, u, U_SEL_STATE) == 1)
        P(uint8_t, u, U_SEL_STATE) = 2;
}

/* 0x1004ea01: the unit's two health bar DIBs, added to its sprite (the turret's for
 * types 0x30-0x37) as cels, then drawn */
void __cdecl unit_health_bar_init(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t *s = P(int32_t, u, U_ID_TYPE) < 0x30 || P(int32_t, u, U_ID_TYPE) > 0x37 ? P(uint8_t *, u, 4)
                                                                                      : P(uint8_t *, P(uint8_t *, u, 0x39d), 4);
    uint8_t *cel = P(uint8_t **, s, 0x24)[1];
    int32_t pips = !P(int32_t, P(uint8_t *, u, 0x32), 0xe0) && P(int32_t, u, 0x10) != 0x30 ? 0 : 10;
    int32_t w = P(int32_t, cel, 8), h = P(int32_t, cel, 0xc);
    for (int side = 0; side < 2; side++) {
        uint8_t *d = SpriteDibCreate(w, pips + 5, 1, 8);
        if (!d) {
            P(uint8_t *, u, 0x90 + side * 4) = NULL;
            continue;
        }
        P(uint8_t *, u, 0x90 + side * 4) = d;
        P(int32_t, d, 0x14) = P(int32_t, cel, 0x14);
        P(int32_t, d, 0x18) = P(int32_t, cel, 0x18) + h / 2 + (pips >> 1) + 3;
        uint16_t n = (uint16_t)SpriteGetNumCels(s);
        if (!SpriteAppendBitmap(I(void *, 0x181e), s, d))
            P(int16_t, u, 0x8c + side * 2) = -1;
        else
            P(uint16_t, u, 0x8c + side * 2) = n;
    }
    if (P(void *, u, 0x90) && P(void *, u, 0x94))
        unit_health_bar(id, 0);
}

/* 0x1004f34e: the unit's report kind (+0x1bd): 0x46 for structures, 0x40 for types
 * 0x28-0x2f */
void __cdecl unit_set_report_kind(uint16_t id)
{
    uint8_t *u;
    if (!id || !(u = iso_unit(id)))
        return;
    int32_t t = P(int32_t, u, U_ID_TYPE);
    if (t < 0x40 || t > 0x9f) {
        if (t > 0x27 && t < 0x30)
            P(int16_t, u, 0x1bd) = 0x40;
    } else {
        P(int16_t, u, 0x1bd) = 0x46;
    }
}
