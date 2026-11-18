/* The game logic, 0x10067abc-0x10068ba2: projectiles. A weapon's shot is an entry of the
 * effects list (0x44 bytes) with a sprite: launched from the muzzle of the firing unit's
 * current frame, flying (turning, for guided kinds) towards the target until it reaches it
 * or its range or time runs out. */
#include "iso_core.h"
#include "healthbar.h"
#include "sound.h"
#include "threatmap.h"
#include "screen.h"
#include "players.h"
#include "projectile.h"

#define ISO_EFFECTS 0xd6db
#define ISO_FX_SPRITES 0xd6e9   /* 0xe-byte records, the sprite at +6 */
#define ISO_FX_SPRITES_N 0xd6e7 /* ushort */

#define g_hypot ORIG(long double(__cdecl *)(double, double), 0x100f4fc0)
#define effect_slot_take ORIG(uint32_t(__cdecl *)(uint8_t, uint16_t), 0x1006bbee)

/* a table of the original's: a pointer to the rows (+0), the row length in ints (+0x2c) */
static uint8_t *table_row(uint32_t off, int32_t row, int32_t size)
{
    uint8_t *t = I(uint8_t *, off);
    return P(uint8_t *, t, 0) + P(int32_t, t, 0x2c) * row * size;
}

/* the muzzle of the firing unit's current frame: the muzzle table (0x1988) by facing and
 * frame (mirrored sprites use the right side's frames for the left), plus a turret's mount
 * (0x1964); also the shot's sprite level, and the unit's frame, cooldown and reload */
static void muzzle_point(uint8_t *u, uint8_t *w, uint8_t *e, int32_t *px, int32_t *py)
{
    uint8_t *type = P(uint8_t *, u, 0x32);
    int32_t big = P(int32_t, type, 0xb4);
    int32_t facing, f, q1, q2, q3, alt, back;
    if (big) {
        facing = f = P(uint8_t, u, 0x7c);
        q1 = 0xc;
        q2 = 0x18;
        q3 = 0x24;
        alt = 0x48 - P(uint8_t, u, 0x7c);
        back = 0x17;
    } else {
        facing = f = P(uint8_t, u, 0x79);
        q1 = 2;
        q2 = 4;
        q3 = 6;
        alt = 7;
        back = 3;
    }
    /* mirrored sprites: the facings on the left use the right's frames */
    if (P(int32_t, type, 0xd8)) {
        if (q1 < f && f <= q2)
            facing = q1 - (f - q1);
        else if (q2 < f && f < q3)
            facing = alt;
        if (q3 <= facing)
            facing -= back;
    }
    int32_t *muzzle = (int32_t *)table_row(0x1988, P(int32_t, type, 0xd4) * facing + P(int32_t, type, 0xd0) +
                                                       P(uint8_t, u, 0xec), 4);
    int32_t sx, sy;
    if (P(int32_t, type, 0xd8) && q1 < f && f < q3)
        sx = P(int16_t, u, 0x52) - muzzle[0];
    else
        sx = P(int16_t, u, 0x52) + muzzle[0];
    sy = P(int16_t, u, 0x54) + muzzle[1];
    if (f >= 0 && f <= q2)
        P(int16_t, e, 0x2e) = (int16_t)(P(int16_t, u, 0x66) + 1);
    else
        P(int16_t, e, 0x2e) = (int16_t)(P(int16_t, u, 0x66) - 1);
    int32_t ut = P(int32_t, u, 0xc);
    if (ut >= 0x28 && ut <= 0x2f) {
        int32_t *off = (int32_t *)(table_row(0x1964, ut - 0x28, 8) + P(uint16_t, u, 0x145) * 8);
        sx += off[0];
        sy += off[1];
    }
    P(uint8_t, u, 0xec)++;
    if (P(int32_t, type, 0xd4) <= P(uint8_t, u, 0xec))
        P(uint8_t, u, 0xec) = 0;
    if (P(int32_t, w, 0x1c) < P(uint16_t, u, 0xdc))
        P(uint16_t, u, 0xdc) = (uint16_t)(P(uint16_t, u, 0xdc) - P(int32_t, w, 0x1c));
    else
        P(uint16_t, u, 0xdc) = 0;
    P(uint8_t, u, 0xee) = P(uint8_t, w, 0x20);
    *px = sx;
    *py = sy;
}

/* 0x10067acc: launches effect `id` as `weapon`'s shot from `unit` at the point (x, y) or,
 * when given, at unit `target`. 0 if the unit is gone or no sprite is free */
int32_t __cdecl projectile_launch(uint32_t id, uint32_t unit, uint32_t x, uint32_t y, uint32_t target,
                                  uint32_t weapon)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    uint8_t *type = P(uint8_t *, u, 0x32);
    int32_t big = P(int32_t, type, 0xb4); /* 36 facings instead of 8 */
    unit_set_report_kind((uint16_t)target);
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t *w = table_row(0x1990, (int16_t)weapon, 4);
    int32_t sx, sy;
    muzzle_point(u, w, e, &sx, &sy);
    P(int16_t, e, 0x36) = (int16_t)(0xe10 / (P(int32_t, w, 0x10) << 3));
    P(int16_t, e, 0x38) = (int16_t)(P(int16_t, e, 0x36) / 2);
    int32_t slot = (int32_t)effect_slot_take(1, 0);
    if (slot < 0 || P(uint16_t, g_iso, ISO_FX_SPRITES_N) <= slot)
        return 0;
    P(void *, e, 0) = P(void *, I(uint8_t *, ISO_FX_SPRITES), slot * 0xe + 6);
    P(void *, e, 4) = 0;
    P(uint16_t, e, 8) = (uint16_t)unit;
    P(uint16_t, e, 0xa) = (uint16_t)target;
    P(uint16_t, e, 0xc) = P(uint16_t, I(uint8_t **, ISO_UNITS)[unit & 0xffff], 0x181);
    P(uint8_t, e, 0x43) &= 0xfd;
    if (target & 0xffff) {
        P(uint16_t, e, 0xe) = P(uint16_t, I(uint8_t **, ISO_UNITS)[target & 0xffff], 0x52);
        P(uint16_t, e, 0x10) = P(uint16_t, I(uint8_t **, ISO_UNITS)[target & 0xffff], 0x54);
    } else {
        P(uint16_t, e, 0xe) = (uint16_t)x;
        P(uint16_t, e, 0x10) = (uint16_t)y;
    }
    P(uint16_t, e, 0x12) = (uint16_t)weapon;
    P(uint8_t *, e, 0x16) = w;
    P(uint8_t, e, 0x14) = P(uint8_t, u, 0x14);
    P(uint8_t, e, 0x15) = P(uint8_t, u, 0x16);
    P(uint16_t, e, 0x1e) = (uint16_t)x;
    P(uint16_t, e, 0x20) = (uint16_t)y;
    P(int16_t, e, 0x22) = (int16_t)sx;
    P(int16_t, e, 0x24) = (int16_t)sy;
    P(int16_t, e, 0x2e) = 19000;
    SpriteSetLevel(I(void *, ISO_HSL), P(void *, e, 0), P(int16_t, e, 0x2e));
    /* the box around the aim point that counts as arrived */
    int32_t r;
    if (u && P(int32_t, type, 0x34))
        r = P(int32_t, type, 0x34) / 2;
    else
        r = P(int32_t, w, 8) * 2;
    P(int16_t, e, 0x26) = (int16_t)(P(int16_t, e, 0x1e) - r);
    P(int16_t, e, 0x28) = (int16_t)(P(int16_t, e, 0x20) - r);
    P(int16_t, e, 0x2a) = (int16_t)(P(int16_t, e, 0x1e) + r);
    P(int16_t, e, 0x2c) = (int16_t)(P(int16_t, e, 0x20) + r);
    P(uint8_t, e, 0x30) = 0;
    P(uint8_t, e, 0x31) = 0;
    if (big) {
        P(uint16_t, e, 0x32) = (uint16_t)(P(uint8_t, u, 0x7c) * P(int32_t, w, 0x44));
        P(uint8_t, e, 0x35) = P(uint8_t, u, 0x7c);
        P(uint16_t, e, 0x3e) = G(uint16_t, 0x10140588 + P(uint8_t, e, 0x35) * 2);
    } else {
        P(uint16_t, e, 0x32) = (uint16_t)(P(uint8_t, u, 0x79) * P(int32_t, w, 0x10) * P(int32_t, w, 0x44));
        P(uint8_t, e, 0x35) = P(uint8_t, u, 0x79);
        P(uint16_t, e, 0x3e) = G(uint16_t, 0x10140578 + P(uint8_t, e, 0x35) * 2);
    }
    P(uint8_t, e, 0x34) = P(uint8_t, w, 0x44);
    int32_t dx = P(int16_t, e, 0x1e) - sx, dy = P(int16_t, e, 0x20) - sy;
    int32_t len = (int32_t)(int64_t)g_hypot((double)dx, (double)dy);
    P(int16_t, e, 0x3c) = (int16_t)(len * 3 / P(int32_t, w, 8));
    P(uint8_t, e, 0x41) = P(uint8_t, w, 0xc);
    P(uint8_t, e, 0x42) = P(uint8_t, e, 0x41);
    P(uint8_t, e, 0x40) = P(uint8_t, w, 0x2c);
    if (!P(int32_t, w, 0x58))
        projectile_sound(e, 200, 0, 0);
    SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, e, 0), 1);
    effect_next_frame(id);
    return 1;
}

#define crt_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define crt_rand ORIG(int32_t(__cdecl *)(void), 0x100f4d80)
#define iso_angle ORIG(int32_t(__stdcall *)(uint8_t *, int32_t, int32_t), 0x1007042c)
#define iso_sin ORIG(int32_t(__stdcall *)(uint8_t *, int32_t), 0x10070757)
#define iso_cos ORIG(int32_t(__stdcall *)(uint8_t *, int32_t), 0x10070380)
#define effect_end ORIG(void(__cdecl *)(uint32_t, int, void *), 0x1006b00a)
#define unit_hit_flash ORIG(void(__cdecl *)(uint32_t, int), 0x10076de9)
#define IsoSetBlitProc ORIG(void(__cdecl *)(uint8_t *, void *, int), 0x100944b2)
#define effect_start                                                                         \
    ORIG(void(__cdecl *)(void *, int, int, int, uint32_t, int, int, int, int, uint32_t, uint32_t), 0x1006d9bd)
#define UNIT_HIT_HANDLERS ((uint8_t *)ORIG_PTR(0x101545c8)) /* per unit type, 0x44 apart */
#define EFFECT_END_HANDLERS ((uint8_t *)ORIG_PTR(0x10157158)) /* per effect kind, 0x18 apart */

typedef void(__cdecl *unit_hit_fn)(uint32_t, int, uint32_t, int32_t);
typedef void(__cdecl *effect_end_fn)(uint32_t);

/* 0x10068286: a projectile's step. Arrived at its aim point: the target unit (if still
 * near) is hit, with a miss chance from the firer's accuracy or a random spread, otherwise
 * the ground is marked; the kind's end handler runs. Out of range or time: the same without
 * the hit. Guided kinds turn towards the aim point. With `trail`, every few steps a trail
 * sprite is left. 0 when it ended */
int32_t __cdecl projectile_fly(uint32_t id, int32_t trail)
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t *w = P(uint8_t *, e, 0x16);
    effect_end_fn on_end;
    P(int16_t, e, 0x3a)++;
    if (P(int16_t, e, 0x26) <= P(int16_t, e, 0x22) && P(int16_t, e, 0x22) < P(int16_t, e, 0x2a) &&
        P(int16_t, e, 0x28) <= P(int16_t, e, 0x24) && P(int16_t, e, 0x24) < P(int16_t, e, 0x2c) &&
        !(P(uint8_t, e, 0x43) >> 1 & 1)) {
        P(uint16_t, e, 0x22) = P(uint16_t, e, 0x1e);
        P(uint16_t, e, 0x24) = P(uint16_t, e, 0x20);
        uint8_t *t = I(uint8_t **, ISO_UNITS)[P(uint16_t, e, 0xa)];
        if (t) {
            uint32_t hi = (uint32_t)(uintptr_t)t & 0xffff0000;
            int away = 0;
            if (!(P(uint32_t, t, 0x3e) >> 6 & 1))
                unit_update_bounds(hi | P(uint16_t, t, 0x1cb));
            else
                SpriteGetBounds(I(void *, ISO_HSL), P(void *, t, 4), (RECT *)(t + 0xc6));
            InflateRect((RECT *)(t + 0xc6), 0x10, 0x10);
            if (P(int16_t, e, 0x22) < P(int32_t, t, 0xc6) || P(int32_t, t, 0xce) <= P(int16_t, e, 0x22) ||
                P(int16_t, e, 0x24) < P(int32_t, t, 0xca) || P(int32_t, t, 0xd2) <= P(int16_t, e, 0x24))
                away = 1;
            if (away) {
                ground_mark(P(int16_t, e, 0x22), P(int16_t, e, 0x24), P(uint16_t, e, 0xa), 0);
            } else {
                int32_t spread;
                uint8_t *firer = I(uint8_t **, ISO_UNITS)[P(uint16_t, e, 8)];
                if (!firer || !P(int32_t, P(uint8_t *, firer, 0x32), 0x34)) {
                    crt_srand(P(uint16_t, e, 0xc));
                    spread = crt_rand() % (P(int32_t, w, 0x40) / 2);
                } else {
                    crt_srand(P(uint16_t, firer, 0x181));
                    int32_t roll = crt_rand() % 100 + 1;
                    if (P(int32_t, P(uint8_t *, firer, 0x32), 0x34) < roll)
                        spread = P(int32_t, w, 0x40) / 2 + (100 - roll);
                    else
                        spread = 0;
                }
                unit_hit_flash(hi | P(uint16_t, t, 0x1cb), 1);
                unit_hit_fn hit = P(unit_hit_fn, UNIT_HIT_HANDLERS, P(int32_t, t, 0xc) * 0x44);
                if (hit && !(P(uint8_t, e, 0x43) >> 1 & 1))
                    hit(((uint32_t)(uintptr_t)e & 0xffff0000) | P(uint16_t, e, 0xa), 0, id, spread);
            }
            if (!(P(uint8_t, e, 0x43) >> 1 & 1))
                effect_end(id, 1, 0);
        }
        on_end = P(effect_end_fn, EFFECT_END_HANDLERS, P(int32_t, w, 0x50) * 0x18);
        if (on_end && !(P(uint8_t, e, 0x43) >> 1 & 1)) {
            on_end(id);
            return 0;
        }
    }
    if (P(int32_t, w, 4) <= P(int16_t, e, 0x3a) || P(int16_t, e, 0x3c) <= P(int16_t, e, 0x3a)) {
        effect_end(id, 0, 0);
        ground_mark(P(int16_t, e, 0x22), P(int16_t, e, 0x24), P(uint16_t, e, 0xa), 0);
        on_end = P(effect_end_fn, EFFECT_END_HANDLERS, P(int32_t, w, 0x50) * 0x18);
        if (on_end) {
            on_end(id);
            return 0;
        }
    }
    int16_t step_x = (int16_t)(P(int16_t, e, 0x1e) - P(int16_t, e, 0x22));
    int16_t step_y = (int16_t)(P(int16_t, e, 0x20) - P(int16_t, e, 0x24));
    int32_t angle = iso_angle(g_iso, step_x, step_y);
    int32_t sn = iso_sin(g_iso, P(int16_t, e, 0x3e));
    int32_t cs = iso_cos(g_iso, P(int16_t, e, 0x3e));
    /* speeding up to the full speed (+8) */
    if ((int32_t)P(uint8_t, e, 0x31) < P(int32_t, w, 8)) {
        int32_t v = P(int32_t, w, 8) - (P(int32_t, w, 8) - P(uint8_t, e, 0x31));
        step_x = (int16_t)(v * cs / 10000);
        step_y = (int16_t)(v * sn / 10000);
        P(uint8_t, e, 0x31) = (uint8_t)(P(int32_t, w, 0x24) + P(uint8_t, e, 0x31));
    }
    if (!(P(uint8_t, e, 0x43) >> 1 & 1)) {
        if (P(int8_t, e, 0x40) == 0) {
            int16_t frame = P(int16_t, e, 0x32);
            int32_t direct = (P(int16_t, e, 0x38) + angle) / P(int16_t, e, 0x36);
            int16_t *dir = &P(int16_t, e, 0x3e);
            int32_t turn = P(int16_t, e, 0x36);
            if (*dir < angle) {
                int32_t d = angle - *dir;
                if (d < 0)
                    d = -d;
                if (d > 0) {
                    if (d > 0x708) {
                        *dir = (int16_t)(*dir - turn);
                        frame--;
                    } else if (turn > d) {
                        *dir = (int16_t)(*dir + d);
                    } else {
                        *dir = (int16_t)(turn + *dir);
                        frame++;
                    }
                }
            } else if (*dir > angle) {
                int32_t d = angle - *dir;
                if (d < 0)
                    d = -d;
                if (d > 0) {
                    if (d > 0x708) {
                        *dir = (int16_t)(turn + *dir);
                        frame++;
                    } else if (turn > d) {
                        *dir = (int16_t)(*dir - d);
                    } else {
                        *dir = (int16_t)(*dir - turn);
                        frame--;
                    }
                }
            } else {
                frame = (int16_t)direct;
            }
            if (*dir < 0)
                *dir = (int16_t)(*dir + 0xe10);
            if (P(int16_t, e, 0x32) != frame) {
                if (frame < 0) {
                    P(int16_t, e, 0x32) = (int16_t)(P(int32_t, w, 0x10) * P(int32_t, w, 0x44) * 8 - 1);
                    *dir = (int16_t)(0xe10 - P(int16_t, e, 0x38));
                } else if (frame < P(int32_t, w, 0x10) * 8) {
                    P(int16_t, e, 0x32) = frame;
                } else {
                    P(int16_t, e, 0x32) = 0;
                    *dir = 0;
                }
            }
            P(uint8_t, e, 0x40) = P(uint8_t, w, 0x2c);
        } else {
            P(uint8_t, e, 0x40)--;
        }
    }
    if (P(uint32_t, w, 8) == P(uint8_t, e, 0x31)) {
        step_x = (int16_t)(P(uint8_t, e, 0x31) * cs / 10000);
        step_y = (int16_t)(P(uint8_t, e, 0x31) * sn / 10000);
    }
    int32_t nx = P(int16_t, e, 0x22) + step_x;
    int32_t ny = P(int16_t, e, 0x24) + step_y;
    if ((P(int16_t, e, 0x3a) & 5) && P(int32_t, w, 0x18) > 0 && (P(uint8_t, e, 0x43) >> 2 & 1) && trail) {
        int32_t slot = (int32_t)effect_slot_take(0, 0);
        if (slot >= 0 && slot < P(uint16_t, g_iso, ISO_FX_SPRITES_N)) {
            void *s = P(void *, I(uint8_t *, ISO_FX_SPRITES), slot * 0xe + 6);
            if (I(int, 0x13c))
                IsoSetBlitProc(g_iso, s, 0x1f9);
            effect_start(s, P(int16_t, e, 0x2e) - 1, P(int16_t, e, 0x22), P(int16_t, e, 0x24), P(uint32_t, w, 0x14),
                         P(int32_t, w, 0x18), 0, 0, I(int32_t, 0xd897), I(uint32_t, 0xd863) >> 1, 0);
        }
    }
    P(int16_t, e, 0x22) = (int16_t)nx;
    P(int16_t, e, 0x24) = (int16_t)ny;
    return 1;
}

#define unit_attach_slot ORIG(int(__cdecl *)(uint32_t, int32_t *), 0x1006cc29)
#define effect_slot_for ORIG(uint32_t(__cdecl *)(uint32_t, uint32_t), 0x1006bbee)
#define effect_free ORIG(void(__cdecl *)(uint32_t), 0x100a7742)

/* a shot's end with an explosion (kinds with an explosion animation, +0x34): the explosion
 * sprite is started where the shot is (and, if the target unit has a free attachment slot,
 * noted there; flipped by the firer's facing), with its sound; then the shot's sprite is
 * hidden and its entry cleared and freed */
static void explode(uint32_t id, int report)
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t *w = P(uint8_t *, e, 0x16);
    int32_t attach = 0;
    uint32_t target = ((uint32_t)(uintptr_t)e & 0xffff0000) | P(uint16_t, e, 0xa);
    if (report)
        unit_set_report_kind(P(uint16_t, e, 0xa));
    if (P(int32_t, w, 0x34) > 0 && unit_attach_slot(target, &attach)) {
        int32_t slot = (int32_t)effect_slot_for(0, target);
        if (slot >= 0 && slot < P(uint16_t, g_iso, ISO_FX_SPRITES_N)) {
            uint8_t **units = I(uint8_t **, ISO_UNITS);
            if (P(uint16_t, e, 0xa) && units[P(uint16_t, e, 0xa)] && attach >= 0)
                P(int16_t, units[P(uint16_t, e, 0xa)], 0x1bf + attach * 2) = (int16_t)slot;
            int flip_x = 0, flip_y = 0;
            if (P(uint16_t, e, 8) && units[P(uint16_t, e, 8)]) {
                uint8_t f = P(uint8_t, units[P(uint16_t, e, 8)], 0x79);
                flip_x = f > 2 && f <= 6;
                flip_y = f < 4;
            }
            effect_start(P(void *, I(uint8_t *, ISO_FX_SPRITES), slot * 0xe + 6), P(int16_t, e, 0x2e) + 1,
                         P(int16_t, e, 0x22), P(int16_t, e, 0x24), P(uint32_t, w, 0x30), P(int32_t, w, 0x34), flip_x,
                         flip_y, 0, 0x18, 0);
            projectile_sound(e, 0xd2, 0, 0);
        }
    }
    SpriteHide(I(void *, ISO_HSL), P(void *, e, 0), 1);
    SpriteEmptyQueue(I(void *, ISO_HSL), P(void *, e, 0));
    SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, e, 0), 0);
    memset(I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44, 0, 0x44);
    effect_free(id);
}

/* 0x10069010: a shot's end with an explosion */
void __cdecl projectile_explode(uint32_t id) { explode(id, 0); }

/* 0x10069f63: the same, noting the target's report kind first */
void __cdecl projectile_explode_report(uint32_t id) { explode(id, 1); }

/* 0x100692fe: launches effect `id` as `weapon`'s beam or burst from `unit` at (x, y) or at
 * unit `target`: like projectile_launch, but with no flight. Weapons without frames
 * (+0x44 = 0) have no sprite; a unit hidden from view (+0x3e bit 16) fires invisibly to
 * players who are not its allies. 0 if the unit is gone or no sprite is free */
int32_t __cdecl projectile_launch_beam(uint32_t id, uint32_t unit, uint32_t x, uint32_t y, uint32_t target,
                                       uint32_t weapon)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[unit & 0xffff];
    if (!u)
        return 0;
    int32_t big = P(int32_t, P(uint8_t *, u, 0x32), 0xb4);
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t *w = table_row(0x1990, (int16_t)weapon, 4);
    int32_t sx, sy, angle = 0;
    muzzle_point(u, w, e, &sx, &sy);
    int32_t dx = (int16_t)x - sx, dy = (int16_t)y - sy;
    int32_t len = (int32_t)(int64_t)g_hypot((double)dx, (double)dy);
    if (P(int32_t, w, 0x10)) {
        angle = iso_angle(g_iso, dx, dy);
        P(int16_t, e, 0x36) = (int16_t)(0xe10 / (P(int32_t, w, 0x10) << 3));
        P(int16_t, e, 0x38) = (int16_t)(P(int16_t, e, 0x36) / 2);
        angle += P(int16_t, e, 0x38);
        if (angle > 0xe0f)
            angle -= 0xe10;
    }
    if (P(int32_t, w, 0x44)) {
        int32_t slot = (int32_t)effect_slot_take(1, 0);
        if (slot < 0 || P(uint16_t, g_iso, ISO_FX_SPRITES_N) <= slot)
            return 0;
        P(void *, e, 0) = P(void *, I(uint8_t *, ISO_FX_SPRITES), slot * 0xe + 6);
    } else {
        P(void *, e, 0) = 0;
    }
    P(void *, e, 4) = 0;
    P(uint16_t, e, 0x12) = (uint16_t)weapon;
    P(uint8_t *, e, 0x16) = w;
    P(uint8_t, e, 0x14) = P(uint8_t, u, 0x14);
    P(uint16_t, e, 8) = (uint16_t)unit;
    P(uint16_t, e, 0xa) = (uint16_t)target;
    P(uint16_t, e, 0xc) = P(uint16_t, I(uint8_t **, ISO_UNITS)[unit & 0xffff], 0x181);
    if (target & 0xffff) {
        P(uint16_t, e, 0xe) = P(uint16_t, I(uint8_t **, ISO_UNITS)[target & 0xffff], 0x52);
        P(uint16_t, e, 0x10) = P(uint16_t, I(uint8_t **, ISO_UNITS)[target & 0xffff], 0x54);
    } else {
        P(uint16_t, e, 0xe) = (uint16_t)x;
        P(uint16_t, e, 0x10) = (uint16_t)y;
    }
    P(uint8_t, e, 0x15) = P(uint8_t, u, 0x16);
    P(int16_t, e, 0x1a) = (int16_t)sx;
    P(int16_t, e, 0x1c) = (int16_t)sy;
    P(uint16_t, e, 0x1e) = (uint16_t)x;
    P(uint16_t, e, 0x20) = (uint16_t)y;
    P(int16_t, e, 0x22) = (int16_t)sx;
    P(int16_t, e, 0x24) = (int16_t)sy;
    P(uint8_t, e, 0x30) = 0;
    P(uint8_t, e, 0x34) = P(uint8_t, w, 0x44);
    if (!P(int32_t, w, 0x10)) {
        P(uint16_t, e, 0x32) = 0;
        P(uint8_t, e, 0x35) = 0;
    } else {
        int32_t per = P(int32_t, w, 0x44) ? P(int32_t, w, 0x44) : P(int32_t, w, 0x18);
        P(int16_t, e, 0x32) = (int16_t)(angle / P(int16_t, e, 0x36) * per);
        P(uint8_t, e, 0x35) = big ? P(uint8_t, u, 0x7c) : P(uint8_t, u, 0x79);
    }
    P(int16_t, e, 0x3a) = 0;
    P(int16_t, e, 0x3c) = (int16_t)(len / P(int32_t, w, 8));
    P(uint8_t, e, 0x41) = P(uint8_t, w, 0xc);
    P(uint8_t, e, 0x42) = P(uint8_t, e, 0x41);
    int hidden = P(uint32_t, u, 0x3e) >> 16 & 1;
    P(int16_t, e, 0x2e) = hidden ? 2 : 19000;
    if (hidden)
        SpriteSetBlitFX(P(void *, e, 0), 1, 0);
    if (P(int32_t, w, 0x44))
        SpriteSetLevel(I(void *, ISO_HSL), P(void *, e, 0), P(int16_t, e, 0x2e));
    if (!hidden && !P(int32_t, w, 0x58))
        projectile_sound(e, 200, 0, 0);
    int local = I(int32_t, ISO_LOCAL_PLAYER);
    int seen = P(uint8_t, e, 0x14) == (uint32_t)local ||
               (1 << (local & 0x1f) & I(uint8_t, P(uint8_t, e, 0x14) * ISO_PLAYER_SIZE + 0x1ba0));
    if (!u || !hidden || seen) {
        SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, e, 0), 1);
    } else {
        P(uint8_t, e, 0x43) |= 8;
        SpriteHide(I(void *, ISO_HSL), P(void *, e, 0), 1);
        SpriteEmptyQueue(I(void *, ISO_HSL), P(void *, e, 0));
        SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, e, 0), 0);
    }
    effect_next_frame(id);
    return 1;
}

/* 0x10069be4: a beam's step: it runs along the line from its start to its aim point in
 * +0x3c steps (a hidden firer moves with it). At the end, for kind 2, the target is hit if
 * it is still within the weapon's spread (+0x40, along the axes) of the aim point; then the
 * kind's end handler runs. 0 when it ended */
int32_t __cdecl projectile_beam_step(uint32_t id)
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    uint8_t **units = I(uint8_t **, ISO_UNITS);
    uint16_t kind = (uint16_t)P(int32_t, P(uint8_t *, e, 0x16), 0x50);
    P(int16_t, e, 0x3a)++;
    if (P(int16_t, e, 0x3a) < P(int16_t, e, 0x3c)) {
        POINT pt;
        SpriteLinePoint(P(int16_t, e, 0x3a), P(int16_t, e, 0x3c), P(int16_t, e, 0x1a), P(int16_t, e, 0x1c),
                        P(int16_t, e, 0x1e), P(int16_t, e, 0x20), &pt);
        P(int16_t, e, 0x22) = (int16_t)pt.x;
        P(int16_t, e, 0x24) = (int16_t)pt.y;
        uint8_t *firer = units[P(uint16_t, e, 8)];
        if (firer && (P(uint32_t, firer, 0x3e) >> 16 & 1)) {
            P(int16_t, firer, 0x52) = P(int16_t, e, 0x22);
            P(int16_t, firer, 0x54) = P(int16_t, e, 0x24);
        }
        return 1;
    }
    P(uint16_t, e, 0x22) = P(uint16_t, e, 0x1e);
    P(uint16_t, e, 0x24) = P(uint16_t, e, 0x20);
    if (kind == 2) {
        uint8_t *t = units[P(uint16_t, e, 0xa)];
        if (!t) {
            effect_end(id, 0, 0);
        } else {
            int32_t dx = P(int16_t, e, 0xe) - P(int16_t, t, 0x52);
            int32_t dy = P(int16_t, e, 0x10) - P(int16_t, t, 0x54);
            int too_far = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > P(int32_t, P(uint8_t *, e, 0x16), 0x40);
            if (!P(uint16_t, e, 0xa) || too_far) {
                effect_end(id, 0, 0);
            } else {
                int32_t spread;
                uint8_t *firer = units[P(uint16_t, e, 8)];
                if (!firer || !P(int32_t, P(uint8_t *, firer, 0x32), 0x34)) {
                    crt_srand(P(uint16_t, e, 0xc));
                    spread = crt_rand() % (P(int32_t, P(uint8_t *, e, 0x16), 0x40) / 2);
                } else {
                    crt_srand(P(uint16_t, firer, 0x181));
                    int32_t roll = crt_rand() % 100 + 1;
                    spread = P(int32_t, P(uint8_t *, firer, 0x32), 0x34) < roll ? 100 - roll : 0;
                }
                unit_hit_fn hit = P(unit_hit_fn, UNIT_HIT_HANDLERS, P(int32_t, units[P(uint16_t, e, 0xa)], 0xc) * 0x44);
                if (hit)
                    hit(((uint32_t)(uintptr_t)e & 0xffff0000) | P(uint16_t, e, 0xa), 0, id, spread);
                effect_end(id, 1, 0);
            }
        }
    }
    effect_end_fn on_end = P(effect_end_fn, EFFECT_END_HANDLERS, P(int32_t, P(uint8_t *, e, 0x16), 0x50) * 0x18);
    if (on_end)
        on_end(id);
    return 0;
}
