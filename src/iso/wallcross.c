/* The game logic, 0x100cdc9f-0x100ce277: walls and the patrols that cross them. Wall
 * segments are records of 0x86 bytes at iso+0xe123 (iso+0xe11f of them; +0 in use, +4/+6
 * the posts). All floats go through memory as floats between the steps, as in the
 * original. */
#include "iso_core.h"
#include "wallcross.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define LEG(u, i) ((u) + 0x1e9 + (i) * 25)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define leg_step ORIG(long double(__cdecl *)(uint32_t), 0x100cc0b9)
#define ignores_marks ORIG(int(__cdecl *)(uint8_t *), 0x100d26fb)
#define players_allied ORIG(int(__cdecl *)(uint16_t, uint16_t), 0x100d25a3)
#define segment_cross                                                                        \
    ORIG(int32_t(__cdecl *)(float *, float *, const float *, const float *, const float *, const float *), 0x10090030)
#define fn_1007abf1 ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x1007abf1)
#define walls_connect_new ORIG(void(__cdecl *)(uint32_t, uint32_t), 0x1007aa2b)

/* a word with the record pointer's upper half above it, as the original passes it */
static uint32_t hi(const void *p, uint16_t v)
{
    return ((uint32_t)(uintptr_t)p & 0xffff0000) | v;
}

/* 0x100cdc9f: checks every wall segment between two existing posts (0x100cddcd) */
void __cdecl walls_check_patrols(void)
{
    for (int32_t i = 0; i < I(int32_t, 0xe11f); i++) {
        uint8_t *seg = I(uint8_t *, 0xe123) + i * 0x86;
        if (!P(int32_t, seg, 0))
            continue;
        int16_t a = P(int16_t, seg, 4), b = P(int16_t, seg, 6);
        if (a <= 0 || a >= 0x7d0 || b <= 0 || b >= 0x7d0)
            continue;
        uint8_t *pa = UNIT(a), *pb = UNIT(b);
        if (pa && pb)
            wall_cuts_patrols(pa, pb);
    }
}

/* 0x100cddcd: whether the wall between posts `pa` and `pb` cuts across the next 128 (86
 * for soldiers) of the patrol of a unit of an allied player near either post; that unit is
 * marked (+0x1cd bit 11) and 1 is returned. Otherwise the two posts are connected
 * (0x1007aa2b) and 0 is returned. */
int32_t __cdecl wall_cuts_patrols(uint8_t *pa, uint8_t *pb)
{
    volatile float a[2], b[2], p[2], q[2], s, t, len, half;
    a[0] = (float)P(int16_t, pa, 0x52);
    a[1] = (float)P(int16_t, pa, 0x54);
    b[0] = (float)P(int16_t, pb, 0x52);
    b[1] = (float)P(int16_t, pb, 0x54);
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        uint8_t *u = UNIT(I(int32_t *, 0xd67a)[i]);
        if (!u || (P(uint32_t, u, 0x10e) >> 15 & 1) || P(int16_t, u, 0x1e7) <= 0)
            continue;
        if (P(uint16_t, u, 0x28) != 3 && P(uint16_t, u, 0x28) != 1)
            continue;
        if (ignores_marks(u))
            continue;
        if (players_allied(P(uint8_t, u, 0x14), P(uint8_t, pa, 0x14)) != 1)
            continue;
        if (sectors_near(u, pa) != 1 && sectors_near(u, pb) != 1)
            continue;
        float v[2];
        leg_vector(P(uint8_t, u, 0x1f9), v);
        long double h = leg_step(P(uint8_t, u, 0x1f9)) * (long double)G(float, 0x101132b8);
        half = (float)h;
        p[0] = (float)((long double)P(int16_t, u, 0x52) - h * v[0]);
        p[1] = (float)((long double)P(int16_t, u, 0x54) - (long double)v[1] * half);
        len = P(uint16_t, u, 0x28) == 1 ? 86.0f : 128.0f;
        for (int32_t j = 0; j < P(int16_t, u, 0x1e7) && len > G(float, 0x10113264); j++) {
            uint8_t *leg = LEG(u, j);
            if (P(float, leg, 0x11) > len) {
                leg_vector(P(uint8_t, leg, 0x10), v);
                q[0] = (float)((long double)v[0] * len + p[0]);
                q[1] = (float)((long double)v[1] * len + p[1]);
            } else {
                q[0] = P(float, leg, 8);
                q[1] = P(float, leg, 0xc);
            }
            len = (float)((long double)len - P(float, leg, 0x11));
            int32_t r = segment_cross((float *)&s, (float *)&t, (const float *)a, (const float *)b, (const float *)p,
                                      (const float *)q);
            /* (both within 0-1, as the x87 compares: unordered counts as below 0) */
            if (r > 0 && s >= G(float, 0x10113264) && !(s > G(float, 0x10113268)) && t >= G(float, 0x10113264) &&
                !(t > G(float, 0x10113268))) {
                /* (the original ignores what 0x1007abf1 returns) */
                fn_1007abf1(hi(pa, P(uint16_t, pa, 0x1cb)), hi(pb, P(uint16_t, pb, 0x1cb)));
                P(uint16_t, u, 0x1cd) |= 0x800;
                return 1;
            }
            P(uint16_t, u, 0x1cd) &= 0xf7ff;
            p[0] = q[0];
            p[1] = q[1];
        }
    }
    walls_connect_new(hi(pa, P(uint16_t, pa, 0x1cb)), hi(pb, P(uint16_t, pb, 0x1cb)));
    return 0;
}

/* 0x100ce197: whether unit `o`'s sector (+0x1a1) is unit `u`'s or one of its 8
 * neighbours (sectors: 0x1e bytes each at iso+0xc2eb, +6-+0x14 the neighbours) */
int32_t __cdecl sectors_near(uint8_t *u, uint8_t *o)
{
    int32_t su = P(int16_t, u, 0x1a1), so = P(int16_t, o, 0x1a1);
    uint8_t *sec = I(uint8_t *, 0xc2eb) + su * 0x1e;
    if (so == su)
        return 1;
    for (int k = 6; k <= 0x14; k += 2)
        if (P(int16_t, sec, k) == so)
            return 1;
    return 0;
}
