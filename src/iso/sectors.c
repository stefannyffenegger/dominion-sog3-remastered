/* The game logic, 0x100a6df0-0x100a73ce: the frame flip timer, and the sector tables. The
 * map is divided into sectors of 16 rows by 4 columns of cells (iso+0xc2eb: 0x1e bytes each;
 * +4 -1, +6 the eight neighbours' numbers or -1 off the map, +0x1d set when the sector's
 * corner cell is blocked), and into blocks of 2 by 0.5 for a second byte table (iso+0x17a7). */
#include "iso_core.h"
#include "sound.h"
#include "debugtime.h"
#include "sectors.h"

#define log2_of ORIG(int32_t(__stdcall *)(uint32_t), 0x100a78fe)
#define IsoSnapTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, uint32_t *, int *, int *), 0x100aeebb)
#define debug_log ORIG(void(__cdecl *)(int, const char *), 0x10095f64)
#define DX(k) G(int8_t, 0x1015b568 + (k))
#define DY(k) G(int8_t, 0x1015b570 + (k))

/* IsoInitFlipProcess */
int32_t __stdcall IsoInitFlipProcessC(uint8_t *iso)
{
    g_iso = iso;
    I(uint32_t, 0xd83f) = 5;
    I(uint32_t, 0xd843) = timeGetTime();
    return 1;
}

/* 0x100a6e82: makes both sector tables for the map. 0 if out of memory */
int32_t __cdecl sectors_init(void)
{
    I(uint32_t, 0x1852) = 0;
    I(uint32_t, 0x114) = 1;
    int32_t rows = I(int32_t, ISO_MAP_H) * 0x10 / 0x100;
    int32_t cols = I(int32_t, ISO_MAP_W) * 0x40 / 0x100;
    I(int16_t, 0xc2fd) = (int16_t)cols;
    I(int16_t, 0xc2ef) = (int16_t)rows;
    I(int32_t, 0xc2f5) = log2_of(I(uint16_t, 0xc2fd));
    I(int32_t, 0xc2f1) = rows * cols;
    I(int32_t, 0xc2f9) = rows * cols * 0x1e;
    I(uint8_t *, 0xc2eb) = winmalloc((SIZE_T)I(int32_t, 0xc2f9), 1);
    if (!I(uint8_t *, 0xc2eb))
        return 0;
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x101573a8), I(uint32_t, 0xc2f9));
    debug_log(7, (const char *)(g_iso + 0xdea));
    for (int16_t r = 0; r < rows; r++)
        for (int16_t c = 0; c < cols; c++) {
            int32_t s = (r << (I(uint8_t, 0xc2f5) & 0x1f)) + c;
            int x, cell;
            uint32_t y;
            IsoGetSectorCoordinates(g_iso, s, &x, (int *)&y);
            cell = IsoSnapTilePos(g_iso, &x, &y, &x, (int *)&y);
            uint32_t m = I(uint32_t *, 0x1934)[cell];
            uint8_t *rec = I(uint8_t *, 0xc2eb) + s * 0x1e;
            if ((m & 1) || (m >> 1 & 1))
                rec[0x1d] = 1;
            P(int16_t, rec, 4) = -1;
            for (int k = 0; k < 8; k++) {
                int32_t nx = DX(k) + c, ny = DY(k) + r;
                int32_t n = (ny << (I(uint8_t, 0xc2f5) & 0x1f)) + nx;
                P(int16_t, I(uint8_t *, 0xc2eb), s * 0x1e + 6 + k * 2) = -1;
                int off;
                switch (k) {
                case 0: off = cols - 1 == c; break;
                case 1: off = cols - 1 == c || r == rows - 1; break;
                case 2: off = r == rows - 1; break;
                case 3: off = c == 0 || r == rows - 1; break;
                case 4: off = c == 0; break;
                case 5: off = c == 0 || r == 0; break;
                case 6: off = r == 0; break;
                default: off = cols - 1 == c || r == 0; break;
                }
                if (!off)
                    P(int16_t, I(uint8_t *, 0xc2eb), s * 0x1e + 6 + k * 2) = (int16_t)n;
            }
        }
    rows = I(int32_t, ISO_MAP_H) * 0x10 / 0x20;
    cols = I(int32_t, ISO_MAP_W) * 0x40 / 0x20;
    I(int16_t, 0x17b9) = (int16_t)cols;
    I(int16_t, 0x17ab) = (int16_t)rows;
    I(int32_t, 0x17b1) = log2_of(I(uint16_t, 0x17b9));
    I(int32_t, 0x17ad) = rows * cols;
    I(int32_t, 0x17b5) = rows * cols;
    I(uint8_t *, 0x17a7) = winmalloc((SIZE_T)I(int32_t, 0x17b5), 1);
    if (!I(uint8_t *, 0x17a7))
        return 0;
    wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x101573d8), I(uint32_t, 0x17b5));
    debug_log(7, (const char *)(g_iso + 0xdea));
    return 1;
}
