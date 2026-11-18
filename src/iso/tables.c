/* The game logic, 0x1004e360-0x1004ea01: the map's neighbour offsets, the external
 * tables (text files of comma-separated numbers loaded into an engine resource), cel
 * offsets, the short unit type names and the file-name comparator. */
#include "iso_core.h"
#include "tables.h"

/* the original's C library */
#define orig_access ORIG(int(__cdecl *)(const char *, int), 0x100f7250)
#define orig_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define orig_strlen ORIG(uint32_t(__cdecl *)(const char *), 0x100f5750)
#define orig_strchr ORIG(char *(__cdecl *)(const char *, int), 0x100f6b70)
#define orig_strtok ORIG(char *(__cdecl *)(char *, const char *), 0x100f5fc0)
#define orig_isalpha ORIG(int(__cdecl *)(int), 0x100f64f0)
#define orig_atoi ORIG(int(__cdecl *)(const char *), 0x100f6260)
#define orig_strcmpi ORIG(int(__cdecl *)(const char *, const char *), 0x100f5680)

#define TEXT_BUF ((char *)(g_iso + 0xdea))

/* 0x1004e360: the cell offsets of the eight neighbours for the map's row length, in
 * three tables (0x1017c4f8: the row length first, 0x1017c538 and 0x1017c53c: two
 * interleaved tables of eight) */
void __cdecl map_neighbour_offsets(void)
{
    int32_t w = I(int32_t, 0x1a9c);
    G(int32_t, 0x1017c4f8) = w;
    G(int32_t, 0x1017c500) = w - 1;
    G(int32_t, 0x1017c508) = -(w + 1);
    G(int32_t, 0x1017c510) = -w;
    G(int32_t, 0x1017c4fc) = w + 1;
    G(int32_t, 0x1017c504) = w;
    G(int32_t, 0x1017c50c) = -w;
    G(int32_t, 0x1017c514) = -(w - 1);
    G(int32_t, 0x1017c538) = 1;
    G(int32_t, 0x1017c540) = w;
    G(int32_t, 0x1017c548) = w * 2;
    G(int32_t, 0x1017c550) = w - 1;
    G(int32_t, 0x1017c558) = -1;
    G(int32_t, 0x1017c560) = -(w + 1);
    G(int32_t, 0x1017c568) = w * -2;
    G(int32_t, 0x1017c570) = -w;
    G(int32_t, 0x1017c53c) = 1;
    G(int32_t, 0x1017c544) = w + 1;
    G(int32_t, 0x1017c54c) = w * 2;
    G(int32_t, 0x1017c554) = w;
    G(int32_t, 0x1017c55c) = -1;
    G(int32_t, 0x1017c564) = -w;
    G(int32_t, 0x1017c56c) = w * -2;
    G(int32_t, 0x1017c574) = -(w - 1);
}

/* 0x1004e474: whether the file named by the string resource exists */
int32_t __cdecl IsoFileExists(uint8_t *iso, uint32_t name)
{
    g_iso = iso;
    return orig_access(RtGetString(I(void *, 0x180e), name), 0) == 0;
}

/* 0x1004e4c8: the short name of a unit type (race, type: from the original's table of
 * 15-character names) into a string resource; its length */
uint32_t __cdecl IsoGetShortUnitTypeString(uint8_t *iso, int32_t race, int32_t type, uint32_t str)
{
    g_iso = iso;
    char *s = (char *)RtGetString(I(void *, 0x180e), str);
    if (!s)
        return 0;
    orig_strcpy(s, (const char *)ORIG_PTR(0x1013b548 + type * 0xf + race * 0x267));
    return orig_strlen(s);
}

/* 0x1004e549: an external table: the text file named by string resource `file` loaded
 * into the numbers of resource `res` (layers x rows x columns of ints). Spaces are
 * dropped, ';' or '/' starts a comment, empty lines are skipped, values starting with a
 * letter are left as they are. 0 on any error. */
int32_t __stdcall IsoLoadExternalTable(uint8_t *iso, uint32_t res, uint32_t file)
{
    g_iso = iso;
    if (!RtLoad(I(void *, 0x180e), res)) {
        wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x1013d748), res);
        OutputDebugStringA(TEXT_BUF);
        return 0;
    }
    int32_t *t = *(int32_t **)RtGetResource(I(void *, 0x180e), res);
    if (!t)
        return 0;
    int32_t layers, rows, cols;
    if (t[4] == 2) {
        layers = 1;
        rows = t[10];
        cols = t[0xb];
    } else {
        layers = t[10];
        rows = t[0xb];
        cols = t[0xc];
    }
    HMMIO f = mmioOpenA((LPSTR)RtGetString(I(void *, 0x180e), file), NULL, 0);
    if (!f)
        return 0;
    char line[0x400], clean[0x400];
    LONG pos = 0;
    for (int32_t row = 0; row < layers * rows;) {
        char *p = line, *q = clean;
        mmioSeek(f, pos, SEEK_SET);
        if (mmioRead(f, line, 0x400) < 1) {
            mmioClose(f, 0);
            return 0;
        }
        char *cr = orig_strchr(line, '\r');
        if (!cr) {
            mmioClose(f, 0);
            return 0;
        }
        *cr = 0;
        pos += (LONG)(cr - line) + 2;
        while (*p) {
            if (*p == ' ') {
                p++;
            } else {
                if (*p == ';' || *p == '/')
                    break;
                *q++ = *p++;
            }
        }
        *q = 0;
        if (!clean[0])
            continue;
        int32_t *v = (int32_t *)(uintptr_t)(cols * row * 4 + *t);
        char *tok = orig_strtok(clean, (const char *)ORIG_PTR(0x1013d770));
        for (int32_t c = 0; c < cols; c++) {
            if (!tok) {
                mmioClose(f, 0);
                return 0;
            }
            if (!orig_isalpha(*tok))
                *v = orig_atoi(tok);
            v++;
            tok = orig_strtok(NULL, (const char *)ORIG_PTR(0x1013d774));
        }
        row++;
    }
    mmioClose(f, 0);
    return 1;
}

/* 0x1004e93d: moves cel `from`'s offset by (-dx2, -dy2) and cel `to`'s by (dx, dy) in
 * the sprite resource */
void __stdcall IsoSetCelOffset(uint8_t *iso, uint32_t res, int32_t to, int32_t dx, int32_t dy, int32_t from,
                               int32_t dx2, int32_t dy2)
{
    g_iso = iso;
    uint8_t *s = RtGetResource(I(void *, 0x180e), res)[1];
    if (!s)
        return;
    uint8_t **cels = P(uint8_t **, s, 0x24);
    P(int32_t, cels[from], 0x14) -= dx2;
    P(int32_t, cels[from], 0x18) -= dy2;
    P(int32_t, cels[to], 0x14) += dx;
    P(int32_t, cels[to], 0x18) += dy;
}

/* 0x1004e9d2: the file list's sort order */
int __cdecl file_name_compare(const char *a, const char *b)
{
    return orig_strcmpi(a, b);
}
