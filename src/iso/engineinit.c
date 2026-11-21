/* The game logic: IsoInitEngine (0x100a7f17-0x100a9195), the start of a game: settings from
 * dominion.ini, the game state cleared, the shadow and highlight tables, the random seed, and
 * the unit, object, marker, timer, vector and slot tables allocated. */
#include "iso_core.h"
#include "debugtime.h"
#include "view.h"
#include "trig.h"
#include "clut.h"
#include "engineinit.h"

#define IsoSetDebugMode ORIG(void(__stdcall *)(uint8_t *, int), 0x100aa03b)
#define IsoDeleteAllUnits ORIG(void(__stdcall *)(uint8_t *), 0x100ac51d)
#define DPGetGameData ORIG(int(__cdecl *)(uint32_t, void *, void *, void *, void *), 0x1011208c)
#define orig_srand ORIG(void(__cdecl *)(unsigned), 0x100f4d60)
#define STR(a) ((const char *)ORIG_PTR(a))

static UINT ini_int(uint32_t app, uint32_t key, int def, uint32_t file)
{
    return GetPrivateProfileIntA(STR(app), STR(key), def, STR(file));
}

static void load_res(uint32_t id, uint32_t keep, uint32_t table)
{
    RtLoad(I(void *, ISO_APP), id);
    I(uint32_t, keep) = id;
    I(uint32_t, table) = *(uint32_t *)RtGetResource(I(void *, ISO_APP), id);
}

/* allocates `size` zeroed bytes at iso+off, freeing what was there */
static void *realloc_at(uint32_t off, uint32_t size)
{
    if (I(void *, off)) {
        winfree(I(void *, off));
        I(void *, off) = 0;
    }
    return I(void *, off) = winmalloc(size, 1);
}

/* IsoInitEngine: 1, or a negative code for the table that could not be allocated */
int32_t __stdcall IsoInitEngineC(uint8_t *iso, uint32_t types, uint32_t res3, uint32_t res4, uint32_t res5,
                                 uint32_t res6, uint32_t res7, uint32_t res8, uint32_t res9, int32_t sprites,
                                 uint32_t v179b, uint32_t v1017df54)
{
    (void)res4;
    (void)res7;
    g_iso = iso;
    iso_debug_print();
    G(uint32_t, 0x1013d6ec) = 0;
    G(uint32_t, 0x10153bd0) = 0;
    G(uint32_t, 0x1013d6f0) = 0;
    G(uint32_t, 0x1013d6f4) = 0;
    G(uint32_t, 0x10118d84) = 0;
    G(uint32_t, 0x1017baf8) = 0;
    I(int32_t, 0x1c) = -1;
    I(uint32_t, 0x20) = 0;
    I(uint32_t, 0x24) = 0;
    I(uint32_t, 0x28) = 0;
    I(uint32_t, 0x114) = 1;
    I(uint32_t, 0x11c) = 1;
    I(uint32_t, 300) = 0;
    I(uint32_t, 0x158) = 0;
    I(uint32_t, 0x150) = 0;
    I(uint32_t, 0x1077) = 0;
    I(uint32_t, 0x179b) = v179b;
    G(uint32_t, 0x1017df54) = v1017df54;
    I(UINT, 0x10c) = ini_int(0x10157454, 0x10157448, 0, 0x10157438);   /* DebuggingOn */
    I(UINT, 0x107b) = ini_int(0x10157480, 0x10157470, 1000, 0x10157460); /* RadarUpdateTime */
    I(UINT, 0x1083) = ini_int(0x101574ac, 0x1015749c, 0, 0x1015748c);   /* AutoSaveTime */
    I(uint32_t, 0x107f) = timeGetTime();
    {
        uint32_t a, b, c, d;
        DPGetGameData(I(uint32_t, 0x1816), &a, &b, &c, &d);
    }
    I(uint32_t, 0x138) = 0;
    I(UINT, 0xe12b) = ini_int(0x101574dc, 0x101574c8, 0, 0x101574b8); /* KeyboardScrolling */
    I(uint32_t, 0xd733) = 0;
    I(uint32_t, 0x124) = 0;
    I(uint32_t, 0x144) = 0;
    I(int32_t, ISO_HAVE_SPRITES) = sprites;
    if (I(int32_t, ISO_HAVE_SPRITES))
        I(uint8_t, 0x17e) = 1;
    I(uint8_t, 0x180) = 0;
    I(uint32_t, 0xa06) = 0;
    I(uint8_t, 0x174) &= 0xf7;
    I(uint8_t, 0x174) &= 0xef;
    I(uint8_t, 0x174) &= 0xfe;
    I(uint32_t, 0x17cd) = 0;
    I(uint32_t, 0x17d1) = 0;
    I(uint32_t, 0x17d5) = 0;
    I(uint32_t, 0xbeda) = 0;
    I(uint32_t, 0x17c9) = 0;
    I(uint32_t, 0x140) = 0;
    I(uint32_t, 0xd883) = 0;
    I(uint32_t, 0xd853) = 0;
    I(uint32_t, 0xd84b) = 0;
    I(uint32_t, 0xd83b) = 0;
    I(uint32_t, 0x16c) = 0;
    iso_debug_print();
    iso_debug_print();
    I(int32_t, 0x1832) = -1;
    I(int32_t, 0x1836) = -1;
    I(uint32_t, 0x183a) = 99;
    memset(g_iso + 0x183a, 0xff, 0x10);
    SpriteSetBkgTransparent(I(void *, 0x181e), 1);
    memset(g_iso + 0x1b69, 0, 0xa308);
    if (!I(int32_t, ISO_HAVE_SPRITES))
        for (int p = 0; p < 8; p++) {
            I(uint8_t *, p * ISO_PLAYER_SIZE + 0x2dfd) = winmalloc(1, 1);
            *I(uint8_t *, p * ISO_PLAYER_SIZE + 0x2dfd) = 0;
        }
    if (I(int32_t, 0x175))
        for (int p = 0; p < 8; p++)
            I(uint32_t, p * ISO_PLAYER_SIZE + 0x1b69) = 1;
    memset(g_iso + 0x2c, 0, 0xe0);
    memset(g_iso + 0xcee6, 0, 0x70);
    memset(g_iso + 0xcf56, 0, 0x700);
    memset(g_iso + 0xd6cb, 0, 0x10);
    memset(g_iso + 0xd6f9, 0, 0x10);
    I(uint32_t, 0x17f5) = 0;
    IsoSetDebugMode(iso, 0);
    SpriteSetSpotColor(I(void *, 0x181e), 0);
    I(void *, 0xc31b) = RtGetShadowTable(I(void *, ISO_APP));
    I(void *, 0x1802) = RtGetHiliteTable(I(void *, ISO_APP));
    if (!I(void *, 0xc31b) || !I(void *, 0x1802))
        DebugStr(STR(0x10157538));
    view_set_scroll_box();
    orig_srand(1); /* the original's C library keeps the random state */
    I(uint32_t, 0xd6df) = 0;
    I(uint32_t, 0x17dd) = 0;
    I(uint32_t, 0x1093) = 0;
    I(uint32_t, 0x17a3) = 0;
    I(uint32_t, 0xc2ff) = 0;
    load_res(types, 0xced6, 0x197c);
    if (I(void *, ISO_UNITS)) {
        IsoDeleteAllUnits(iso);
        winfree(I(void *, ISO_UNITS));
        I(void *, ISO_UNITS) = 0;
    }
    if (!(I(void *, ISO_UNITS) = winmalloc(8000, 1)))
        return -0x28;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x10157558), 8000);
    iso_debug_print();
    wsprintfA((char *)(g_iso + 0xdea), STR(0x10157594), 0x3a9);
    iso_debug_log();
    wsprintfA((char *)(g_iso + 0xdea), STR(0x101575ac), 0xe12f);
    iso_debug_log();
    I(uint32_t, 0xd67e) = 0;
    if (!realloc_at(0xd6db, 0x4400))
        return -0x82;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x101575c0), 0x4400);
    iso_debug_print();
    if (I(void *, 0xd6e3))
        winfree(I(void *, 0xd6e3));
    I(void *, 0xd6e3) = winmalloc(0x400, 1);
    memset(I(void *, 0xd6e3), 0xff, 0x400);
    I(uint32_t, 0x1b47) = 0;
    memset(I(void *, ISO_UNITS), 0, 4);
    I(uint32_t, ISO_UNIT_COUNT) = 0;
    I(uint32_t, 0x1ad1) = 1;
    I(uint32_t, 0xd67e) = 0;
    if (!realloc_at(ISO_UNIT_LIST, 8000))
        return -0x32;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x101575fc), 0x4400); /* the original prints the previous size */
    iso_debug_print();
    realloc_at(0x192c, 0x9000);
    I(uint32_t, 0x1acd) = 0;
    if (!I(void *, 0x192c))
        return -0x78;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x1015762c), 0x9000, 0x800);
    iso_debug_print();
    realloc_at(0xcd9e, 0x340);
    I(uint32_t, 0x1ad5) = 0;
    if (!I(void *, 0xcd9e))
        return -0x96;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x1015766c), 0x340, 0x20);
    iso_debug_print();
    load_res(res8, 0xcda6, 0x18f8);
    load_res(res9, 0xceca, 0x1930);
    load_res(res6, 0xced2, 0x1964);
    if (!realloc_at(0x1984, 0x12000))
        return -0x6e;
    wsprintfA((char *)(g_iso + 0xdea), STR(0x101576ac), 0x12000, 0x1000);
    iso_debug_print();
    I(uint32_t, 0xd697) = 0x1000;
    I(uint32_t, 0x1ad9) = 1;
    for (int32_t k = 1; k < 0xfff; k++)
        P(int16_t, I(uint8_t *, 0x1984), 0xe + k * 0x12) = (int16_t)(k + 1);
    iso_debug_print();
    if (sprites == 1) {
        I(uint32_t, 0x13c) = 1;
        I(UINT, 0x120) = ini_int(0x10157730, 0x1015771c, 1, 0x1015770c); /* BrightnessEffects */
        if (I(int32_t, 0x13c) && !IsoLoadClutC(iso, 0x28))
            I(uint32_t, 0x13c) = 0;
        if (I(int32_t, 0x120) && !IsoLoadBlutC(iso))
            I(uint32_t, 0x120) = 0;
        load_res(res3, 0xcede, 0x1990);
        load_res(res5, 0xceda, 0x1988);
        if (!realloc_at(0x1938, 0x800))
            return -0x64;
        wsprintfA((char *)(g_iso + 0xdea), STR(0x1015773c), 0x800);
        iso_debug_print();
        for (int p = 0; p < 8; p++) {
            I(uint32_t, p * 4 + 0x1b23) = 0;
            I(uint8_t *, p * 4 + 0x193c) = I(uint8_t *, 0x1938) + p * 0x200;
        }
        if (!realloc_at(0x18e8, 0x1600))
            return -0x8c;
        wsprintfA((char *)(g_iso + 0xdea), STR(0x10157770), 0x1600, 0x200);
        iso_debug_print();
        for (int32_t k = 0; k < 0x200; k++) {
            P(int16_t, I(uint8_t *, 0x18e8), 9 + k * 0xb) = (int16_t)(k + 1);
            P(int16_t, I(uint8_t *, 0x18e8), 7 + k * 0xb) = (int16_t)(k - 1);
        }
        P(int16_t, I(uint8_t *, 0x18e8), 0x15fe) = -1;
        I(int16_t, 0x1066) = -1;
        I(int16_t, 0x1068) = -1;
        I(int16_t, 0x1acb) = 0;
    } else {
        I(uint32_t, 0x1990) = 0;
        I(uint32_t, 0x1988) = 0;
        I(uint32_t, 0x1938) = 0;
        for (int p = 0; p < 8; p++) {
            I(uint32_t, p * 4 + 0x193c) = 0;
            I(uint32_t, p * 4 + 0xd817) = 0;
        }
    }
    for (int p = 0; p < 8; p++)
        for (int s = 0; s < 4; s++) {
            memset(g_iso + s * 0x31 + p * 0xc4 + 0x2ca, 0, 0x31);
            I(int32_t, s * 0x31 + p * 0xc4 + 0x2ca) = -1;
        }
    I(uint8_t, 0x1abf) = 0xff;
    I(uint8_t, 0x1abe) = I(uint8_t, 0x1abf);
    for (int k = 0; k < 5; k++)
        I(int16_t, k * 0xc0 + 0xae0) = -1;
    I(uint32_t, 0xd857) = 0;
    I(uint32_t, 0xd86b) = 0;
    I(uint32_t, 0xe113) = 0;
    I(uint32_t, 0xe117) = 0;
    I(uint32_t, 0xd8bf) = 0;
    I(uint32_t, 0xd8af) = I(uint32_t, 0xd8bf);
    I(uint8_t, 0xd90f) = 0;
    I(uint8_t, 0xd90e) = I(uint8_t, 0xd90f);
    I(uint32_t, 0x168) = 0;
    colormap_load();
    I(uint8_t, 0xd947) = 0;
    I(uint32_t, 0xd94a) = 0;
    I(uint32_t, 0xd7fb) = timeGetTime();
    return 1;
}
