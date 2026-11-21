/* The game logic, 0x100c6d20-0x100c701f: the CD check stub and the path finder's buffers. */
#include "iso_core.h"
#include "pathinit.h"

#define MSG ((char *)(g_iso + 0xdea))

/* 0x100c6d20: the CD check, which always passes */
int32_t __cdecl cd_check(void)
{
    return 1;
}

static void say(uint32_t text)
{
    lstrcpyA(MSG, (const char *)ORIG_PTR(text));
    OutputDebugStringA(MSG);
}

static void said(uint32_t fmt, int32_t size)
{
    wsprintfA(MSG, (const char *)ORIG_PTR(fmt), size);
    OutputDebugStringA(MSG);
}

/* 0x100c6d50: allocates the path finder's buffers for a map of `w` x `h` cells: the grid
 * (2 bytes a cell, 0x1017e414), the open list (0x4e20 bytes, 0x1017e3f8), its lengths
 * (0xfa0, 0x1017e3fc) and the vectors (0x8ca0, 0x1017e400); each is reported on the debug
 * output. Only the grid and the vectors are freed first when they exist (the other two
 * leak when a map is loaded again, as in the original). 0 when one cannot be had */
int32_t __cdecl pathfinder_init(int32_t w, int32_t h)
{
    if (G(void *, 0x1017e414)) {
        winfree(G(void *, 0x1017e414));
        G(void *, 0x1017e414) = 0;
    }
    int32_t size = h * w * 2;
    G(void *, 0x1017e414) = winmalloc(size, 1);
    if (!G(void *, 0x1017e414)) {
        say(0x10158d98);
        return 0;
    }
    said(0x10158dac, size);
    size = 0x4e20;
    G(void *, 0x1017e3f8) = winmalloc(size, 1);
    if (!G(void *, 0x1017e3f8)) {
        say(0x10158de0);
        winfree(G(void *, 0x1017e414));
        return 0;
    }
    said(0x10158df4, size);
    size = 0xfa0;
    G(void *, 0x1017e3fc) = winmalloc(size, 1);
    if (!G(void *, 0x1017e3fc)) {
        say(0x10158e28);
        winfree(G(void *, 0x1017e3f8));
        winfree(G(void *, 0x1017e414));
        return 0;
    }
    said(0x10158e40, size);
    size = 0x8ca0;
    if (G(void *, 0x1017e400)) {
        winfree(G(void *, 0x1017e400));
        G(void *, 0x1017e400) = 0;
    }
    G(void *, 0x1017e400) = winmalloc(size, 1);
    if (!G(void *, 0x1017e400)) {
        say(0x10158e78);
        winfree(G(void *, 0x1017e3fc));
        winfree(G(void *, 0x1017e3f8));
        winfree(G(void *, 0x1017e414));
        return 0;
    }
    said(0x10158e90, size);
    G(int32_t, 0x1017e410) = I(int32_t, 0x1904);
    return 1;
}
