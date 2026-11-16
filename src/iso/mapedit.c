/* The game logic, 0x1001c7c0-0x1001d92f: the map's tiles as the engine's tile
 * background, the tile library (a .TL2 file: an 18-byte "TLX" header and the tiles),
 * and the map editor's tile library compiler and autofill. */
#include <stdarg.h>
#include "iso_core.h"
#include "mapedit.h"

#define g_srand ORIG(void(__cdecl *)(uint32_t), 0x100f4d60)
#define g_rand ORIG(int(__cdecl *)(void), 0x100f4d80)
#define g_time ORIG(int32_t(__cdecl *)(int32_t *), 0x100f4df0)

#define ISO_TILE_BG 0x1960    /* the engine's tile background (0x7c bytes + cell tiles) */
#define ISO_TILE_LIST 0x196c  /* per tile: its data in the library */
#define ISO_TILE_DATA 0x1970  /* the library's tile data */
#define ISO_TILE_RES 0x1978   /* the tile list resource (compiling) */
#define ISO_OBJ_MAP 0x1934
#define ISO_TEXT 0xdea        /* a text buffer for debug messages */
#define TEXT_BUF ((char *)(g_iso + ISO_TEXT))

static void debug_text(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(TEXT_BUF, fmt, ap);
    va_end(ap);
    OutputDebugStringA(TEXT_BUF);
}

/* 0x1001d0a6: frees the tile background and the tile library */
int __cdecl tiles_free(void)
{
    uint8_t *bg = I(uint8_t *, ISO_TILE_BG);
    if (bg) {
        RtSetTileBackground(I(uint8_t *, ISO_APP), 0, 0);
        if (P(void *, bg, 0x10))
            winfree(P(void *, bg, 0x10));
        P(void *, bg, 0x10) = NULL;
        winfree(bg);
    }
    I(void *, ISO_TILE_BG) = NULL;
    if (I(void *, ISO_TILE_LIST))
        winfree(I(void *, ISO_TILE_LIST));
    I(void *, ISO_TILE_LIST) = NULL;
    if (I(void *, ISO_TILE_DATA))
        winfree(I(void *, ISO_TILE_DATA));
    I(void *, ISO_TILE_DATA) = NULL;
    return 1;
}

/* 0x1001c7c0: makes the engine's tile background for a w x h map from the map's
 * cells and the tile library */
int __stdcall tiles_background(uint8_t *iso, uint32_t w, uint32_t h, int unused)
{
    (void)unused;
    g_iso = iso;
    uint32_t cells = (h & 0xffff) * (w & 0xffff);
    int32_t size = (int32_t)(cells * 4 + 0x7c);
    debug_text((const char *)ORIG_PTR(0x10130f60), size);
    uint8_t *bg = winmalloc((SIZE_T)size, 1);
    I(uint8_t *, ISO_TILE_BG) = bg;
    if (!bg)
        return 0;
    P(int32_t, bg, 0x3c) = 0;
    P(int16_t, bg, 0x2c) = 1;
    P(uint32_t, bg, 0x24) = w & 0xffff;
    P(uint32_t, bg, 0x28) = h & 0xffff;
    P(int32_t, bg, 0x1c) = I(int32_t, 0xcd7e);
    P(int32_t, bg, 0x20) = I(int32_t, 0xc34e);
    P(uint8_t, bg, 0x2e) = 0;
    P(uint8_t, bg, 0x2f) = 0;
    P(uint8_t *, bg, 0) = bg + 0x7c;
    debug_text((const char *)ORIG_PTR(0x10130f88), cells);
    bg = I(uint8_t *, ISO_TILE_BG);
    if (P(void *, bg, 0x10))
        winfree(P(void *, bg, 0x10));
    P(void *, bg, 0x10) = NULL;
    P(void *, bg, 0x10) = winmalloc(cells, 1);
    if (!P(void *, bg, 0x10) || !I(void *, ISO_TILE_LIST)) {
        winfree(bg);
        I(void *, ISO_TILE_BG) = NULL;
        return 0;
    }
    memset(P(void *, bg, 0x10), 1, cells);
    P(int32_t, bg, 0xc) = I(int32_t, 0x18f4);
    if (!I(void *, ISO_MAP)) {
        if (P(void *, bg, 0x10))
            winfree(P(void *, bg, 0x10));
        P(void *, bg, 0x10) = NULL;
        if (I(void *, ISO_TILE_BG))
            winfree(I(void *, ISO_TILE_BG));
        I(void *, ISO_TILE_BG) = NULL;
        return 0;
    }
    for (uint32_t i = 0; i < cells; i++)
        P(uint32_t *, bg, 0)[i] = I(uint32_t *, ISO_TILE_LIST)[I(uint32_t *, ISO_MAP)[i] & 0x7fff];
    RtSetTileBackground(I(uint8_t *, ISO_APP), 0, 0);
    RtSetTileBackground(I(uint8_t *, ISO_APP), (uint32_t)(uintptr_t)I(uint8_t *, ISO_TILE_BG), 0);
    return 1;
}

/* 0x1001cb55: frees the map's cells and object map */
int __cdecl map_free(void)
{
    if (I(void *, ISO_MAP))
        winfree(I(void *, ISO_MAP));
    if (I(void *, ISO_OBJ_MAP))
        winfree(I(void *, ISO_OBJ_MAP));
    I(void *, ISO_MAP) = NULL;
    I(void *, ISO_OBJ_MAP) = NULL;
    return 1;
}

/* 0x1001cbce: loads tile library `name`: all of it (`whole`), or the tiles whose size
 * in `sizes` is positive (negative: skipped, `total` bytes in all) */
int __stdcall tiles_load(char *name, int32_t *sizes, int32_t total, int whole)
{
    struct __attribute__((packed)) {
        char magic[6];
        int32_t count, bytes, header;
    } h;
    DebugStr((const char *)ORIG_PTR(0x10130fb4));
    if (!sizes && !whole)
        return 0;
    tiles_free();
    HMMIO f = mmioOpenA(name, NULL, 0);
    if (!f)
        return 0;
    LONG got = mmioRead(f, (HPSTR)&h, 0x12);
    if (got != 0x12 || h.magic[0] != 'T' || h.magic[1] != 'L' || h.magic[2] != 'X') {
        mmioClose(f, 0);
        return 0;
    }
    I(int32_t, 0xcda2) = h.count;
    int32_t list = h.count << 2;
    debug_text((const char *)ORIG_PTR(0x10130fcc), list, h.count);
    I(void *, ISO_TILE_LIST) = winmalloc((SIZE_T)list, 1);
    if (!I(void *, ISO_TILE_LIST)) {
        tiles_free();
        OutputDebugStringA((const char *)ORIG_PTR(0x10131004));
        mmioClose(f, 0);
        return 0;
    }
    LONG end = mmioSeek(f, 0, SEEK_END);
    int32_t data = end - 0x12;
    if (!whole) {
        debug_text((const char *)ORIG_PTR(0x101310b0), total, name);
        I(void *, ISO_TILE_DATA) = winmalloc((SIZE_T)(total + 1), 1);
        if (!I(void *, ISO_TILE_DATA)) {
            tiles_free();
            mmioClose(f, 0);
            return 0;
        }
    } else {
        mmioSeek(f, 0x12, SEEK_SET);
        debug_text((const char *)ORIG_PTR(0x10131034), data, name);
        I(void *, ISO_TILE_DATA) = winmalloc((SIZE_T)(data + 1), 1);
        got = 0;
        if (I(void *, ISO_TILE_DATA))
            got = mmioRead(f, I(HPSTR, ISO_TILE_DATA), data);
        debug_text((const char *)ORIG_PTR(0x10131068), got);
        if (got < data || !I(void *, ISO_TILE_DATA)) {
            OutputDebugStringA((const char *)ORIG_PTR(0x10131094));
            if (I(void *, ISO_TILE_DATA) || I(void *, ISO_TILE_LIST))
                tiles_free();
            mmioClose(f, 0);
            return 0;
        }
    }
    int32_t at = 0, pos = 0x12;
    for (int i = 0; i < h.count; i++) {
        int wanted = sizes ? sizes[i] > 0 : 0;
        uint8_t **tiles = I(uint8_t **, ISO_TILE_LIST);
        if (wanted || whole == 1) {
            if (!whole) {
                mmioSeek(f, pos, SEEK_SET);
                got = mmioRead(f, I(HPSTR, ISO_TILE_DATA) + at, sizes[i] < 0 ? -sizes[i] : sizes[i]);
            }
            uint8_t *t = I(uint8_t *, ISO_TILE_DATA) + at;
            tiles[i] = t;
            P(uint8_t *, t, 0x20) = (uint8_t *)(uintptr_t)(P(int32_t, t, 0) + (int32_t)(uintptr_t)t);
            at += P(int32_t, t, 0x18);
        } else {
            tiles[i] = NULL;
        }
        if (!whole) {
            pos += sizes[i] < 0 ? -sizes[i] : sizes[i];
            if (pos > end) {
                tiles_free();
                OutputDebugStringA((const char *)ORIG_PTR(0x101310e8));
                mmioClose(f, 0);
                return 0;
            }
        }
    }
    mmioClose(f, 0);
    return 1;
}

/* 0x1001d1a4 */
int __cdecl IsoUnloadMaskSubSystem(uint8_t *iso)
{
    g_iso = iso;
    if (I(void *, 0x18f4))
        winfree(I(void *, 0x18f4));
    I(void *, 0x18f4) = NULL;
    if (I(void *, 0x1920))
        winfree(I(void *, 0x1920));
    I(void *, 0x1920) = NULL;
    return 1;
}

/* 0x1001d239: the map editor writes the tiles of tile list resource `list` into a
 * tile library file (name: string `name_id`); 0 or a negative error */
int __stdcall IsoCompileTileLibrary(uint8_t *iso, uint32_t list, uint16_t tag, uint32_t name_id)
{
    struct __attribute__((packed)) {
        char magic[4];
        uint16_t tag;
        int32_t count, bytes, header;
    } h;
    g_iso = iso;
    LPSTR name = (LPSTR)RtGetString(I(void *, ISO_APP), name_id);
    int loaded = 0;
    int32_t **res = (int32_t **)RtGetResource(I(void *, ISO_APP), list);
    if (!*res || !(*res)[2]) {
        loaded = 1;
        RtLoad(I(void *, ISO_APP), list);
        I(int32_t *, ISO_TILE_RES) = *res;
    }
    int32_t *t = I(int32_t *, ISO_TILE_RES);
    if (!t)
        return -0xdc;
    HMMIO f = mmioOpenA(name, NULL, MMIO_CREATE | MMIO_WRITE | MMIO_ALLOCBUF);
    if (!f)
        return -0xe6;
    int32_t n = t[0x28 / 4];
    lstrcpyA(h.magic, (const char *)ORIG_PTR(0x10131104));
    h.tag = tag;
    h.count = 0;
    h.bytes = 0;
    h.header = 0x12;
    if (mmioWrite(f, (char *)&h, 0x12) != 0x12) {
        mmioClose(f, 0);
        return -0xf0;
    }
    uint32_t size = 0;
    for (int i = 0; i < n; i++) {
        t = I(int32_t *, ISO_TILE_RES);
        int32_t *row = (int32_t *)(uintptr_t)(t[0] + t[0xb] * i * 4);
        if (row[2] == -1)
            continue;
        uint8_t *tile = ResLoadResource(RtGetHResfile(I(void *, ISO_APP)), (uint32_t)row[0], &size);
        int ok = 0;
        if (tile && size) {
            P(uint32_t, tile, 0x18) = size;
            P(int32_t, tile, 0x1c) = row[1];
            ok = mmioWrite(f, (char *)tile, (LONG)size) == (LONG)size;
            h.bytes += (int32_t)size;
            h.count++;
        }
        if (!ok || !tile) {
            debug_text((const char *)ORIG_PTR(0x10131108), h.count, h.bytes);
            mmioClose(f, 0);
            return -0xfa;
        }
    }
    debug_text((const char *)ORIG_PTR(0x10131140), h.count, h.bytes);
    mmioSeek(f, 0, SEEK_SET);
    if (mmioWrite(f, (char *)&h, 0x12) != 0x12) {
        mmioClose(f, 0);
        return -0xf0;
    }
    mmioClose(f, 0);
    if (loaded == 1)
        RtUnload(I(void *, ISO_APP), list);
    return 0;
}

/* 0x1001d60a: the map editor fills the w x h map with random tiles first..first+n-1
 * (only over cells already of that range with `only_range`), no tile equal to its
 * upper neighbours or left neighbour; the edge (and impassable tiles) are blocked.
 * Seeded from the clock. */
void __stdcall IsoAutofillMap(uint8_t *iso, int w, int h, int unused, uint32_t first, int n, int only_range)
{
    (void)unused;
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10131174));
    uint32_t t = 0;
    uint32_t last = (uint32_t)(n + (int)first - 1);
    g_srand((uint32_t)g_time(NULL));
    uint32_t *map = NULL;
    for (int row = 0; row < h; row++) {
        int c = row << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f);
        for (int col = 0; col < w; col++, c++) {
            map = I(uint32_t *, ISO_MAP);
            if (only_range && ((map[c] & 0x7fff) < first || (map[c] & 0x7fff) > last))
                continue;
            for (int k = 0; k < n * 8; k++) {
                t = (uint32_t)(g_rand() % n) + first;
                map = I(uint32_t *, ISO_MAP);
                /* neighbours with the map width as the row stride */
                if (col >= 2 && col < w - 1 && row >= 2 &&
                    ((map[c - (w + 1)] & 0x7fff) == t || (map[c - w] & 0x7fff) == t ||
                     (map[c - (w - 1)] & 0x7fff) == t || (map[c - 1] & 0x7fff) == t))
                    continue;
                map[c] = (map[c] & 0xffff8000) | (t & 0x7fff);
                break;
            }
            uint32_t *o = &I(uint32_t *, ISO_OBJ_MAP)[c];
            if (col == 0 || col == w - 1 || row < 2 || row >= h - 2) {
                *o |= 1;
                *o |= 2;
            } else if (*I(int32_t **, 0xd755)[t]) {
                *o |= 1;
            } else {
                *o &= ~1u;
            }
        }
    }
}

/* 0x1001d225 IsoClearScreen only returns 0: too short to hook, left original */
