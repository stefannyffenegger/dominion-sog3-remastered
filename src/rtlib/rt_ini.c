/* Engine core, 0x1001aa0b-0x1001c176: settings in DOMINION.INI and the registry, the
 * display mode switch, and a few small commands. Rewritten from the original.
 *
 * Values can be stored scrambled ("crypted"): a string becomes two characters per byte
 * after adding the key name's first letter; a number written to the registry has its
 * bits shuffled through a table and 0xbebc205 added. */
#include "rtcore.h"
#include "crt.h"
#include "rt.h"
#include "rt_ini.h"
#include <mmsystem.h>

#define HSL R(uint8_t *, RT_HSL)
#define AUDIO R(uint8_t *, 0x0c90)
#define RT_ZOOM 0x3c43
#define RT_ZOOM_X 0x3c53
#define RT_ZOOM_Y 0x3c57
#define RT_CD_PLAYING 0x95d2
#define RT_ZOOMED 0x9601
#define RT_SECTION 0x9904      /* the game's INI section, or empty */
#define RT_APP_NAME 0x9d56     /* otherwise this */

#define AudioStopWave ORIG(void(__stdcall *)(uint8_t *, uint32_t, int32_t *), 0x100403b5)

static char g_value[0x401]; /* 0x1007cae0 */

/* 0x1001aa0b: the zoom command: +2 factor (0-4), +6 and +10 the zoomed area's corner */
void __stdcall cmd_zoom(const uint8_t *c)
{
    R(int32_t, RT_ZOOM) = P(int32_t, c, 2);
    if (R(int32_t, RT_ZOOM) < 0 || R(int32_t, RT_ZOOM) > 4)
        R(int32_t, RT_ZOOM) = 0;
    R(int32_t, RT_ZOOM_X) = P(int32_t, c, 6);
    R(int32_t, RT_ZOOM_Y) = P(int32_t, c, 10);
    R(uint8_t, RT_ZOOMED) = !(R(int32_t, RT_ZOOM) == 0 ||
                              (R(int32_t, RT_ZOOM) == 1 && !R(int32_t, RT_ZOOM_X) && !R(int32_t, RT_ZOOM_Y)));
    if (SpriteSetZoom(HSL, R(int32_t, RT_ZOOM), R(int32_t, RT_ZOOM_X), R(int32_t, RT_ZOOM_Y))) {
        if (R(uint8_t, RT_BKG_SCROLLS))
            paint_black(R(HDC, RT_DC));
        SpritePaint(HSL, NULL, 0);
    }
}

/* 0x1001ab63 */
void __cdecl cd_stop(void)
{
    if (R(uint8_t, RT_CD_PLAYING))
        R(uint8_t, RT_CD_PLAYING) = (uint8_t)AudioStopCD(AUDIO);
}

/* 0x1001aba5: stops wave resource id (0: all) */
void __stdcall cmd_stop_wave(uint32_t id, int32_t *params)
{
    if (!id)
        AudioStopWave(AUDIO, 0, params);
    else if (obj_loaded(id, 0xc))
        AudioStopWave(AUDIO, obj_entry(id)[0], params);
}

/* ---- INI files ---- */

/* 0x1001ac20, 0x1001ac46 */
void __stdcall ini_delete_key(LPCSTR file, LPCSTR section, LPCSTR key)
{
    WritePrivateProfileStringA(section, key, NULL, file);
}

void __stdcall ini_delete_section(LPCSTR file, LPCSTR section)
{
    WritePrivateProfileStringA(section, NULL, NULL, file);
}

static LPCSTR section_name(uint32_t id)
{
    if (id)
        return string_get(id);
    return R(char, RT_SECTION) ? (LPCSTR)(g_rt + RT_SECTION) : (LPCSTR)(g_rt + RT_APP_NAME);
}

/* 0x1001ac6a: deletes a section of DOMINION.INI (+2 its name, 0: the game's) */
void __stdcall cmd_ini_delete_section(const uint8_t *c)
{
    LPCSTR s = section_name(P(uint32_t, c, 2));
    if (s)
        WritePrivateProfileStringA(s, NULL, NULL, "DOMINION.INI");
}

/* 0x1001b19d: reads \n, \", \r, \t and \\ escapes */
void __stdcall ini_unescape(char *dst, const char *src)
{
    char ch;
    while ((ch = *src)) {
        if (ch == '\\') {
            ch = src[1];
            src += 2;
            if (!ch)
                break;
            if (ch >= 'A' && ch <= 'Z') /* tolower in the C locale */
                ch += 0x20;
            *dst = ch == 'n' ? '\n' : ch == '"' ? '"' : ch == 'r' ? '\r' : ch == 't' ? '\t' : ch == '\\' ? '\\' : ch;
        } else {
            *dst = ch;
            src++;
        }
        dst++;
    }
    *dst = 0;
}

/* 0x1001b7aa: writes those escapes. (A backslash comes out doubled.) */
void __stdcall ini_escape(char *dst, const char *src)
{
    for (char ch; (ch = *src++);) {
        switch (ch) {
        case '\t':
            *dst++ = '\\', *dst++ = 't';
            break;
        case '\n':
            *dst++ = '\\', *dst++ = 'n';
            break;
        case '\r':
            *dst++ = '\\', *dst++ = 'r';
            break;
        case '"':
            *dst++ = '\\', *dst++ = '"';
            break;
        case '\\':
            *dst++ = ch;
            /* fall through */
        default:
            *dst++ = ch;
        }
    }
    *dst = 0;
}

/* 0x1001b8f5: scrambles a string: each byte plus key, as two characters '0' + low and
 * '0' + high nibble (the high nibble shifted as a signed byte) */
void __stdcall ini_scramble(char *s, char key)
{
    char out[0x404], *o = out;
    for (const char *p = s; *p; p++) {
        uint8_t b = (uint8_t)(key + *p);
        *o++ = (char)((b & 0xf) + '0');
        *o++ = (char)(((int8_t)b >> 4) + '0');
    }
    *o = 0;
    lstrcpyA(s, out);
}

/* 0x1001b2af: undoes it (an odd-length string reads past its end, as in the original) */
void __stdcall ini_unscramble(char *s, char key)
{
    char out[0x404], *o = out;
    for (const char *p = s; *p; p += 2)
        *o++ = (char)(((uint8_t)(p[1] * 0x10) | (uint8_t)(p[0] - '0')) - (uint8_t)key);
    *o = 0;
    lstrcpyA(s, out);
}

/* 0x1001b0e9: reads a string (escapes undone, unscrambled with the key's first letter) */
void __stdcall ini_get_string(LPCSTR file, LPCSTR section, LPCSTR key, LPSTR out, int crypt)
{
    char raw[0x404];
    if (!file || !out || !key || !GetPrivateProfileStringA(section, key, "", raw, 0x401, file))
        return;
    ini_unescape(g_value, raw);
    if (crypt)
        ini_unscramble(g_value, *key);
    lstrcpynA(out, g_value, 0x200);
}

/* 0x1001b368 */
void __stdcall ini_get_int(LPCSTR file, LPCSTR section, LPCSTR key, int32_t *out, int crypt)
{
    if (!file || !section || !key || !out || !GetPrivateProfileStringA(section, key, "", g_value, 0x401, file))
        return;
    if (crypt)
        ini_unscramble(g_value, *key);
    *out = crt_atoi(g_value);
}

/* 0x1001b6c3: writes a string; an empty or blank one deletes the key */
void __stdcall ini_set_string(LPCSTR file, LPCSTR section, LPCSTR key, LPCSTR value, int crypt)
{
    char esc[0x404];
    if (!file || !section || !key || !value)
        return;
    ini_escape(esc, value);
    value = esc;
    if (crypt) {
        lstrcpynA(g_value, value, 0x200);
        ini_scramble(g_value, *key);
        value = g_value;
    }
    const char *p = value;
    while (*p == ' ')
        p++;
    WritePrivateProfileStringA(section, key, *p ? value : NULL, file);
}

/* 0x1001b9ad */
void __stdcall ini_set_int(LPCSTR file, LPCSTR section, LPCSTR key, int32_t value, int crypt)
{
    if (!file || !section || !key)
        return;
    wsprintfA(g_value, "%d", value);
    if (crypt)
        ini_scramble(g_value, *key);
    WritePrivateProfileStringA(section, key, g_value, file);
}

/* 0x1001acf5: read or write (command 0xe4) DOMINION.INI: +2 variable or string, +6 key,
 * +10 section (0: the game's), +0xe scrambled, +0xf a string */
void __stdcall cmd_ini(const uint8_t *c)
{
    LPCSTR section = section_name(P(uint32_t, c, 10));
    LPCSTR key = string_get(P(uint32_t, c, 6));
    if (!section || !key)
        return;
    uint8_t crypt = c[0xe];
    if (P(int16_t, c, 0) == 0xe4) {
        if (!c[0xf])
            ini_set_int("DOMINION.INI", section, key, *script_var(P(uint32_t, c, 2)), crypt);
        else {
            LPCSTR v = string_get(P(uint32_t, c, 2));
            if (v)
                ini_set_string("DOMINION.INI", section, key, v, crypt);
        }
    } else if (!c[0xf])
        ini_get_int("DOMINION.INI", section, key, script_var(P(uint32_t, c, 2)), crypt);
    else if (!(c[5] & 0x80))
        ini_get_string("DOMINION.INI", section, key, string_get(P(uint32_t, c, 2)), crypt);
}

/* 0x1001af6f: copies one file to another */
int __stdcall file_copy_mmio(HMMIO dst, HMMIO src)
{
    char *buf = winmalloc(0x400, 0);
    if (!buf)
        return 0;
    LONG left = mmioSeek(src, 0, SEEK_END);
    mmioSeek(src, 0, SEEK_SET);
    int ok = 1;
    while (left && ok) {
        LONG n = left > 0x3ff ? 0x400 : left;
        left -= n;
        ok = mmioRead(src, buf, n) == n && mmioWrite(dst, buf, n) == n;
    }
    winfree(buf);
    return ok;
}

/* 0x1001b058: the first free backup name 7thlevel.001 ... .999 in dir */
void __stdcall backup_name(char *out, char *dir)
{
    char name[256];
    for (uint32_t i = 1; i <= 0x3e7; i++) {
        wsprintfA(name, "7thlevel.%03i", i);
        MakeFullPath(out, dir, name);
        if (!mmioOpenA(out, NULL, MMIO_EXIST)) /* (an existing one stays open) */
            return;
    }
    MakeFullPath(out, dir, "7thlevel.999");
}

/* 0x1001ae99: backs DOMINION.INI up as 7thlevel.NNN in the Windows directory */
void __stdcall ini_backup(uint32_t unused)
{
    (void)unused; /* the command record */
    char dir[256], path[256], backup[256];
    GetWindowsDirectoryA(dir, sizeof dir);
    MakeFullPath(path, dir, "DOMINION.INI");
    HMMIO src = mmioOpenA(path, NULL, MMIO_READ);
    if (!src)
        return;
    backup_name(backup, dir);
    HMMIO dst = mmioOpenA(backup, NULL, MMIO_CREATE | MMIO_WRITE);
    if (dst) {
        file_copy_mmio(dst, src);
        mmioClose(dst, 0);
    }
    mmioClose(src, 0);
}

/* ---- the registry ---- */

static const uint8_t SCRAMBLE_BITS[8] = {0x80, 0x20, 0x10, 0x01, 0x40, 0x04, 0x02, 0x08};   /* 0x10075378 */
static const uint8_t UNSCRAMBLE_BITS[8] = {0x08, 0x40, 0x20, 0x80, 0x04, 0x02, 0x10, 0x01}; /* 0x10075380 */

static uint8_t bits_map(uint8_t b, const uint8_t *table)
{
    uint8_t r = 0;
    for (int i = 0; i < 8; i++)
        if (b & (1 << i))
            r |= table[i];
    return r;
}

/* 0x1001bd16, 0x1001b659 */
uint32_t __stdcall bits_scramble(uint8_t b)
{
    return bits_map(b, SCRAMBLE_BITS);
}

uint32_t __stdcall bits_unscramble(uint8_t b)
{
    return bits_map(b, UNSCRAMBLE_BITS);
}

/* 0x1001bc8b: a number scrambled for the registry */
uint32_t __stdcall reg_int_scramble(uint32_t v)
{
    v -= 0xbebc205;
    return bits_scramble((uint8_t)(v >> 16)) << 24 | bits_scramble((uint8_t)(v >> 24)) << 8 |
           bits_scramble((uint8_t)v) << 16 | bits_scramble((uint8_t)(v >> 8));
}

/* 0x1001b5cc */
uint32_t __stdcall reg_int_unscramble(uint32_t v)
{
    return (bits_unscramble((uint8_t)(v >> 8)) << 24 | bits_unscramble((uint8_t)v) << 8 |
            bits_unscramble((uint8_t)(v >> 24)) << 16 | bits_unscramble((uint8_t)(v >> 16))) +
           0xbebc205;
}

/* 0x1001b401: reads string value `name` of root\path\key */
void __stdcall reg_get_string(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, LPBYTE out, int crypt)
{
    HKEY a, b;
    DWORD size = 0x200;
    char buf[0x404];
    if (out)
        *out = 0;
    if (!path || !key || !name || !out || RegOpenKeyA(root, path, &a))
        return;
    if (!RegOpenKeyA(a, key, &b)) {
        RegQueryValueExA(b, name, NULL, NULL, out, &size);
        RegCloseKey(b);
        if (crypt) {
            lstrcpynA(buf, (LPCSTR)out, 0x200);
            ini_unscramble(buf, *key);
            lstrcpynA((LPSTR)out, buf, 0x200);
        }
    }
    RegCloseKey(a);
}

/* 0x1001b50c */
void __stdcall reg_get_int(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, int32_t *out, int crypt)
{
    HKEY a, b;
    DWORD size = 4;
    if (!path || !key || !name || RegOpenKeyA(root, path, &a))
        return;
    if (!RegOpenKeyA(a, key, &b)) {
        RegQueryValueExA(b, name, NULL, NULL, (LPBYTE)out, &size);
        if (crypt)
            *out = (int32_t)reg_int_unscramble((uint32_t)*out);
        RegCloseKey(b);
    }
    RegCloseKey(a);
}

/* 0x1001ba26: writes a string value; an empty name deletes the key, an empty value the
 * value */
void __stdcall reg_set_string(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, const BYTE *value, int crypt)
{
    HKEY a, b;
    DWORD disp;
    char buf[0x404];
    if (!path || !key || !name || !value || RegCreateKeyExA(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &a, &disp))
        return;
    if (!*name)
        RegDeleteKeyA(a, key);
    else if (!RegCreateKeyExA(a, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &b, &disp)) {
        if (!*value)
            RegDeleteValueA(b, name);
        else {
            if (crypt) {
                lstrcpynA(buf, (LPCSTR)value, 0x200);
                ini_scramble(buf, *key);
                value = (const BYTE *)buf;
            }
            RegSetValueExA(b, name, 0, REG_SZ, value, lstrlenA((LPCSTR)value));
        }
        RegCloseKey(b);
    }
    RegCloseKey(a);
}

/* 0x1001bb78: writes a number (as 4 binary bytes); del deletes the value */
void __stdcall reg_set_int(HKEY root, LPCSTR path, LPCSTR key, LPCSTR name, uint32_t value, int del, int crypt)
{
    HKEY a, b;
    DWORD disp;
    if (!path || !key || !name || RegCreateKeyExA(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &a, &disp))
        return;
    if (!*name)
        RegDeleteKeyA(a, key);
    else if (!RegCreateKeyExA(a, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &b, &disp)) {
        if (!del) {
            if (crypt)
                value = reg_int_scramble(value);
            RegSetValueExA(b, name, 0, REG_BINARY, (BYTE *)&value, 4);
        } else
            RegDeleteValueA(b, name);
        RegCloseKey(b);
    }
    RegCloseKey(a);
}

/* ---- display mode ---- */

/* 0x1001bd80, 0x1001bda7: ChangeDisplaySettings in a thread of its own (CDS_FULLSCREEN);
 * returns its result, -1 when the thread can't start */
DWORD __stdcall display_change_thread(void *mode)
{
    return (DWORD)ChangeDisplaySettingsA(mode, CDS_FULLSCREEN);
}

DWORD __cdecl display_change(void *mode)
{
    DWORD code;
    HANDLE t = CreateThread(NULL, 0, display_change_thread, mode, 0, &code);
    if (!t)
        return 0xffffffff;
    WaitForSingleObject(t, INFINITE);
    GetExitCodeThread(t, &code);
    CloseHandle(t);
    return code;
}

/* 0x1001c06a: Windows 4 (95/NT 4) or later */
int __cdecl windows_has_display_modes(void)
{
    return (uint8_t)GetVersion() > 3;
}

/* 0x1001c0c9 */
int __cdecl screen_is(int w, int h)
{
    return GetSystemMetrics(SM_CXSCREEN) == w && GetSystemMetrics(SM_CYSCREEN) == h;
}

/* 0x1001be13: switches the desktop to w x h at bpp bits (0: the current depth; all 0:
 * back to the registry's mode). The exact depth is tried first, then any deeper one.
 * Returns 0 on success, -1 on failure (0 too when Windows is too old). */
uint32_t __stdcall display_mode_set(DWORD w, DWORD h, uint32_t bpp)
{
    if (!windows_has_display_modes())
        return 0;
    if (!w && !h && !bpp)
        return display_change(NULL) ? 0xffffffff : 0;
    if (!bpp)
        bpp = GetDeviceCaps(R(HDC, RT_DC), BITSPIXEL);
    for (int exact = 1;; exact = 0) {
        DEVMODEA dm;
        int done = 0;
        for (DWORD i = 0; EnumDisplaySettingsA(NULL, i, &dm); i++) {
            int skip = exact ? dm.dmBitsPerPel != bpp : dm.dmBitsPerPel < bpp;
            if (skip || dm.dmPelsWidth != w || dm.dmPelsHeight != h)
                continue;
            DEVMODEA set;
            memset(&set, 0, 0x9c);
            set.dmSize = 0x9c;
            set.dmBitsPerPel = dm.dmBitsPerPel;
            set.dmPelsWidth = dm.dmPelsWidth;
            set.dmPelsHeight = dm.dmPelsHeight;
            set.dmFields = 0x3c0000;
            if (!display_change(&set)) {
                done = 1;
                break;
            }
        }
        if (done)
            return screen_is(w, h) ? 0 : 0xffffffff;
        if (!exact)
            return 0xffffffff;
    }
}

/* RtCaptureFrame (0x1001c110): starts saving every frame, to files named after the
 * string resource */
void __cdecl RtCaptureFrame(uint8_t *rt, char *prefix, uint32_t n)
{
    char name[256];
    g_rt = rt;
    capture_name(name, prefix, 0x100, n);
    if (lstrlenA(name))
        SpriteInitScreenCapture(R(uint8_t *, RT_HSL), name);
}
