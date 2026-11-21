/* The game logic, 0x100d1e0f-0x100d2319: the units standing on the line of a planned wall.
 * Each unit counts as a short segment across the wall (half a step each way, at right
 * angles to it). Floats go through memory as floats, as in the original. */
#include "iso_core.h"
#include "wallunits.h"

#define UNIT(k) (I(uint8_t **, ISO_UNITS)[k])
#define wall_direction ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x10005ef5)
#define leg_step ORIG(long double(__cdecl *)(uint32_t), 0x100cc0b9)
#define leg_vector ORIG(void(__cdecl *)(uint32_t, float *), 0x100cba36)
#define ignores_marks ORIG(int(__cdecl *)(uint8_t *), 0x100d26fb)
#define players_allied ORIG(int(__cdecl *)(uint16_t, uint16_t), 0x100d25a3)
#define sectors_near ORIG(int32_t(__cdecl *)(uint8_t *, uint8_t *), 0x100ce197)
#define segment_cross                                                                        \
    ORIG(int32_t(__cdecl *)(float *, float *, const float *, const float *, const float *, const float *), 0x10090030)

typedef struct {
    volatile float a[2], b[2], hx, hy;
} WallLine;

static uint32_t hi(const void *p, uint16_t v)
{
    return ((uint32_t)(uintptr_t)p & 0xffff0000) | v;
}

static void line_init(WallLine *w, uint8_t *pa, uint8_t *pb)
{
    w->a[0] = (float)P(int16_t, pa, 0x52);
    w->a[1] = (float)P(int16_t, pa, 0x54);
    w->b[0] = (float)P(int16_t, pb, 0x52);
    w->b[1] = (float)P(int16_t, pb, 0x54);
    int16_t along = (int16_t)wall_direction(hi(pa, P(uint16_t, pa, 0x1cb)), hi(pb, P(uint16_t, pb, 0x1cb)));
    uint8_t across = (uint8_t)((along + 2) & 7);
    volatile float half = (float)(leg_step(across) * (long double)G(float, 0x101132b8));
    float v[2];
    leg_vector(across, v);
    w->hx = (float)((long double)v[0] * half);
    w->hy = (float)((long double)v[1] * half);
}

/* whether unit `o`'s short segment crosses the wall */
static int line_hits(WallLine *w, uint8_t *o)
{
    volatile float p[2], q[2], s, t;
    p[0] = (float)((long double)P(int16_t, o, 0x52) - w->hx);
    p[1] = (float)((long double)P(int16_t, o, 0x54) - w->hy);
    q[0] = (float)((long double)P(int16_t, o, 0x52) + w->hx);
    q[1] = (float)((long double)P(int16_t, o, 0x54) + w->hy);
    if (segment_cross((float *)&s, (float *)&t, (const float *)w->a, (const float *)w->b, (const float *)p,
                      (const float *)q) <= 0)
        return 0;
    /* (as the x87 compares: unordered counts as below 0) */
    return s >= G(float, 0x10113264) && !(s > G(float, 0x10113268)) && t >= G(float, 0x10113264) &&
           !(t > G(float, 0x10113268));
}

/* 0x100d1e0f: whether a unit allied with `pa`'s player, near either post, stands on the line
 * between posts `pa` and `pb` */
int32_t __cdecl posts_too_close(uint8_t *pa, uint8_t *pb)
{
    WallLine w;
    line_init(&w, pa, pb);
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        uint8_t *o = UNIT(I(int32_t *, 0xd67a)[i]);
        if (!o || o == pa || o == pb || ignores_marks(o))
            continue;
        if (players_allied(P(uint8_t, o, 0x14), P(uint8_t, pa, 0x14)) != 1)
            continue;
        if (sectors_near(o, pa) != 1 && sectors_near(o, pb) != 1)
            continue;
        if (line_hits(&w, o))
            return 1;
    }
    return 0;
}

/* 0x100d2083: lists in `ids` (at most 20) the soldiers and vehicles not allied with `pa`'s
 * player, near either post, that stand on the line between the posts; their number */
int32_t __cdecl units_between(uint8_t *pa, uint8_t *pb, int32_t *ids)
{
    WallLine w;
    int32_t count = 0;
    line_init(&w, pa, pb);
    int32_t n = I(int32_t, 0x1b43);
    for (int32_t i = 0; i < n; i++) {
        int32_t id = I(int32_t *, 0xd67a)[i];
        uint8_t *o = UNIT(id);
        if (!o || (P(uint16_t, o, 0x28) != 3 && P(uint16_t, o, 0x28) != 1) || ignores_marks(o))
            continue;
        if (players_allied(P(uint8_t, o, 0x14), P(uint8_t, pa, 0x14)))
            continue;
        if (sectors_near(o, pa) != 1 && sectors_near(o, pb) != 1)
            continue;
        if (line_hits(&w, o) && count < 0x14)
            ids[count++] = id;
    }
    return count;
}
