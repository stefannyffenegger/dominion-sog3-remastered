/* The game logic, 0x100221d9-0x100229d0: the list of map files for the menus
 * (IsoBuildFileList, IsoBuildFileListIni): 0x104-byte names at 0x18f0, sorted, their
 * count at 0x179f. In a multiplayer game other than the host the list is kept. */
#include "iso_core.h"
#include "filelist.h"

#define g_strlen ORIG(size_t(__cdecl *)(const char *), 0x100f5750)
#define g_strcmpi ORIG(int(__cdecl *)(const char *, const char *), 0x100f5680)
#define g_qsort ORIG(void(__cdecl *)(void *, size_t, size_t, void *), 0x100f57d0)
#define name_compare ORIG_PTR(0x1004e9d2)

#define ISO_FILES 0x18f0
#define ISO_FILE_COUNT 0x179f
#define NAME(i) ((char *)I(uint8_t *, ISO_FILES) + (i) * 0x104)

/* 0x100221d9: whether `name` is one of the ini file's entries "<key>1".."<key>n" of
 * section `section` */
int __cdecl file_listed(const char *name, const char *ini, const char *key, const char *section, uint32_t n)
{
    char *value = winmalloc(0x104, 1), *entry;
    if (!value || !(entry = winmalloc(0x104, 1)))
        return 0;
    for (uint32_t i = 0; i < n; i++) {
        wsprintfA(entry, (const char *)ORIG_PTR(0x101314e4), key, i + 1);
        GetPrivateProfileStringA(section, entry, (const char *)ORIG_PTR(0x101314ec), value, 0x104, ini);
        if (g_strlen(value) && !g_strcmpi(name, value)) {
            winfree(entry);
            winfree(value);
            return 1;
        }
    }
    winfree(entry);
    winfree(value);
    return 0;
}

static int keep_list(void)
{
    return !I(int32_t, 0x148) && I(int, 0x175);
}

static int list_failed(HANDLE find)
{
    winfree(I(void *, ISO_FILES));
    I(void *, ISO_FILES) = NULL;
    I(int32_t, ISO_FILE_COUNT) = 0;
    FindClose(find);
    return 0;
}

/* 0x100222e6: the files matching pattern string `pattern_id` that the ini file
 * (string `ini_id`) lists in section `section_id` as "<key>1".."<key>n" */
uint32_t __stdcall IsoBuildFileListIni(uint8_t *iso, uint32_t pattern_id, uint32_t key_id, uint32_t ini_id,
                                       uint32_t section_id, uint32_t n)
{
    WIN32_FIND_DATAA fd;
    g_iso = iso;
    if (keep_list())
        return *I(char *, ISO_FILES) != 0;
    const char *pattern = RtGetString(I(void *, ISO_APP), pattern_id);
    const char *key = RtGetString(I(void *, ISO_APP), key_id);
    const char *ini = RtGetString(I(void *, ISO_APP), ini_id);
    const char *section = RtGetString(I(void *, ISO_APP), section_id);
    HANDLE find = FindFirstFileA(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        FindClose(INVALID_HANDLE_VALUE);
        return 0;
    }
    uint32_t count = file_listed(fd.cFileName, ini, key, section, n) != 0;
    int files = 1;
    while (FindNextFileA(find, &fd) == 1) {
        if (file_listed(fd.cFileName, ini, key, section, n))
            count++;
        files++;
    }
    if (I(void *, ISO_FILES)) {
        winfree(I(void *, ISO_FILES));
        I(void *, ISO_FILES) = NULL;
        I(int32_t, ISO_FILE_COUNT) = 0;
    }
    I(void *, ISO_FILES) = winmalloc(count * 0x104, 1);
    find = FindFirstFileA(pattern, &fd);
    int listed = file_listed(fd.cFileName, ini, key, section, n);
    if (listed)
        lstrcpyA(NAME(0), fd.cFileName);
    uint32_t at = listed != 0;
    for (int i = 1; i < files; i++) {
        if (!FindNextFileA(find, &fd))
            return list_failed(find);
        if (file_listed(fd.cFileName, ini, key, section, n)) {
            lstrcpyA(NAME(at), fd.cFileName);
            at++;
        }
    }
    I(uint32_t, ISO_FILE_COUNT) = count;
    FindClose(find);
    g_qsort(I(void *, ISO_FILES), I(uint32_t, ISO_FILE_COUNT), 0x104, name_compare);
    return count;
}

/* 0x100226f0: all files matching pattern string `pattern_id` */
int __stdcall IsoBuildFileList(uint8_t *iso, uint32_t pattern_id)
{
    WIN32_FIND_DATAA fd;
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x101314f0));
    if (keep_list())
        return *I(char *, ISO_FILES) != 0;
    const char *pattern = RtGetString(I(void *, ISO_APP), pattern_id);
    HANDLE find = FindFirstFileA(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        FindClose(INVALID_HANDLE_VALUE);
        return 0;
    }
    int count = 1;
    while (FindNextFileA(find, &fd) == 1)
        count++;
    if (I(void *, ISO_FILES)) {
        winfree(I(void *, ISO_FILES));
        I(void *, ISO_FILES) = NULL;
    }
    I(void *, ISO_FILES) = winmalloc((SIZE_T)(count * 0x104), 1);
    find = FindFirstFileA(pattern, &fd);
    lstrcpyA(NAME(0), fd.cFileName);
    for (int i = 1; i < count; i++) {
        if (!FindNextFileA(find, &fd))
            return list_failed(find);
        lstrcpyA(NAME(i), fd.cFileName);
    }
    I(int32_t, ISO_FILE_COUNT) = count;
    FindClose(find);
    g_qsort(I(void *, ISO_FILES), I(uint32_t, ISO_FILE_COUNT), 0x104, name_compare);
    return count;
}

#define path_fix ORIG(void(__cdecl *)(char *), 0x1005bfc2)
#define g_strcmp ORIG(int(__cdecl *)(const char *, const char *), 0x100f53a0)
#define g_memcmp ORIG(int(__cdecl *)(const void *, const void *, size_t), 0x100f5b20)
#define message_box ORIG(void(__cdecl *)(int, const char *, const char *, int), 0x1005c515)

/* the info a map header gives: tile set, players, height, width */
static void header_info(const uint8_t *h, uint32_t *info)
{
    info[0] = h[0x2e];
    info[1] = h[0x2f];
    info[2] = P(uint32_t, h, 0x38) & 0xffff;
    info[3] = P(uint32_t, h, 0x38) >> 16;
}

/* 0x100229d0: the index in the file list of the current scenario's name (0xbfd9), or
 * -1 */
int __stdcall IsoGetSDLFilenameIndex(uint8_t *iso, uint32_t var)
{
    char name[0x104];
    g_iso = iso;
    RtGetVariableString(I(void *, ISO_APP), var);
    for (int i = 0; i < I(int32_t, ISO_FILE_COUNT); i++) {
        lstrcpyA(name, NAME(i));
        if (!g_strcmpi((const char *)(g_iso + 0xbfd9), name))
            return i;
    }
    return -1;
}

/* 0x10022afd: the header of map `index` of the file list (in folder variable
 * `dir_var`): its name (without extension) into variable `name_var`, its title into
 * `title_var`, tile set, players and size into info[4], whether it is a .wxm into
 * *is_wxm. A multiplayer client checks the host's map (the first entry) against the
 * header it has. Returns 0 only when the client lacks the file. */
int __stdcall IsoGetWXPInfo(uint8_t *iso, int index, uint32_t dir_var, uint32_t name_var, uint32_t title_var,
                            uint32_t *info, int *is_wxm)
{
    char path[0x104];
    uint8_t h[0x80];
    int rc;
    g_iso = iso;
    char *name = RtGetVariableString(I(void *, ISO_APP), name_var);
    char *title = RtGetVariableString(I(void *, ISO_APP), title_var);
    char *dir = RtGetVariableString(I(void *, ISO_APP), dir_var);
    DebugStr((const char *)ORIG_PTR(0x101315d0), index);
    DebugStr((const char *)ORIG_PTR(0x101315e8), dir);
    if (!I(void *, ISO_FILES))
        return 1;
    lstrcpyA(path, dir);
    if (!I(int32_t, 0x148) && I(int, 0x175) && *I(char *, ISO_FILES)) {
        lstrcpyA(name, NAME(0));
        lstrcatA(path, name);
        name[g_strlen(name) - 4] = 0;
        path_fix(path);
        HMMIO f = mmioOpenA(path, NULL, 0);
        if (!f) {
            rc = 0;
            memcpy(h, g_iso + 0xbf40, 0x80);
            DebugStr((const char *)ORIG_PTR(0x101315f4), path);
        } else {
            LONG got = mmioRead(f, (HPSTR)h, 0x80);
            mmioClose(f, 0);
            if (got != 0x80) {
                wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10131608), path);
                message_box(0, (const char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x1013161c), 0);
                return 1;
            }
            if (g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131628)) &&
                g_strcmp((const char *)h, (const char *)ORIG_PTR(0x1013162c))) {
                wsprintfA((char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x10131630), path, h);
                message_box(0, (const char *)(g_iso + 0xdea), (const char *)ORIG_PTR(0x1013164c), 0);
                return 1;
            }
            if (g_memcmp(h, g_iso + 0xbf40, 0x80))
                return 1;
            rc = 1;
        }
    } else {
        if (index < 0 || index > I(int32_t, ISO_FILE_COUNT))
            return 1;
        lstrcpyA(name, NAME(index));
        lstrcatA(path, name);
        name[g_strlen(name) - 4] = 0;
        path_fix(path);
        memset(h, 0, 0x80);
        HMMIO f = mmioOpenA(path, NULL, 0);
        if (!f) {
            DebugStr((const char *)ORIG_PTR(0x10131658), path);
            return 1;
        }
        LONG got = mmioRead(f, (HPSTR)h, 0x80);
        if (got != 0x80 || (g_strcmp((const char *)h, (const char *)ORIG_PTR(0x1013166c)) &&
                            g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131670)))) {
            mmioClose(f, 0);
            return 1;
        }
        mmioClose(f, 0);
        rc = 1;
    }
    lstrcpyA(title, (const char *)h + 6);
    header_info(h, info);
    *is_wxm = !g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131674));
    return rc;
}

/* 0x10023014: a map file's tile set, players and size into info[4] */
int __cdecl map_file_info(char *path, uint32_t *info)
{
    uint8_t h[0x80];
    HMMIO f = mmioOpenA(path, NULL, 0);
    if (!f)
        return 0;
    LONG got = mmioRead(f, (HPSTR)h, 0x80);
    if (got == 0x80 && (!g_strcmp((const char *)h, (const char *)ORIG_PTR(0x10131678)) ||
                        !g_strcmp((const char *)h, (const char *)ORIG_PTR(0x1013167c)))) {
        mmioClose(f, 0);
        header_info(h, info);
        return 1;
    }
    mmioClose(f, 0);
    return 0;
}

/* 0x10023118 */
int __stdcall IsoFreeFileList(uint8_t *iso)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10131680));
    if (I(void *, ISO_FILES)) {
        winfree(I(void *, ISO_FILES));
        I(void *, ISO_FILES) = NULL;
    }
    I(int32_t, ISO_FILE_COUNT) = 0;
    return 1;
}
