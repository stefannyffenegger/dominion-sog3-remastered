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
