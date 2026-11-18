/* The game logic, 0x1001d92f-0x1001e25a: IsoCreateMap, a new map of w x h cells:
 * the view's size in cells for the screen width, the map arrays, the tiles (from the
 * scenario's tile library, or the editor's autofill), the path finder and the AI. */
#include "iso_core.h"
#include "tilelib.h"
#include "mapedit.h"
#include "createmap.h"
#include "tables.h"

/* not rewritten yet (names ours, provisional) */
#define map_reset_objects map_neighbour_offsets
#define log2_of ORIG(int32_t(__stdcall *)(uint32_t), 0x100a78fe)
#define map_reset_view ORIG(void(__cdecl *)(void), 0x10050fc6)
#define IsoDeleteAllUnits ORIG(void(__stdcall *)(uint8_t *), 0x100ac51d)
#define pathfinder_init ORIG(int(__cdecl *)(int32_t, int32_t), 0x100c6d50)
#define ai_init ORIG(int(__cdecl *)(void), 0x100a6e82)

#define TEXT_BUF ((char *)(g_iso + 0xdea))

/* the view in cells, for map width index (0x20, 0x40, 0x80, 0x100 cells) and height
 * index (0x40-0x200), per screen width */
static const uint8_t view_w_640[4] = {0x13, 0x26, 0x4c, 0x98}, view_h_640[4] = {9, 0x12, 0x24, 0x48};
static const uint8_t view_w_800[4] = {0xf, 0x1e, 0x3c, 0x78}, view_h_800[4] = {7, 0xf, 0x1e, 0x3c};
static const uint8_t view_w_1024[4] = {0xc, 0x18, 0x30, 0x60}, view_h_1024[4] = {6, 0xc, 0x18, 0x30};
static const uint8_t view_w_1280[4] = {9, 0x12, 0x24, 0x48}, view_h_1280[4] = {4, 9, 0x12, 0x24};

/* 0x1001d92f: 0, or a negative error */
int __stdcall IsoCreateMap(uint8_t *iso, uint32_t w, uint32_t h, int scenario, int load_tiles, int from_file,
                           int autofill, uint32_t fill_first, int fill_count)
{
    int rc = 0, found_w = 0, found_h = 0;
    uint8_t wi = 0, hi = 0;
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10131188));
    switch (w) {
    case 0x20: wi = 0; break;
    case 0x40: wi = 1; break;
    case 0x80: wi = 2; break;
    case 0x100: wi = 3; break;
    }
    switch (h) {
    case 0x40: hi = 0; break;
    case 0x80: hi = 1; break;
    case 0x100: hi = 2; break;
    case 0x200: hi = 3; break;
    }
    switch (I(int32_t, 0xd70e)) { /* the screen width */
    case 640:
        I(uint8_t, 0xd70d) = view_w_640[wi];
        I(uint8_t, 0xd732) = view_h_640[hi];
        break;
    case 800:
        I(uint8_t, 0xd70d) = view_w_800[wi];
        I(uint8_t, 0xd732) = view_h_800[hi];
        break;
    case 1024:
        I(uint8_t, 0xd70d) = view_w_1024[wi];
        I(uint8_t, 0xd732) = view_h_1024[hi];
        break;
    case 1280:
        I(uint8_t, 0xd70d) = view_w_1280[wi];
        I(uint8_t, 0xd732) = view_h_1280[hi];
        break;
    }
    /* the row shifts for the allowed sizes */
    for (int i = 0; i < 4; i++) {
        if (G(uint32_t, 0x10130f30 + i * 4) == w) {
            found_w = 1;
            I(uint8_t, ISO_MAP_SHIFT) = G(uint8_t, 0x10130f50 + i * 4);
        }
        if (G(uint32_t, 0x10130f20 + i * 4) == h) {
            found_h = 1;
            I(uint8_t, 0x1aa1) = G(uint8_t, 0x10130f40 + i * 4);
        }
    }
    if (!found_w || !found_h)
        return -10;
    char *name = (char *)(g_iso + 0xc352 + scenario * 0x104);
    if (!name)
        return -20;
    I(int16_t, 0xc340) = (int16_t)scenario;
    I(uint32_t, ISO_MAP_W) = w;
    I(uint32_t, ISO_MAP_H) = h;
    map_reset_objects();
    I(int32_t, 0xcd7e) = 0x40; /* a cell in pixels */
    I(int32_t, 0xc34e) = 0x20;
    I(int32_t, 0xc32b) = log2_of(I(uint32_t, 0xcd7e));
    I(int32_t, 0xc32f) = log2_of(I(uint32_t, 0xc34e));
    I(int32_t, 0xc333) = log2_of(I(uint32_t, 0xcd7e) >> 1);
    I(int32_t, 0xc337) = log2_of(I(uint32_t, 0xc34e) >> 1);
    I(int32_t, 0xd6f1) = I(int32_t, ISO_MAP_W) << (I(uint8_t, 0xc32b) & 0x1f);
    I(int32_t, 0xd6f5) = I(int32_t, ISO_MAP_H) << (I(uint8_t, 0xc337) & 0x1f);
    I(int32_t, 0xd709) = I(int32_t, 0xd6f1) >> 1;
    I(int32_t, 0xd71e) = I(int32_t, 0xd6f5) >> 1;
    map_reset_view();
    IsoDeleteAllUnits(iso);
    if (I(void *, ISO_MAP))
        map_free();
    int32_t size = (int32_t)(h * w * 4);
    wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x10131198), size);
    OutputDebugStringA(TEXT_BUF);
    I(void *, ISO_MAP) = winmalloc((SIZE_T)size, 1);
    if (!I(void *, ISO_MAP))
        return -30;
    size = (int32_t)(h * w * 4);
    wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x101311c4), size);
    OutputDebugStringA(TEXT_BUF);
    I(void *, 0x1934) = winmalloc((SIZE_T)size, 1);
    if (!I(void *, 0x1934))
        return -30;
    if (load_tiles) {
        if (from_file) {
            rc = tilelib_load(name, 0, 0, 0, 1);
            DebugStr((const char *)ORIG_PTR(0x101311f8), rc);
        }
        if (!rc) {
            map_free();
            return -40;
        }
    }
    if (autofill)
        IsoAutofillMap(iso, (int)w, (int)h, scenario, fill_first, fill_count, 0);
    if (load_tiles && from_file) {
        rc = tilelib_background(iso, w, h, I(int32_t, 0xcda2));
        if (!rc) {
            tilelib_free();
            if (I(void *, 0x1974)) {
                winfree(I(void *, 0x1974));
                I(void *, 0x1974) = NULL;
            }
            memset(g_iso + 0x1994, 0, 0x104);
            map_free();
            return -0x10e;
        }
    }
    I(int32_t, 0x17fe) = I(int32_t, ISO_MAP_W) * 2;
    I(int32_t, 0x17fa) = I(int32_t, ISO_MAP_H) * 2;
    if (!pathfinder_init(I(int32_t, 0x17fe), I(int32_t, 0x17fa))) {
        OutputDebugStringA((const char *)ORIG_PTR(0x10131200));
        return -200;
    }
    OutputDebugStringA((const char *)ORIG_PTR(0x10131224));
    if (!ai_init()) {
        OutputDebugStringA((const char *)ORIG_PTR(0x1013124c));
        return -0xd2;
    }
    OutputDebugStringA((const char *)ORIG_PTR(0x10131270));
    I(int32_t, 0xc2ff) = 0;
    I(int16_t, 0x1b4b) = (int16_t)(10000 / I(int32_t, 0xd709));
    if (!I(int16_t, 0x1b4b))
        I(int16_t, 0x1b4b) = 1;
    I(int16_t, 0x1b11) = (int16_t)(10000 / (I(int32_t, 0xd70e) / 2));
    if (!I(int16_t, 0x1b11))
        I(int16_t, 0x1b11) = 1;
    return 0;
}
