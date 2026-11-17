/* The game logic, 0x1002c74e-0x1002cbfa: what the scripts ask of the scenario
 * description (briefing lines, the briefing's wave, the cinematics, the next
 * scenario), and linking two energy wall towers. */
#include "iso_core.h"
#include "power.h"
#include "display.h"
#include "sdl.h"
#include "sdlexec.h"
#include "sdcget.h"

#define link_possible ORIG(int(__cdecl *)(uint32_t, uint32_t), 0x10077c54)
#define link_add ORIG(int(__cdecl *)(uint32_t, uint32_t, int, int), 0x10078dd5)
#define net_link ORIG(void(__cdecl *)(uint32_t, uint32_t, int), 0x1003527c)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_strcat ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c80)

static char *var(uint32_t v) { return RtGetVariableString(I(void *, ISO_APP), v); }

/* 0x1002c74e: briefing line n (0-29) into variable `v` */
int __cdecl IsoGetBriefTextLine(uint8_t *iso, uint32_t n, uint32_t v)
{
    g_iso = iso;
    if (n >= 0x1e)
        return 0;
    char *s = var(v);
    if (!s || !I(char *, 0x1860 + n * 4))
        return 0;
    g_strcpy(s, I(char *, 0x1860 + n * 4));
    return 1;
}

/* 0x1002c7eb: the briefing's wave into variable `v` */
int __cdecl IsoGetBriefWav(uint8_t *iso, uint32_t v)
{
    g_iso = iso;
    char *s = var(v);
    if (!s || !I(char *, 0x18d8))
        return 0;
    g_strcpy(s, I(char *, 0x18d8));
    return 1;
}

/* 0x1002c86d: cinematic n (0-2) appended to variable `v` */
int __cdecl IsoGetMissionCine(uint8_t *iso, int n, uint32_t v)
{
    g_iso = iso;
    if (!I(void *, 0xc2e5))
        return 0;
    char *s = var(v);
    if (!s || !I(char *, 0x1908 + n * 4))
        return 0;
    g_strcat(s, I(char *, 0x1908 + n * 4));
    return 1;
}

/* 0x1002c95c: the next scenario's name into variable `v` */
int __cdecl IsoGetNextSDLFilename(uint8_t *iso, uint32_t v)
{
    g_iso = iso;
    char *s = var(v);
    if (!s || !I(char *, 0x1928))
        return 0;
    g_strcpy(s, I(char *, 0x1928));
    return 1;
}

/* 0x1002ca08: links the two energy wall towers (types 0x40-0x4f) standing at (x1, y1)
 * and (x2, y2) */
void __cdecl energywall_link(int x1, int y1, int x2, int y2)
{
    if (!I(int32_t, ISO_HAVE_SPRITES))
        return;
    uint32_t first = 0;
    for (int i = 0; i < I(int32_t, ISO_UNIT_COUNT); i++) {
        uint32_t v = I(uint32_t *, ISO_UNIT_LIST)[i];
        uint8_t *u;
        if (!v || !(u = I(uint8_t **, ISO_UNITS)[v]))
            continue;
        int32_t t = P(int32_t, u, U_ID_TYPE);
        if (t < 0x40 || t >= 0x50)
            continue;
        if (!((P(int16_t, u, U_X) == x1 && P(int16_t, u, U_Y) == y1) || (P(int16_t, u, U_X) == x2 && P(int16_t, u, U_Y) == y2)))
            continue;
        if (!first) {
            first = v;
            continue;
        }
        if (unit_direction(first, v) == -1) {
            DebugStr((const char *)ORIG_PTR(0x1013619c));
            return;
        }
        if (!link_possible(first, v)) {
            DebugStr((const char *)ORIG_PTR(0x10136168));
            return;
        }
        power_check((int16_t)first);
        power_check((int16_t)v);
        if (!link_add(first, v, 0, 1)) {
            DebugStr((const char *)ORIG_PTR(0x10136144), first, v);
            return;
        }
        if (I(int, 0x175))
            net_link(first, v, 1);
        return;
    }
}

#define g_strlen ORIG(size_t(__cdecl *)(const char *), 0x100f5750)
#define sdl_start ORIG(void(__stdcall *)(uint8_t *), 0x10048270)
#define g_brief_lines G(uint8_t, 0x101347c8) /* briefing lines still to collect */

/* 0x1002cf1c: loads compiled scenario description `file` (in folder `dir`): a 0x18-byte
 * header (the record count at 0x14) and 0x5a-byte records; with `run` it carries out the
 * timers, hot spots and settings records, collects the briefing lines and starts the
 * mission. A multiplayer client without the host's file reads the text form instead. */
int __cdecl sdc_load(char *file, char *dir, int run)
{
    char path[260];
    struct __attribute__((packed)) { char magic[0x14]; int32_t count; } h;
    uint8_t brief = 0;
    g_brief_lines = 0;
    if (I(int, 0x175) && I(int32_t, 0xd9da)) {
        file[g_strlen(file) - 1] = 'L';
        return sdl_load(file, dir, run);
    }
    g_strcpy((char *)(g_iso + 0xbfd9), file);
    g_strcpy((char *)(g_iso + 0xc1e1), dir);
    wsprintfA(path, (const char *)ORIG_PTR(0x101362c8), dir, file);
    HMMIO f = mmioOpenA(path, NULL, 0);
    if (!f)
        return 0;
    if (mmioRead(f, (HPSTR)&h, 0x18) != 0x18) {
        mmioClose(f, 0);
        return 0;
    }
    I(int16_t, 0xc2e9) = (int16_t)h.count;
    if (I(void *, 0xc2e5)) {
        winfree(I(void *, 0xc2e5));
        I(void *, 0xc2e5) = NULL;
    }
    I(void *, 0xc2e5) = winmalloc((SIZE_T)(I(uint16_t, 0xc2e9) * 0x5a), 1);
    if (!I(void *, 0xc2e5)) {
        mmioClose(f, 0);
        return 0;
    }
    if (mmioRead(f, I(HPSTR, 0xc2e5), h.count * 0x5a) != h.count * 0x5a) {
        winfree(I(void *, 0xc2e5));
        I(void *, 0xc2e5) = NULL;
        mmioClose(f, 0);
        return 0;
    }
    for (int k = 0; k < 0x1e; k++)
        I(int32_t, 0x1860 + k * 4) = 0;
    for (int k = 0; k < 3; k++)
        I(int32_t, 0x1908 + k * 4) = 0;
    I(int32_t, 0x1918) = 0;
    I(int32_t, 0x1914) = 0;
    I(int32_t, 0x18d8) = 0;
    I(int32_t, 0x1928) = 0;
    for (uint32_t i = 0; (int)i < (int)I(uint16_t, 0xc2e9); i++) {
        uint8_t *r = I(uint8_t *, 0xc2e5) + i * 0x5a;
        int32_t type = P(int32_t, r, 0), sub = P(int32_t, r, 4);
        if (!run)
            continue;
        if (((type == 10 && !sub) || (type == 0x14 && !sub) || type == 0x28 || type == 0x32 || type == 0x3c ||
             type == 0x46 || type == 0x50) &&
            sub != 5) {
            if (!sdl_exec(i)) {
                winfree(I(void *, 0xc2e5));
                I(void *, 0xc2e5) = NULL;
                mmioClose(f, 0);
                return 0;
            }
        } else if (g_brief_lines) { /* the lines after the briefing record */
            g_brief_lines--;
            DebugStr((const char *)ORIG_PTR(0x10136348), g_brief_lines, i, r + 10);
            if (g_strlen((const char *)r + 10)) {
                I(uint8_t *, 0x1860 + brief * 4) = r + 10;
                brief++;
            }
        }
    }
    mmioClose(f, 0);
    if (run && I(int32_t, ISO_HAVE_SPRITES) && !G(int32_t, 0x10139e68)) {
        if (I(int, 0x175) && !I(int32_t, 0x138))
            return 1;
        sdl_start(g_iso);
    }
    return 1;
}

/* 0x1002d4de: loads the compiled description named by string `file_id` in the folder of
 * string `dir_id` */
int __cdecl IsoLoadSDCFile(uint8_t *iso, uint32_t file_id, uint32_t dir_id, int run)
{
    g_iso = iso;
    char *file = (char *)RtGetString(I(void *, ISO_APP), file_id);
    if (!file)
        return 0;
    char *dir = (char *)RtGetString(I(void *, ISO_APP), dir_id);
    if (!dir)
        return 0;
    DebugStr((const char *)ORIG_PTR(0x10136390), file);
    return sdc_load(file, dir, run);
}

/* 0x1002e6f7: looks through the description for records of type `type` until the one of
 * subtype `want`: SET_PLAYER_RACE (0x28/0x96: player 0's race is returned), the next
 * scenario (0x28/0xbe) and the cinematics (0x46, the first such record). -1 when no
 * race was found. */
int32_t __cdecl IsoProcessSDCRecord(uint8_t *iso, int32_t type, int32_t want)
{
    int32_t race = -1;
    int done = 0;
    g_iso = iso;
    for (int i = 0; i < (int)I(uint16_t, 0xc2e9) && !done; i++) {
        uint8_t *r = I(uint8_t *, 0xc2e5) + i * 0x5a;
        int32_t sub = P(int32_t, r, 4);
        if (P(int32_t, r, 0) != type)
            continue;
        if (type == 0x28) {
            if (sub == 0x96) {
                if (!P(int32_t, r, 10)) {
                    race = P(int32_t, r, 0xe);
                    if (want == 0x96)
                        done = 1;
                }
            } else if (sub == 0xbe) {
                I(uint8_t *, 0x1928) = r + 10;
                if (want == 0xbe)
                    done = 1;
            }
        } else if (type == 0x46) {
            I(uint8_t *, 0x1908) = r + 10;
            I(uint8_t *, 0x190c) = r + 0x1e;
            I(uint8_t *, 0x1910) = r + 0x32;
            done = 1;
        }
    }
    return race;
}
