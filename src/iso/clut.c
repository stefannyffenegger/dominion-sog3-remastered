/* The game logic, 0x100945da-0x10094a25: the colour lookup tables for the engine's blitter:
 * clutNN.dat (64 KB, table 2) and blut.dat (1280 bytes, widened to ints, table 1). */
#include "iso_core.h"
#include "clut.h"

/* IsoLoadClut (C++ name ?IsoLoadClut@@YAHPAXI@Z): loads clut<n>.dat. 0 if it cannot */
int32_t __cdecl IsoLoadClutC(uint8_t *iso, uint32_t n)
{
    char name[0x40];
    g_iso = iso;
    wsprintfA(name, (const char *)ORIG_PTR(0x10152a70), n);
    if (I(void *, 0x18e0))
        winfree(I(void *, 0x18e0));
    I(void *, 0x18e0) = winmalloc(0x10000, 1);
    if (!I(void *, 0x18e0))
        return 0;
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10152a7c), 0x10000, name);
    OutputDebugStringA((const char *)(g_iso + 0xdea));
    HMMIO h = mmioOpenA(name, NULL, 0);
    if (!h) {
        winfree(I(void *, 0x18e0));
        I(void *, 0x18e0) = 0;
        return 0;
    }
    if (mmioRead(h, I(char *, 0x18e0), 0x10000) != 0x10000) {
        winfree(I(void *, 0x18e0));
        I(void *, 0x18e0) = 0;
        mmioClose(h, 0);
        return 0;
    }
    mmioClose(h, 0);
    RtSetBlitLookupTable(I(void *, ISO_APP), 2, I(void *, 0x18e0));
    return 1;
}

/* IsoLoadBlut (?IsoLoadBlut@@YAHPAX@Z): loads blut.dat. 0 if it cannot (when only one of
 * the two allocations fails, the other is not freed, as in the original) */
int32_t __cdecl IsoLoadBlutC(uint8_t *iso)
{
    char name[0x40];
    g_iso = iso;
    wsprintfA(name, (const char *)ORIG_PTR(0x10152aa8));
    if (I(void *, 0x185c))
        winfree(I(void *, 0x185c));
    I(uint32_t *, 0x185c) = winmalloc(0x1400, 1);
    uint8_t *raw = winmalloc(0x500, 1);
    if (!I(void *, 0x185c) || !raw)
        return 0;
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10152ab4), 0x1400, name);
    OutputDebugStringA((const char *)(g_iso + 0xdea));
    HMMIO h = mmioOpenA(name, NULL, 0);
    if (!h) {
        winfree(I(void *, 0x185c));
        winfree(raw);
        I(void *, 0x185c) = 0;
        return 0;
    }
    if (mmioRead(h, (char *)raw, 0x500) != 0x500) {
        winfree(I(void *, 0x185c));
        winfree(raw);
        I(void *, 0x185c) = 0;
        mmioClose(h, 0);
        return 0;
    }
    for (int k = 0; k < 0x500; k++)
        I(uint32_t *, 0x185c)[k] = raw[k];
    winfree(raw);
    mmioClose(h, 0);
    RtSetBlitLookupTable(I(void *, ISO_APP), 1, I(void *, 0x185c));
    return 1;
}
