/* The game logic, 0x1006ab64-0x1006be2a: instant hits, area damage, whether a unit may
 * attack another, a random point on a unit, and taking a free effect sprite. */
#include "iso_core.h"
#include "healthbar.h"
#include "sound.h"
#include "threatmap.h"
#include "weapons.h"

#define ISO_EFFECTS 0xd6db
#define ISO_FX_SPRITES 0xd6e9   /* 0xe-byte records: an id (+0), the sprite (+6), flags (+10) */
#define ISO_FX_SPRITES_N 0xd6e7 /* ushort */
#define ISO_FX_NEXT 0x1ac1      /* ushort: where the search for a free sprite starts */

#define crt_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define crt_rand ORIG(int32_t(__cdecl *)(void), 0x100f4d80)
#define unit_attach_slot ORIG(int(__cdecl *)(uint32_t, int32_t *), 0x1006cc29)
#define effect_free ORIG(void(__cdecl *)(uint32_t), 0x100a7742)
#define effect_start                                                                         \
    ORIG(void(__cdecl *)(void *, int, int, int, uint32_t, int, int, int, int, uint32_t, uint32_t), 0x1006d9bd)
#define unit_near ORIG(int(__cdecl *)(uint32_t, int, int, int, uint32_t), 0x100d979f)
#define structure_can_target ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x100d9855)
#define UNIT_HIT_HANDLERS ((uint8_t *)ORIG_PTR(0x101545c8)) /* per unit type, 0x44 apart */

typedef void(__cdecl *unit_hit_fn)(uint32_t, int, uint32_t, int32_t);
typedef void(__cdecl *area_fn)(uint32_t);

static uint8_t *unit_ptr(uint32_t id) { return I(uint8_t **, ISO_UNITS)[id & 0xffff]; }

static uint8_t *weapon_row(int16_t weapon)
{
    uint8_t *t = I(uint8_t *, 0x1990);
    return P(uint8_t *, t, 0) + P(int32_t, t, 0x2c) * weapon * 4;
}

/* the hit's spread: from the firer's accuracy (+0x34 of its type: a miss chance in percent),
 * or a random part of half the weapon's spread */
static int32_t hit_spread(uint8_t *firer, uint8_t *w)
{
    crt_srand(P(uint16_t, firer, 0x181));
    if (!P(int32_t, P(uint8_t *, firer, 0x32), 0x34))
        return crt_rand() % (P(int32_t, w, 0x40) / 2);
    int32_t roll = crt_rand() % 100 + 1;
    return P(int32_t, P(uint8_t *, firer, 0x32), 0x34) < roll ? 100 - roll : 0;
}

/* 0x1006bbee: takes a free effect sprite (one not in use, or not visible), searching from
 * where the last search ended; `keep` marks it in use. The sprite is hidden, unflipped and
 * emptied, and given the id `owner`. Its index, or -1 */
uint32_t __cdecl effect_slot_take(uint8_t keep, uint16_t owner)
{
    int32_t found = -1;
    uint16_t k = I(uint16_t, ISO_FX_NEXT);
    uint16_t end = I(uint16_t, ISO_FX_SPRITES_N);
    uint8_t *fx = I(uint8_t *, ISO_FX_SPRITES);
    for (int pass = 0;; pass++) {
        if (pass > 1)
            return 0xffffffff;
        for (; k < end && found < 0; k++) {
            if (!(P(uint8_t, fx, 10 + k * 0xe) & 1) || !SpriteIsVisible(P(void *, fx, 6 + k * 0xe)))
                found = k;
        }
        if (found >= 0)
            break;
        k = 0;
        end = (uint16_t)(I(uint16_t, ISO_FX_NEXT) - 1);
    }
    I(uint16_t, ISO_FX_NEXT)++;
    if (I(uint16_t, ISO_FX_SPRITES_N) <= I(uint16_t, ISO_FX_NEXT))
        I(uint16_t, ISO_FX_NEXT) = 0;
    if (keep & 1)
        P(uint32_t, fx, 10 + found * 0xe) |= 1;
    void *s = P(void *, fx, 6 + found * 0xe);
    SpriteHide(I(void *, ISO_HSL), s, 1);
    SpriteSetFlip(I(void *, ISO_HSL), s, 0, 0);
    SpriteEmptyQueue(I(void *, ISO_HSL), s);
    P(uint16_t, fx, found * 0xe) = owner;
    SpriteSetBlitFX(s, 0, 0);
    return (uint32_t)found;
}

/* 0x1006bb01: a random point on unit `target` (within its screen box), seeded from
 * `firer`'s serial number */
int32_t __cdecl unit_random_point(uint32_t firer, uint32_t target, int16_t *x, int16_t *y)
{
    int32_t bx, by, w, h;
    unit_screen_box(target, &bx, &by, &w, &h);
    *x = (int16_t)bx;
    *y = (int16_t)by;
    crt_srand(P(uint16_t, unit_ptr(firer), 0x181));
    int32_t rx = crt_rand();
    int32_t ry = crt_rand();
    *x = (int16_t)(*x - (int16_t)(w >> 1));
    *y = (int16_t)(*y - (int16_t)(h >> 1));
    *x = (int16_t)(*x + (int16_t)(rx % w));
    *y = (int16_t)(*y + (int16_t)(ry % h));
    return 1;
}

/* 0x1006ab64: an instant hit (a bullet: no flight) of `weapon` from `unit` on `target` at
 * (x, y): the impact animation (at a random point of a turret), the sound, the hit through
 * the target type's handler, then the effect entry is freed. 0 if the unit is gone */
int32_t __cdecl weapon_hit_instant(uint32_t id, uint32_t unit, int32_t x, int32_t y, uint32_t target,
                                   int32_t weapon)
{
    int32_t attach = 0;
    int16_t hx = (int16_t)x, hy = (int16_t)y;
    uint8_t *w = weapon_row((int16_t)weapon);
    uint8_t *u = unit_ptr(unit);
    if (!u)
        return 0;
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    unit_set_report_kind((uint16_t)target);
    if (P(int32_t, w, 0x1c) < P(uint16_t, u, 0xdc))
        P(uint16_t, u, 0xdc) = (uint16_t)(P(uint16_t, u, 0xdc) - P(int32_t, w, 0x1c));
    else
        P(uint16_t, u, 0xdc) = 0;
    P(uint8_t, u, 0xee) = P(uint8_t, w, 0x20);
    if (unit_attach_slot(target, &attach) && P(int32_t, w, 0x34) > 0) {
        int32_t slot = (int32_t)effect_slot_take(0, (uint16_t)target);
        if (slot >= 0 && slot < I(uint16_t, ISO_FX_SPRITES_N)) {
            uint8_t *t = unit_ptr(target);
            if ((target & 0xffff) && t && attach >= 0) {
                P(int16_t, t, 0x1bf + attach * 2) = (int16_t)slot;
                if (P(int32_t, t, 0xc) >= 0x30 && P(int32_t, t, 0xc) <= 0x37)
                    unit_random_point(unit, target, &hx, &hy);
            }
            int flip_x = 0, flip_y = 0;
            if ((unit & 0xffff) && u) {
                flip_x = P(uint8_t, u, 0x79) > 2 && P(uint8_t, u, 0x79) <= 6;
                flip_y = P(uint8_t, u, 0x79) < 4;
            }
            effect_start(P(void *, I(uint8_t *, ISO_FX_SPRITES), slot * 0xe + 6), 29000, hx, hy,
                         P(uint32_t, w, 0x30), P(int32_t, w, 0x34), flip_x, flip_y, 0, 0x18, 0);
        }
    }
    P(uint8_t *, e, 0x16) = w;
    P(int16_t, e, 0x22) = hx;
    P(int16_t, e, 0x24) = hy;
    P(uint16_t, e, 8) = (uint16_t)unit;
    projectile_sound(e, 200, 0, 0);
    int32_t spread = hit_spread(u, P(uint8_t *, e, 0x16));
    uint8_t *t = unit_ptr(target);
    if (t) {
        unit_hit_fn hit = P(unit_hit_fn, UNIT_HIT_HANDLERS, P(int32_t, t, 0xc) * 0x44);
        if (hit)
            hit(target, 0, id, spread);
    }
    memset(I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44, 0, 0x44);
    effect_free(id);
    return 1;
}

/* 0x1006b00a: area damage where effect `id` is: every unit in the sector and its eight
 * neighbours within the weapon's spread (+0x40) is hit (not the firer, nor with `skip_target`
 * the target; not raised turrets, nor units flagged 0x10e bit 15), through `fn` or the unit
 * type's handler. The upper bits of the unit ids passed on were left over on the original's
 * stack; ours are 0 */
void __cdecl effect_area_hit(uint32_t id, int skip_target, void(__cdecl *fn)(uint32_t))
{
    uint8_t *e = I(uint8_t *, ISO_EFFECTS) + (id & 0xffff) * 0x44;
    int32_t cx = (I(int32_t, 0xd6f1) >> 1) + P(int16_t, e, 0x22);
    int32_t cy = P(int16_t, e, 0x24) + (I(int32_t, 0xd6f5) >> 1);
    int32_t sector = ((cy >> 8) << (I(uint8_t, 0xc2f5) & 0x1f)) + (cx >> 8);
    uint8_t *sectors = I(uint8_t *, 0xc2eb); /* 0x1e bytes each: first unit (+2), neighbours (+6) */
    for (int k = 0; k < 9; k++) {
        int32_t s = k == 0 ? sector : P(int16_t, sectors, sector * 0x1e + 4 + k * 2);
        if (s < 0 || I(int32_t, 0xc2f1) <= s)
            continue;
        uint32_t uid = P(uint16_t, sectors, s * 0x1e + 2);
        while (uid & 0xffff) {
            uint8_t *u = unit_ptr(uid);
            if (!u)
                break;
            uint16_t next = P(uint16_t, u, 0x393);
            int low = P(int32_t, u, 0xc) < 0x30 || P(int32_t, u, 0xc) > 0x37 ||
                      !(P(float, P(uint8_t *, u, 0x39d), 0x20) > G(float, 0x101130b8));
            if (low && P(uint16_t, e, 8) != (uid & 0xffff) && (!skip_target || P(uint16_t, e, 0xa) != (uid & 0xffff)) &&
                (fn || P(unit_hit_fn, UNIT_HIT_HANDLERS, P(int32_t, u, 0xc) * 0x44)) &&
                !(P(uint32_t, u, 0x10e) >> 15 & 1)) {
                int32_t bx, by, bw, bh;
                unit_screen_box(uid, &bx, &by, &bw, &bh);
                if (unit_near(uid, P(int32_t, P(uint8_t *, e, 0x16), 0x40), P(int16_t, e, 0x22), P(int16_t, e, 0x24),
                              1)) {
                    crt_srand((uint16_t)(P(uint16_t, e, 8) & uid));
                    int32_t r = crt_rand() % P(int32_t, P(uint8_t *, e, 0x16), 0x40);
                    if (fn)
                        fn(uid);
                    else
                        P(unit_hit_fn, UNIT_HIT_HANDLERS, P(int32_t, u, 0xc) * 0x44)(uid, 0, id, r);
                }
            }
            uid = next;
        }
    }
}

/* 0x1006b6d3: whether `attacker` may attack `target`. `no_structures`: 0 lets units of
 * kind 0x10-0x13 attack structures; `allies`: allies may be attacked */
int32_t __cdecl unit_can_attack(uint32_t attacker, uint32_t target, int no_structures, int allies)
{
    uint8_t *a = unit_ptr(attacker), *t = unit_ptr(target);
    if (!a || !t)
        return 0;
    int friendly = P(uint8_t, t, 0x14) == P(uint8_t, a, 0x14) ||
                   (1 << (P(uint8_t, t, 0x14) & 0x1f) & I(uint8_t, P(uint8_t, a, 0x14) * ISO_PLAYER_SIZE + 0x1ba0));
    int32_t at = P(int32_t, a, 0xc), ak = P(int32_t, a, 0x10), tk = P(int32_t, t, 0x10);
    if (at == 0x21) /* the medic: friendly men only */
        return friendly && ((tk >= 0x18 && tk <= 0x2c && tk != 0x20) || tk == 0x38) && P(char, t, 0x147) != 'E' &&
               P(int32_t, t, 0x84) == 0;
    if (P(int32_t, t, 0x84) == 1 || (P(uint32_t, t, 0x10e) >> 15 & 1))
        return 0;
    if ((at >= 0x50 && at <= 0x9f) || (at >= 0x40 && at <= 0x43))
        return 0;
    if (friendly && !allies)
        return 0;
    if (P(uint32_t, t, 0x3e) >> 16 & 1)
        return 0;
    if (ak >= 0x10 && ak <= 0x13)
        return !no_structures && tk >= 0x40 && tk <= 0x9f;
    if (P(int32_t, P(uint8_t *, a, 0x32), 0xe0) && !P(uint8_t, a, 0xf3) && P(int16_t, a, 0xe6) < 0)
        return 0;
    if (ak >= 0x40 && ak <= 0x4f && !structure_can_target(attacker, target))
        return 0;
    if (P(int16_t, a, 0xe6) < 0)
        return 0;
    int32_t kind = P(int32_t, weapon_row(P(int16_t, a, 0xe6)), 0x50);
    uint8_t ttype = P(uint8_t, t, 0xc);
    int ground = ttype >= 0x18 && ttype < 0x40;
    int turret = ttype >= 0x30 && ttype <= 0x37;
    if (kind == 5 && !(P(uint8_t, t, 0x14) != P(uint8_t, a, 0x14) && ground && !turret))
        return 0;
    if (kind == 4 && !((ground && !turret && !P(uint8_t, t, 0xf3)) || (ttype >= 0x40 && ttype <= 0x4f)))
        return 0;
    return !(P(uint32_t, a, 0x3e) >> 16 & 1) && !(P(uint32_t, a, 0x10e) >> 4 & 1);
}

/* 0x1006b633: whether any of the local player's selected units may attack `target` */
int32_t __cdecl selection_can_attack(uint32_t target, int no_structures, int allies)
{
    int32_t player = I(int32_t, ISO_LOCAL_PLAYER);
    int32_t n = I(int32_t, ISO_SELECTED_N + player * 4);
    int32_t any = 0;
    for (int32_t k = 0; k < n && !any; k++)
        any |= unit_can_attack(I(uint16_t *, ISO_SELECTED + I(int32_t, ISO_LOCAL_PLAYER) * 4)[k], target,
                               no_structures, allies);
    return any;
}

/* ---- 0x1006be2a-0x1006c6f6 (part 71): the weapon table and firing ---- */

#define effect_alloc ORIG(uint32_t(__cdecl *)(void), 0x100a751c)
#define effect_ready ORIG(void(__cdecl *)(uint32_t), 0x100a76e3)
#define unit_set_anim ORIG(void(__cdecl *)(uint32_t, int), 0x10076d00)
#define attack_alert ORIG(void(__cdecl *)(int32_t, int32_t, uint32_t, int, int, int, int), 0x10090223)
#define LAUNCHERS ((uint8_t *)ORIG_PTR(0x10157148)) /* per effect kind, 0x18 apart */
#define STEPPERS ((uint8_t *)ORIG_PTR(0x1015714c))

typedef int32_t(__cdecl *launch_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t(__cdecl *step_fn)(uint32_t, int32_t);

static void free_fx_sprites(void)
{
    if (!I(uint8_t *, ISO_FX_SPRITES))
        return;
    for (uint32_t k = 0; k < I(uint16_t, ISO_FX_SPRITES_N); k++) {
        uint8_t *s = P(uint8_t *, I(uint8_t *, ISO_FX_SPRITES), 6 + k * 0xe);
        if (s)
            RtDelete(I(void *, ISO_APP), P(uint32_t, s, 0x94));
    }
    winfree(I(uint8_t *, ISO_FX_SPRITES));
    I(uint8_t *, ISO_FX_SPRITES) = 0;
}

/* IsoInitWeaponTable: loads the weapons' sprite resource and makes `count` effect sprites
 * (copies of the template 0xc33b). Returns `count`, or 0 */
uint32_t __stdcall IsoInitWeaponTable(uint8_t *iso, uint32_t res, uint32_t count)
{
    g_iso = iso;
    RtLoad(P(void *, iso, ISO_APP), res);
    I(void *, 0x1822) = RtGetResource(I(void *, ISO_APP), res)[1];
    if (!res || !I(void *, 0x1822))
        return 0;
    free_fx_sprites();
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x101405e8), count * 0xe);
    OutputDebugStringA((const char *)(g_iso + 0xdea));
    I(uint8_t *, ISO_FX_SPRITES) = winmalloc(count * 0xe, 1);
    I(uint16_t, ISO_FX_SPRITES_N) = (uint16_t)count;
    if (I(uint8_t *, ISO_FX_SPRITES)) {
        for (uint32_t k = 0; k < count; k++) {
            uint32_t copy = RtCopy(I(void *, ISO_APP), I(uint32_t, 0xc33b));
            RtLoad(I(void *, ISO_APP), copy);
            P(void *, I(uint8_t *, ISO_FX_SPRITES), 6 + k * 0xe) = RtGetResource(I(void *, ISO_APP), copy)[1];
            P(uint32_t, I(uint8_t *, ISO_FX_SPRITES), 10 + k * 0xe) &= 0xfffffffe;
            SpriteSetProcessMsgs(I(void *, ISO_HSL), P(void *, I(uint8_t *, ISO_FX_SPRITES), 6 + k * 0xe), 0);
            I(uint32_t, 0x1858) = copy;
        }
    }
    return count;
}

/* IsoFreeWeaponTable */
void __stdcall IsoFreeWeaponTable(uint8_t *iso)
{
    g_iso = iso;
    free_fx_sprites();
}

/* fires the unit's weapon (+0xe6): as many effects as the weapon's shots (+0x54), each
 * launched by its kind's launcher. 0 if a launcher fails */
static int fire(uint8_t *u, uint32_t attacker, uint32_t target, uint32_t x, uint32_t y, uint32_t weapon)
{
    P(uint16_t, u, 0x193) |= 0x10;
    uint8_t *w = weapon_row(P(int16_t, u, 0xe6));
    int32_t shots = P(int32_t, w, 0x54);
    for (int32_t k = 0; k < shots; k++) {
        uint32_t fx = effect_alloc() & 0xffff;
        launch_fn launch = P(launch_fn, LAUNCHERS, P(int32_t, w, 0x50) * 0x18);
        if (launch && !launch(fx, attacker, x, y, target, weapon))
            return 0;
        effect_ready(fx);
    }
    return 1;
}

/* 0x1006c18a: `attacker` fires at unit `target` (aiming at x, y); outside network games
 * the target's player is warned if it cannot see the attacker. Nonzero if it fired */
int32_t __stdcall weapon_fire_at_unit(uint8_t *iso, int32_t attacker, int32_t target, uint32_t x, uint32_t y)
{
    g_iso = iso;
    uint8_t *a = P(uint8_t **, iso, ISO_UNITS)[attacker];
    uint8_t *t = P(uint8_t **, iso, ISO_UNITS)[target];
    if (!a || !t)
        return 0;
    uint32_t hi = (uint32_t)(uintptr_t)a & 0xffff0000;
    P(uint32_t, a, 0x10e) &= 0xffff7fff;
    unit_set_anim(hi | P(uint16_t, a, 0x1cb), 2);
    if (P(int16_t, a, 0xe6) < 0)
        return 0;
    if (!fire(a, (uint32_t)attacker, (uint32_t)target, x, y, hi | P(uint16_t, a, 0xe6)))
        return 0;
    if (!I(int, 0x175) && !(P(uint8_t, a, 0x2f) & 1 << (P(uint8_t, t, 0x14) & 0x1f)))
        attack_alert(P(int16_t, a, 0x52), P(int16_t, a, 0x54),
                     ((uint32_t)(uintptr_t)t & 0xffff0000) | P(uint8_t, t, 0x14), 2, -1, 0, 0);
    return 1;
}

/* 0x1006c375: `attacker` fires at the point (x, y). Always 0 */
int32_t __stdcall weapon_fire_at_point(uint8_t *iso, int32_t attacker, uint32_t x, uint32_t y)
{
    g_iso = iso;
    uint8_t *a = P(uint8_t **, iso, ISO_UNITS)[attacker];
    P(uint32_t, a, 0x10e) &= 0xffff7fff;
    unit_set_anim(((uint32_t)(uintptr_t)a & 0xffff0000) | P(uint16_t, a, 0x1cb), 2);
    int16_t weapon = P(int16_t, a, 0xe6);
    if (weapon == -1 || weapon < 0)
        return 0;
    fire(a, (uint32_t)attacker, 0, x, y, (uint16_t)weapon);
    return 0;
}

/* IsoSetWeaponDelay: does nothing */
void __stdcall IsoSetWeaponDelay(uint8_t *iso, uint32_t delay)
{
    (void)delay;
    g_iso = iso;
}

/* IsoInitWeaponProcess: trail sprites last 30 frames */
int32_t __stdcall IsoInitWeaponProcess(uint8_t *iso)
{
    g_iso = iso;
    P(int32_t, iso, 0xd897) = 0x1e;
    return 1;
}

/* IsoStopWeaponProcess */
void __stdcall IsoStopWeaponProcess(uint8_t *iso)
{
    g_iso = iso;
    P(int32_t, iso, 0xd897) = 0;
}

/* IsoWeaponMovementLoop: unless paused, every effect in play takes as many steps as game
 * ticks have passed (trails only when there are at most 2). The `IsoKaboom` flag (0xe020)
 * crashes the game here on purpose, as in the original */
void __stdcall IsoWeaponMovementLoop(uint8_t *iso, uint32_t unused)
{
    (void)unused;
    g_iso = iso;
    if (P(uint8_t, iso, 0x17e) || P(uint8_t, iso, 0x180))
        return;
    if (P(int32_t, iso, 0xe020))
        ORIG(void *(__cdecl *)(void *, const void *, uint32_t), 0x100f5530)(0, 0, 1);
    uint32_t ticks = I(uint32_t, 0x17f1) - I(uint32_t, 0x17ed);
    for (uint16_t k = 0; k < I(int32_t, 0x1b47); k++) {
        uint32_t fx = I(uint32_t *, 0xd6e3)[k];
        for (uint16_t s = 0; s < ticks; s++) {
            uint8_t *e = I(uint8_t *, ISO_EFFECTS) + fx * 0x44;
            if (!P(void *, e, 0))
                continue;
            step_fn step = P(step_fn, STEPPERS, P(int32_t, P(uint8_t *, e, 0x16), 0x50) * 0x18);
            if (step)
                step(fx, ticks < 3);
        }
    }
    I(uint32_t, 0x17ed) += ticks;
}
