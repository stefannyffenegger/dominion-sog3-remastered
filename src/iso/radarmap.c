/* The game logic, 0x1005077d-0x10051152 (the parts every game runs): the radar's dots
 * and its scale. The radar has two DIBs (I(0xbefe), I(0xbf06)), pitch I(0xbf22); its
 * scale (map pixels per radar pixel) is I(0xd70d)/I(0xd732), the aspect I(0xd733). */
#include "iso_core.h"
#include "radarmap.h"

#define RADAR(b) I(uint8_t *, (b) ? 0xbefe : 0xbf06)
#define PITCH ((uint32_t)I(uint16_t, 0xbf22))

/* the original tests the aspect with the x87's "equal" flag, which a NaN also sets */
static int aspect_set(void)
{
    float a = I(float, 0xd733);
    return a < 0.0f || a > 0.0f;
}

/* 0x1005077d: a 2x2 dot of colour `c` at offset `at` of radar DIB `which` */
void __cdecl radar_plot(int32_t at, uint8_t c, int32_t which)
{
    uint8_t *d = RADAR(which);
    uint8_t *bits = P(uint8_t *, d, 0x20);
    int32_t size = P(int32_t, d, 8) * P(int32_t, d, 0xc);
    if (at < 0 || (int32_t)(PITCH + at + 1) >= size || !bits || !at || !aspect_set())
        return;
    bits[at] = c;
    bits[at + 1] = c;
    bits[PITCH + at] = c;
    bits[PITCH + at + 1] = c;
}

/* 0x100509b7: a 4x4 dot for an object (+4/+8 its map position, +0x27 its player,
 * +0x14 its kind) on radar DIB `which`: framed in colour 10 in the player's colour
 * (own 0xdd, allied 0xbe, enemy 0x77; in network games the player's colour from the
 * table at 0x1013a638), or cleared with `erase`. 0 */
int32_t __cdecl radar_plot_object(uint8_t *o, int32_t erase, int32_t which)
{
    int32_t p = P(int32_t, o, 0x27);
    uint8_t *d = RADAR(which);
    uint8_t *bits = P(uint8_t *, d, 0x20);
    int32_t size = P(int32_t, d, 8) * P(int32_t, d, 0xc);
    uint8_t *r = I(uint8_t *, 0xbf06);
    uint32_t x = (uint32_t)(P(int32_t, o, 4) / (int32_t)(I(uint8_t, 0xd70d) + 1)) + (P(uint32_t, r, 8) >> 1);
    uint32_t y = (uint32_t)(P(int32_t, o, 8) / (int32_t)(I(uint8_t, 0xd732) + 1)) + (P(uint32_t, r, 0xc) >> 1);
    uint8_t colour;
    if (p < 0 || p > 7) {
        colour = P(uint8_t, o, 0x14) == 3 ? 10 : 0x5b;
    } else if (!I(int32_t, 0x175) || I(uint8_t, 0xe098) == 1) {
        if (I(int32_t, 0x1b65) == p)
            colour = 0xdd;
        else if (I(int32_t, 0x1b65) == p || ((1 << (I(uint8_t, 0x1b65) & 0x1f)) & I(uint8_t, p * 0x1461 + 0x1ba0)))
            colour = 0xbe;
        else
            colour = 0x77;
    } else {
        colour = G(uint8_t, 0x1013a638 + I(int32_t, p * 0x1461 + 0x1ba2));
    }
    if (x >= P(uint32_t, r, 8) - 1 || y >= P(uint32_t, r, 0xc) - 1)
        return 0;
    int32_t at = (int32_t)(PITCH * y + x);
    if (at < 0 || (int32_t)(PITCH * 3 + at + 3) >= size || !bits || !at || !aspect_set())
        return 0;
    for (int32_t row = 0; row < 4; row++)
        for (int32_t col = 0; col < 4; col++) {
            uint8_t c;
            if (!erase)
                c = row < 1 || row > 2 || col < 1 || col > 2 ? 10 : colour;
            else
                c = 0;
            bits[PITCH * row + at + col + 0] = c;
        }
    return 0;
}

/* 0x10050fc6: the radar's scale for the map's size: the larger of the map's width and
 * twice its height over the radar's size; the aspect ratio near 1 counts as 1 */
void __cdecl radar_set_scale(void)
{
    uint8_t *r = I(uint8_t *, 0xbefe);
    if (!P(int32_t, r, 8))
        return;
    int32_t w = (I(int32_t, 0xd709) - 0x20) - (0x20 - I(int32_t, 0xd709));
    int32_t h = ((I(int32_t, 0xd71e) - 0x10) - (0x10 - I(int32_t, 0xd71e))) * 2;
    float aspect;
    if (h < w)
        aspect = (float)((long double)w / (long double)h);
    else
        aspect = (float)-(long double)(h / w);
    if ((long double)aspect > (long double)G(double, 0x10113098) &&
        (long double)aspect < (long double)G(double, 0x101130a0))
        aspect = 1.0f;
    uint32_t n;
    if (aspect > 0.0f)
        n = (uint32_t)w / P(uint32_t, r, 8) + 1;
    else
        n = (uint32_t)h / P(uint32_t, r, 0xc) + 1;
    I(uint8_t, 0xd70d) = (uint8_t)n;
    I(uint8_t, 0xd732) = (uint8_t)n;
    I(uint8_t, 0xd732) = (uint8_t)(I(uint8_t, 0xd732) >> 1);
    I(float, 0xd733) = aspect;
}

#define orig_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define orig_strstr ORIG(char *(__cdecl *)(const char *, const char *), 0x100f6aa0)
#define orig_memcpy ORIG(void *(__cdecl *)(void *, const void *, uint32_t), 0x100f5530)
#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)

#define RADAR_OK (I(void *, 0xbf1e) && I(void *, 0xbf02) && I(void *, 0xbf06) && I(void *, 0xbefe))

/* 0x10051152: the radar's background from the map: each radar pixel takes the colour
 * of its map cell's tile from the scenario's ".TLH" file (one byte per tile); the fog
 * DIB (I(0xbf02)) is set to 10 where the map is, but keeps the cells already cleared;
 * then the objects of kind 2 and 3 are drawn. 1, or 0 on an error */
int32_t __stdcall IsoCreateRadarBgndBitmap(uint8_t *iso)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x1013d7a0));
    if (!RADAR_OK)
        return 0;
    LONG size = I(LONG, 0xcda2);
    char *colours = winmalloc((SIZE_T)size, 1);
    if (!colours)
        return 0;
    char name[0x100];
    orig_strcpy(name, (const char *)(g_iso + 0xc352 + I(uint16_t, 0xc340) * 0x104));
    char *ext = orig_strstr(name, (const char *)ORIG_PTR(0x1013d7bc)); /* ".TL2" */
    if (!ext) {
        winfree(colours);
        return 0;
    }
    ext[3] = 'H';
    HMMIO f = mmioOpenA(name, NULL, 0);
    if (!f) {
        winfree(colours);
        return 0;
    }
    if (mmioRead(f, colours, size) < 1) {
        mmioClose(f, 0);
        winfree(colours);
        return 0;
    }
    mmioClose(f, 0);
    int32_t x0 = 0x20 - I(int32_t, 0xd709), y0 = 0x10 - I(int32_t, 0xd71e);
    int32_t x1 = I(int32_t, 0xd709) - 0x20, y1 = I(int32_t, 0xd71e) - 0x10;
    uint8_t *bg = I(uint8_t *, 0xbefe), *fog = I(uint8_t *, 0xbf02), *r = I(uint8_t *, 0xbf06);
    if (!aspect_set())
        radar_set_scale();
    uint32_t sx = I(uint8_t, 0xd70d), sy = I(uint8_t, 0xd732);
    int32_t bg_size = P(int32_t, bg, 0xc) * P(int32_t, bg, 8);
    uint8_t *bg_bits = P(uint8_t *, bg, 0x20), *fog_bits = P(uint8_t *, fog, 0x20);
    uint32_t fog_size = (uint32_t)(P(int32_t, fog, 8) * P(int32_t, fog, 0xc));
    uint8_t *old = winmalloc(fog_size, 1);
    if (old)
        orig_memcpy(old, fog_bits, fog_size);
    orig_memset(fog_bits, 0, fog_size);
    if (!I(int32_t, 0x154))
        orig_memset(bg_bits, 1, (uint32_t)bg_size);
    for (int32_t x = x0; x < x1; x += (int32_t)sx)
        for (uint32_t y = (uint32_t)y0; (int32_t)y < y1; y += sy) {
            int32_t cx = x, cy = (int32_t)y, a, b;
            int32_t cell = IsoVTilePos(g_iso, &cx, &cy, &a, &b);
            int32_t px = (int32_t)((P(uint32_t, r, 8) >> 1) + x / (int32_t)sx);
            int32_t py = (int32_t)((P(uint32_t, r, 0xc) >> 1) + (int32_t)y / (int32_t)sy);
            int32_t at = (int32_t)(PITCH * py + px);
            if (at >= 0 && at < bg_size) {
                bg_bits[at] = colours[I(uint32_t *, 0x1904)[cell] & 0x7fff];
                fog_bits[at] = 10;
            }
        }
    if (old) {
        for (int32_t i = 0; i < (int32_t)fog_size; i++)
            if (!old[i])
                fog_bits[i] = 0;
        winfree(old);
    }
    winfree(colours);
    for (int i = 0; i < 0x28; i++) {
        uint8_t *o = g_iso + 0x1097 + i * 0x2b;
        if (P(uint8_t, o, 0x14) == 2 || P(uint8_t, o, 0x14) == 3)
            radar_plot_object(o, 0, 1);
    }
    return 1;
}

/* 0x10051b1d: the radar's sprite and DIBs from four resources; 1 when all are there */
int32_t __stdcall IsoSetRadarBitmap(uint8_t *iso, uint32_t sprite, uint32_t fog, uint32_t radar, uint32_t bg)
{
    g_iso = iso;
    if (!RtLoad(I(void *, 0x180e), sprite))
        return 0;
    I(void *, 0xbf1e) = RtGetResource(I(void *, 0x180e), sprite)[1];
    I(void *, 0xbf02) = RtGetResource(I(void *, 0x180e), fog)[0];
    I(void *, 0xbf06) = RtGetResource(I(void *, 0x180e), radar)[0];
    I(void *, 0xbefe) = RtGetResource(I(void *, 0x180e), bg)[0];
    if (!RADAR_OK)
        return 0;
    SpriteSetFlip(I(void *, 0x181e), I(void *, 0xbf1e), 0, 1);
    P(int32_t, I(uint8_t *, 0xbf1e), 0xf0) = 1;
    I(int16_t, 0xbf22) = (int16_t)P(int32_t, I(uint8_t *, 0xbf06), 8);
    return 1;
}

/* 0x10051cb3: clears the radar's fog where the map cells in the rectangle carry one of
 * the bits `mask` (bits 24-31 of a cell: the players who have seen it). 1, or 0 */
int32_t __stdcall IsoUpdateRadarFogBitmap(uint8_t *iso, uint32_t x0, uint32_t y0, int32_t x1, int32_t y1,
                                          uint8_t mask)
{
    g_iso = iso;
    if (!RADAR_OK)
        return 0;
    uint32_t sx = I(uint8_t, 0xd70d), sy = I(uint8_t, 0xd732);
    uint8_t *fog = I(uint8_t *, 0xbf02), *r = I(uint8_t *, 0xbf06);
    int32_t fog_size = P(int32_t, fog, 8) * P(int32_t, fog, 0xc);
    int32_t cells = I(int32_t, 0x1a9c) * I(int32_t, 0x1a98);
    if (!sx || !sy)
        return 0;
    uint32_t *map = I(uint32_t *, 0x1904);
    uint8_t *bits = P(uint8_t *, fog, 0x20);
    for (uint32_t x = x0; (int32_t)x < x1; x += sx)
        for (uint32_t y = y0; (int32_t)y < y1; y += sy) {
            int32_t ax = (int32_t)x < 0 ? -(int32_t)x : (int32_t)x, ay = (int32_t)y < 0 ? -(int32_t)y : (int32_t)y;
            if (ax + 0x20 >= I(int32_t, 0xd709) || ay + 0x10 >= I(int32_t, 0xd71e))
                continue;
            int32_t cx = (int32_t)x, cy = (int32_t)y, a, b;
            int32_t cell = IsoVTilePos(g_iso, &cx, &cy, &a, &b);
            if (cell < 0 || cell >= cells)
                continue;
            int32_t px = (int32_t)((P(uint32_t, r, 8) >> 1) + (int32_t)x / (int32_t)sx);
            int32_t py = (int32_t)((P(uint32_t, r, 0xc) >> 1) + (int32_t)y / (int32_t)sy);
            int32_t at = (int32_t)(PITCH * py + px);
            if (at >= 0 && ((uint8_t)(map[cell] >> 24) & mask) && at < fog_size)
                bits[at] = 0;
        }
    return 1;
}

static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }
static double fabs_d(double v)
{
    union { double d; uint64_t u; } x = {v};
    x.u &= ~(1ull << 63);
    return x.d;
}

/* 0x1005264b: the radar's view box: a DIB the size of the screen on the radar's scale
 * (by the aspect), filled with colour 1 and with corners drawn in 0x5b and 10, appended
 * to the radar sprite. 1, or 0 when it exists already or the map has no size */
int32_t __stdcall IsoCreateRadarScreenArea(uint8_t *iso)
{
    g_iso = iso;
    if (I(void *, 0xbf0a))
        return 0;
    if (!I(int32_t, 0xd6f1) || !I(int32_t, 0xd6f5))
        return 0;
    uint8_t *bg = I(uint8_t *, 0xbefe);
    I(int32_t, 0xbf12) = ftol((long double)I(int32_t, 0xd716) / (long double)I(int32_t, 0xd6f1) *
                              (long double)(uint64_t)P(uint32_t, bg, 8));
    I(int32_t, 0xbf0e) = ftol((long double)I(int32_t, 0xd72a) / (long double)I(int32_t, 0xd6f5) *
                              (long double)(uint64_t)P(uint32_t, bg, 0xc));
    float aspect = I(float, 0xd733);
    if (aspect > 0.0f) {
        I(int32_t, 0xbf0e) = ftol((long double)(uint64_t)I(uint32_t, 0xbf0e) / (long double)aspect);
    } else {
        /* the original calls a helper that returns fabs((double)aspect) on the x87 */
        I(int32_t, 0xbf12) = ftol((long double)(uint64_t)I(uint32_t, 0xbf12) / (long double)fabs_d((double)aspect));
    }
    int32_t w = I(int32_t, 0xbf12), h = I(int32_t, 0xbf0e);
    uint8_t *d = SpriteDibCreate(w, h, 1, 8);
    /* 0x5b for scenarios 0-3; the original leaves the colour unset for others */
    uint8_t c = 0x5b;
    int32_t p = P(int32_t, d, 8);
    uint8_t *b = P(uint8_t *, d, 0x20);
    orig_memset(b, 1, (uint32_t)(P(int32_t, d, 8) * P(int32_t, d, 0xc)));
    b[0] = c;
    b[1] = c;
    b[p] = c;
    b[2] = 10;
    b[p + 1] = 10;
    b[p * 2] = 10;
    b[w - 2] = c;
    b[w - 1] = c;
    b[p + w - 1] = c;
    b[w - 3] = 10;
    b[w - 2 + p] = 10;
    b[w - 2 + p * 2] = 10;
    b[(h - 2) * p] = c;
    b[(h - 1) * p] = c;
    b[(h - 1) * p + 1] = c;
    b[(h - 2) * p + 1] = 10;
    b[(h - 3) * p] = 10;
    b[(h - 1) * p + 2] = 10;
    b[(h - 1) * p + w - 2] = c;
    b[(h - 1) * p + w - 1] = c;
    b[(h - 2) * p + w - 1] = c;
    b[(h - 1) * p + w - 3] = 10;
    b[(h - 2) * p + w - 2] = 10;
    b[(h - 3) * p + w - 1] = 10;
    I(uint8_t *, 0xbf0a) = d;
    SpriteAppendBitmap(I(void *, 0x181e), I(void *, 0xbf1e), d);
    SpriteSetFlip(I(void *, 0x181e), I(void *, 0xbf1e), 0, 1);
    return 1;
}
