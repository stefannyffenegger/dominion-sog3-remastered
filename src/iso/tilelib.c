/* The game logic, 0x10023185-0x10023ee6: the mask library (IsoLoadMaskLibrary, a "TLX"
 * file of fog masks) and the scenario's tile library (a "TL2" file of 0x444-byte tiles,
 * all or the ones the map uses) with the tile background the engine draws. */
#include <stdarg.h>
#include "iso_core.h"
#include "mapedit.h"
#include "tilelib.h"

#define g_memmove ORIG(void *(__cdecl *)(void *, const void *, size_t), 0x100f5bd0)

#define ISO_MASKS 0x18f4     /* per mask: its data */
#define ISO_MASK_DATA 0x1920
#define ISO_TILES 0xd755     /* per tile: its data (0x444 bytes), or 0 */
#define ISO_TILE_DATA 0xd749
#define TEXT_BUF ((char *)(g_iso + 0xdea))

static void debug_text(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(TEXT_BUF, fmt, ap);
    va_end(ap);
    OutputDebugStringA(TEXT_BUF);
}

/* 0x10023185: loads the mask library named by string `name_id`; masks whose size is
 * odd are followed by a byte of padding, so that each starts at an even offset */
int __cdecl IsoLoadMaskLibrary(uint8_t *iso, uint32_t name_id)
{
    struct __attribute__((packed)) { char magic[6]; int32_t count; char rest[8]; } h;
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10131694));
    char *name = (char *)RtGetString(I(void *, ISO_APP), name_id);
    if (I(void *, ISO_MASKS) || I(void *, ISO_MASK_DATA))
        IsoUnloadMaskSubSystem(iso);
    HMMIO f = mmioOpenA(name, NULL, 0);
    if (!f) {
        DebugStr((const char *)ORIG_PTR(0x101316ac));
        return 0;
    }
    LONG got = mmioRead(f, (HPSTR)&h, 0x12);
    if (got != 0x12 || h.magic[0] != 'T' || h.magic[1] != 'L' || h.magic[2] != 'X') {
        DebugStr((const char *)ORIG_PTR(0x101316c4));
        mmioClose(f, 0);
        return 0;
    }
    int32_t list = h.count << 2;
    debug_text((const char *)ORIG_PTR(0x101316dc), list, h.count);
    I(void *, ISO_MASKS) = winmalloc((SIZE_T)list, 1);
    if (!I(void *, ISO_MASKS)) {
        DebugStr((const char *)ORIG_PTR(0x10131714));
        IsoUnloadMaskSubSystem(iso);
        OutputDebugStringA((const char *)ORIG_PTR(0x1013172c));
        return 0; /* the original leaves the file open here */
    }
    int32_t size = mmioSeek(f, 0, SEEK_END) - 0x12;
    mmioSeek(f, 0x12, SEEK_SET);
    debug_text((const char *)ORIG_PTR(0x10131758), size, name);
    I(void *, ISO_MASK_DATA) = winmalloc((SIZE_T)(h.count + size + 1), 1);
    got = 0;
    if (I(void *, ISO_MASK_DATA))
        got = mmioRead(f, I(HPSTR, ISO_MASK_DATA), size);
    debug_text((const char *)ORIG_PTR(0x1013178c), got);
    mmioClose(f, 0);
    if (got < size || !I(void *, ISO_MASK_DATA)) {
        DebugStr((const char *)ORIG_PTR(0x101317b8));
        OutputDebugStringA((const char *)ORIG_PTR(0x101317d0));
        IsoUnloadMaskSubSystem(iso);
        return 0;
    }
    uint32_t at = 0;
    for (int i = 0; i < h.count; i++) {
        uint8_t *data = I(uint8_t *, ISO_MASK_DATA);
        uint8_t *m = data + at;
        I(uint8_t **, ISO_MASKS)[i] = m;
        P(uint8_t *, m, 0x20) = (uint8_t *)(uintptr_t)(P(int32_t, m, 0) + (int32_t)(uintptr_t)data + (int32_t)at);
        at += P(uint32_t, m, 0x18);
        if (at & 1) {
            g_memmove(data + at + 1, data + at, (size_t)(size - (int32_t)at));
            at++;
            size++;
        }
    }
    *I(uint32_t *, ISO_MASKS) = 0; /* mask 0 is none */
    return 1;
}

/* 0x10023559: frees the tile background and the tile library */
int __cdecl tilelib_free(void)
{
    uint8_t *bg = I(uint8_t *, 0x1960);
    if (bg) {
        RtSetTileBackground(I(uint8_t *, ISO_APP), 0, 0);
        if (P(void *, bg, 0x10))
            winfree(P(void *, bg, 0x10));
        P(void *, bg, 0x10) = NULL;
        winfree(bg);
    }
    I(void *, 0x1960) = NULL;
    if (I(void *, ISO_TILES))
        winfree(I(void *, ISO_TILES));
    I(void *, ISO_TILES) = NULL;
    if (I(void *, ISO_TILE_DATA))
        winfree(I(void *, ISO_TILE_DATA));
    I(void *, ISO_TILE_DATA) = NULL;
    return 1;
}

/* 0x10023657: loads tile library `name`: all of it (`whole`), or the tiles with a
 * positive count in `used` (`n` of them; the high half of `n` is not used) */
int __stdcall tilelib_load(char *name, int32_t *used, int32_t unused, uint32_t n, int whole)
{
    struct __attribute__((packed)) { char magic[6]; int32_t count; char rest[8]; } h;
    (void)unused;
    if (!used && !whole)
        return 0;
    tilelib_free();
    HMMIO f = mmioOpenA(name, NULL, 0);
    if (!f)
        return 0;
    LONG got = mmioRead(f, (HPSTR)&h, 0x12);
    if (got != 0x12 || h.magic[0] != 'T' || h.magic[1] != 'L' || h.magic[2] != '2') {
        mmioClose(f, 0);
        return 0;
    }
    I(int32_t, 0xcda2) = h.count;
    memcpy(g_iso + 0xd737, &h, 0x12);
    I(void *, ISO_TILES) = winmalloc((SIZE_T)(h.count << 2), 1);
    if (!I(void *, ISO_TILES)) {
        tilelib_free();
        OutputDebugStringA((const char *)ORIG_PTR(0x101317ec));
        mmioClose(f, 0);
        return 0;
    }
    LONG end = mmioSeek(f, 0, SEEK_END);
    int32_t data = end - 0x12;
    I(int32_t, 0xd74d) = data;
    if (!whole) {
        int32_t bytes = (int32_t)((n & 0xffff) * 0x444);
        debug_text((const char *)ORIG_PTR(0x10131894), bytes, name);
        I(void *, ISO_TILE_DATA) = winmalloc((SIZE_T)(bytes + 4), 1);
        if (!I(void *, ISO_TILE_DATA)) {
            tilelib_free();
            mmioClose(f, 0);
            return 0;
        }
    } else {
        mmioSeek(f, 0x12, SEEK_SET);
        debug_text((const char *)ORIG_PTR(0x10131818), data, name);
        I(void *, ISO_TILE_DATA) = winmalloc((SIZE_T)(data + 4), 1);
        got = 0;
        if (I(void *, ISO_TILE_DATA))
            got = mmioRead(f, I(HPSTR, ISO_TILE_DATA), data);
        debug_text((const char *)ORIG_PTR(0x1013184c), got);
        if (got < data || !I(void *, ISO_TILE_DATA)) {
            OutputDebugStringA((const char *)ORIG_PTR(0x10131878));
            if (I(void *, ISO_TILE_DATA) || I(void *, ISO_TILES))
                tilelib_free();
            mmioClose(f, 0);
            return 0;
        }
    }
    int32_t at = 0, pos = 0x12;
    for (int i = 0; i < h.count; i++) {
        int wanted = used ? used[i] > 0 : 0;
        if (wanted || whole == 1) {
            if (!whole) {
                mmioSeek(f, pos, SEEK_SET);
                mmioRead(f, I(HPSTR, ISO_TILE_DATA) + at, 0x444);
            }
            I(uint8_t **, ISO_TILES)[i] = I(uint8_t *, ISO_TILE_DATA) + at;
            at += 0x444;
        } else {
            I(uint8_t **, ISO_TILES)[i] = NULL;
        }
        if (!whole) {
            pos += 0x444;
            if (pos > end) {
                tilelib_free();
                OutputDebugStringA((const char *)ORIG_PTR(0x101318cc));
                mmioClose(f, 0);
                return 0;
            }
        }
    }
    mmioClose(f, 0);
    DebugStr((const char *)ORIG_PTR(0x101318e8), I(int32_t, 0xcda2));
    I(int32_t, 0xd751) = 1;
    return 1;
}

/* 0x10023b04: makes the engine's tile background (tile numbers) for a w x h map */
int __stdcall tilelib_background(uint8_t *iso, uint32_t w, uint32_t h, int32_t unused)
{
    (void)unused;
    g_iso = iso;
    uint32_t cells = (h & 0xffff) * (w & 0xffff);
    uint8_t *bg = winmalloc(cells * 2 + 0x7c, 1);
    I(uint8_t *, 0x1960) = bg;
    if (!bg)
        return 0;
    P(int32_t, bg, 0x3c) = 0;
    P(int16_t, bg, 0x2c) = 2;
    P(uint32_t, bg, 0x24) = w & 0xffff;
    P(uint32_t, bg, 0x28) = h & 0xffff;
    P(int32_t, bg, 0x1c) = I(int32_t, 0xcd7e);
    P(int32_t, bg, 0x20) = I(int32_t, 0xc34e);
    P(uint8_t, bg, 0x2e) = 0;
    P(uint8_t, bg, 0x2f) = 0;
    P(void *, bg, 4) = I(void *, ISO_TILES);
    P(uint8_t *, bg, 8) = bg + 0x7c;
    P(uint32_t, bg, 0) = 0;
    debug_text((const char *)ORIG_PTR(0x101318fc), cells);
    if (P(void *, bg, 0x10))
        winfree(P(void *, bg, 0x10));
    P(void *, bg, 0x10) = NULL;
    P(void *, bg, 0x10) = winmalloc(cells, 1);
    if (!P(void *, bg, 0x10) || !I(void *, ISO_TILES)) {
        winfree(bg);
        I(void *, 0x1960) = NULL;
        return 0;
    }
    memset(P(void *, bg, 0x10), 1, cells);
    P(void *, bg, 0xc) = I(void *, ISO_MASKS);
    if (!I(void *, ISO_MAP)) {
        if (P(void *, bg, 0x10))
            winfree(P(void *, bg, 0x10));
        P(void *, bg, 0x10) = NULL;
        if (I(void *, 0x1960))
            winfree(I(void *, 0x1960));
        I(void *, 0x1960) = NULL;
        return 0;
    }
    for (uint32_t i = 0; i < cells; i++)
        P(uint16_t *, bg, 8)[i] = (uint16_t)(I(uint32_t *, ISO_MAP)[i] & 0x7fff);
    P(void *, bg, 0x14) = I(void *, ISO_MAP);
    P(void *, bg, 0x18) = I(void *, 0x1934);
    P(uint32_t, bg, 0x30) = 0;
    P(uint32_t, bg, 0x34) = 0;
    P(uint32_t, bg, 0x38) = 0;
    RtSetTileBackground(I(uint8_t *, ISO_APP), 0, 0);
    RtSetTileBackground(I(uint8_t *, ISO_APP), (uint32_t)(uintptr_t)bg, 0);
    return 1;
}
