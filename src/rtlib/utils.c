/* The engine's utility library: memory, strings, paths, debug output, integer trigonometry
 * (angles in tenths of a degree, 0-3599; results scaled by 10000).
 * Rewritten from the original (0x1003d800-0x1003e9b7).
 *
 * The floating-point math (GetAngle, Collide, Distance, SquareRoot, GetIntersect) is in
 * math87.c, ExternalCall in rt_script.c. */
#include "orig.h"
#include "utils.h"
#include <stdarg.h>
#include <mmsystem.h>

/* globals of the original */
#define g_mem_used G(uint32_t, 0x10087420)       /* bytes allocated with winmalloc */
#define g_open_pointers G(uint32_t, 0x10087424)
#define g_bsearch_lo G(int32_t, 0x100776e8)
#define g_bsearch_hi G(int32_t, 0x100776ec)
#define g_sine ((const uint16_t *)ORIG_PTR(0x100776f0))    /* 0..90 degrees, 901 entries */
#define g_tangent ((const int32_t *)ORIG_PTR(0x10077e00))  /* 0..180 degrees */
#define g_debug_wnd G(HWND, 0x100883d8)
typedef void(__stdcall *DebugHook)(char *, uint32_t);
#define g_debug_hook G(DebugHook, 0x100883e0)
#define g_debug_hook_arg G(uint32_t, 0x100883e4)
#define g_pentium G(uint32_t, 0x100883e8)
#define g_processor G(uint8_t, 0x100883ec)

static char *find_last(char *s, char c)
{
    char *r = NULL;
    for (; *s; s++)
        if (*s == c)
            r = s;
    return r;
}

static char *find_first(char *s, char c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return NULL;
}

/* ---- memory and strings ---- */

void __stdcall CopyHugeBytes(void *dst, const void *src, uint32_t n)
{
    memcpy(dst, src, n);
}

void __stdcall CopyHugeStr(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}

int __stdcall HugeStrLen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

void __stdcall SetHugeBytes(void *dst, uint8_t value, uint32_t n)
{
    memset(dst, value, n);
}

/* Replaces each byte with table[byte] */
void __stdcall MapBytes(uint8_t *p, int n, const uint8_t *table)
{
    for (; n; n--, p++)
        *p = table[*p];
}

/* Finds key in a sorted table of strings (base + offsets[i]). Returns the index, or the
 * complement of the insertion point. */
uint32_t __stdcall BinarySearch(const char *key, const char *base, const int32_t *offsets, int n)
{
    uint32_t mid = 0;
    if (!n)
        return ~0u;
    g_bsearch_hi = n - 1;
    g_bsearch_lo = 0;
    while (mid = (uint32_t)(g_bsearch_lo + g_bsearch_hi + 1) >> 1, g_bsearch_lo <= g_bsearch_hi) {
        uint32_t i = mid & 0x3fffffff;
        const char *s = base + offsets[mid];
        const char *k = key;
        char a, b;
        for (;;) {
            a = *s++;
            b = *k++;
            if (!a) {
                if (!b)
                    return i;
                break; /* key is longer: it sorts after */
            }
            if (a != b)
                break;
        }
        if (a && b < a)
            g_bsearch_hi = (int32_t)i - 1;
        else
            g_bsearch_lo = (int32_t)i + 1;
    }
    return ~mid;
}

LPVOID __stdcall winmalloc(SIZE_T size, int zero)
{
    HGLOBAL h;
    if (!size)
        return NULL;
    h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | (zero ? GMEM_ZEROINIT : 0), size);
    if (!h)
        return NULL;
    g_mem_used += GlobalSize(h);
    return GlobalLock(h);
}

LPVOID __stdcall winfree(LPCVOID p)
{
    HGLOBAL h;
    if (p && (h = GlobalHandle(p)) != NULL) {
        GlobalUnlock(h);
        g_mem_used -= GlobalSize(h);
        GlobalFree(h);
    }
    return NULL;
}

LPVOID __stdcall winptrcpy(LPCVOID p)
{
    HGLOBAL h;
    SIZE_T n;
    LPVOID copy;
    if (!p || (h = GlobalHandle(p)) == NULL)
        return NULL;
    n = GlobalSize(h);
    copy = winmalloc(n, 0);
    if (copy)
        memcpy(copy, p, n);
    return copy;
}

LPVOID __stdcall winrealloc(LPCVOID p, SIZE_T size, int zero)
{
    HGLOBAL h;
    SIZE_T old;
    if (!p)
        return winmalloc(size, zero);
    h = GlobalHandle(p);
    GlobalUnlock(h);
    old = GlobalSize(h);
    h = GlobalReAlloc(h, size, GMEM_MOVEABLE | (zero ? GMEM_ZEROINIT : 0));
    if (!h)
        return NULL;
    g_mem_used += GlobalSize(h) - old;
    return GlobalLock(h);
}

LPVOID __stdcall CopyString(LPCSTR s)
{
    int n = lstrlenA(s) + 1;
    LPVOID p = winmalloc((SIZE_T)n, 0);
    if (p)
        memcpy(p, s, (size_t)n);
    return p;
}

uint32_t __stdcall mem_unused(uint32_t unused)
{
    (void)unused;
    return 0;
}

uint32_t __stdcall MemMgrPreAllocate(uint32_t unused)
{
    (void)unused;
    return 0;
}

uint32_t __cdecl MemMgrSize(void)
{
    return g_mem_used;
}

uint32_t __stdcall MemSetNotify(uint32_t unused)
{
    (void)unused;
    return 1;
}

/* ---- trigonometry ---- */

int __stdcall NormalizeAngle(int a)
{
    a %= 3600;
    if (a < 0)
        a += 3600;
    return a;
}

int __stdcall GetQuadrant(int a)
{
    return a < 900 ? 0 : a < 1800 ? 1 : a < 2700 ? 2 : 3;
}

int __stdcall Sine(int a)
{
    int neg, i;
    a = NormalizeAngle(a);
    neg = a > 1799;
    if (neg)
        a -= 1800;
    i = a % 900;
    if (a > 899)
        i = 900 - i;
    return neg ? -(int)g_sine[i] : g_sine[i];
}

int __stdcall Cosine(int a)
{
    int neg, i;
    a = NormalizeAngle(a - 900);
    i = a % 900;
    neg = a > 1799;
    if (neg)
        a -= 1800;
    if (a > 899)
        i = 900 - i;
    return neg ? g_sine[i] : -(int)g_sine[i];
}

int __stdcall Tangent(int a)
{
    int neg;
    a = NormalizeAngle(a);
    neg = a > 1800;
    if (neg)
        a -= 1800;
    /* original: a == 1800 reads one entry past the table */
    return neg ? -g_tangent[a] : g_tangent[a];
}

int __stdcall ReflectAngle(int a, int mirror)
{
    return NormalizeAngle(mirror * 2 - a);
}

/* ---- debug output ---- */

void __cdecl utils_check_pointers(void)
{
    if (g_open_pointers)
        DebugStr("Number of pointers open = %d");
    g_open_pointers = 0;
}

void __cdecl utils_exit_nothing(void)
{
}

void __cdecl utils_reset_pointers(void)
{
    g_open_pointers = 0;
}

uint32_t __cdecl utils_reset_memory(void)
{
    g_mem_used = 0;
    return 1;
}

void __stdcall DebugMessage(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a, (void)b, (void)c, (void)d, (void)e;
}

/* printf-style; goes to the debug window (if shown) and to the debug hook */
void __cdecl DebugStr(LPCSTR fmt, ...)
{
    char buf[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = wvsprintfA(buf, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 3)
        n = (int)sizeof buf - 3; /* original: no limit */
    buf[n] = '\r';
    buf[n + 1] = '\n';
    buf[n + 2] = 0;
    if (g_debug_wnd && IsWindowVisible(g_debug_wnd)) {
        SendMessageA(g_debug_wnd, 0x41a, (WPARAM)(n + 3), (LPARAM)buf);
        UpdateWindow(g_debug_wnd);
    }
    if (g_debug_hook)
        g_debug_hook(buf, g_debug_hook_arg);
}

void __cdecl SetDebugOutputHook(void(__stdcall *hook)(char *, uint32_t), uint32_t arg)
{
    g_debug_hook = hook;
    g_debug_hook_arg = arg;
}

void __stdcall UtilsInitDebug(HWND wnd)
{
    g_debug_wnd = wnd;
    utils_reset_pointers();
}

void __cdecl UtilsExitDebug(void)
{
    utils_check_pointers();
}

/* ---- paths ---- */

HMMIO __stdcall FileExist(LPSTR path)
{
    return mmioOpenA(path, NULL, MMIO_EXIST);
}

/* Splits path into folder (without the last backslash; a drive keeps its colon) and file */
void __stdcall GetPathAndFile(LPCSTR path, char *dir, LPSTR file)
{
    char *p;
    file[0] = 0;
    lstrcpyA(dir, path);
    p = find_last(dir, '\\');
    if (!p)
        p = find_first(dir, ':');
    if (!p) {
        dir[0] = 0;
        lstrcpyA(file, path);
        return;
    }
    lstrcpyA(file, p + 1);
    if (*p == '\\')
        *p = 0;
    else
        p[1] = 0;
}

/* dir + file, unless file is already absolute; then made fully qualified by Windows */
void __stdcall MakeFullPath(char *out, char *dir, char *file)
{
    if (file[0] == '\\') {
        out[0] = 0;
        if (dir && dir[1] == ':') {
            out[0] = dir[0];
            out[1] = dir[1];
            out[2] = 0;
        }
        lstrcatA(out, file);
    } else if (!find_first(file, ':')) {
        if (dir != out)
            lstrcpyA(out, dir);
        if (find_first(out, '\\') && out[lstrlenA(out) - 1] != '\\')
            lstrcatA(out, "\\");
        lstrcatA(out, file);
    } else {
        lstrcpyA(out, file);
    }
    mmioOpenA(out, NULL, MMIO_PARSE);
}

char *__stdcall SkipPath(char *path)
{
    char *p = find_last(path, '\\');
    if (!p)
        p = find_first(path, ':');
    return p ? p + 1 : path;
}

/* out = the module's folder + the file name part of name */
void __stdcall MakeModulePath(HMODULE module, LPSTR out, char *name)
{
    char path[260];
    GetModuleFileNameA(module, path, 0x100);
    lstrcpyA(SkipPath(path), SkipPath(name));
    lstrcpyA(out, path);
}

/* ---- library ---- */

uint8_t __cdecl GetProcessorType(void)
{
    return g_processor;
}

/* 3 = 386, 4 = 486, 5 = Pentium (unless dominion.ini [utils] Pentium=0), 0 = other */
uint32_t __cdecl utils_detect_processor(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    switch (si.dwProcessorType) {
    case PROCESSOR_INTEL_386: return 3;
    case PROCESSOR_INTEL_486: return 4;
    case PROCESSOR_INTEL_PENTIUM: return g_pentium ? 5 : 4;
    default: return 0;
    }
}

UINT __stdcall utils_ini_int(LPCSTR key, INT def)
{
    return GetPrivateProfileIntA("utils", key, def, "dominion.ini");
}

uint32_t __cdecl UtilsInitLibrary(void)
{
    uint32_t r = utils_reset_memory();
    g_pentium = utils_ini_int("Pentium", 1);
    g_processor = (uint8_t)utils_detect_processor();
    return r;
}

void __cdecl UtilsExitLibrary(void)
{
    utils_exit_nothing();
}

static uint8_t lower(uint8_t c) { return c >= 'A' && c <= 'Z' ? c | 0x20 : c; }

/* LinearSearch (0x1003d8b4, hand-written assembly; unused by Dominion): finds `key` in a
 * list of `n` zero-terminated strings, ignoring case when `nocase`. Returns the entry's
 * offset from the list start in the high word and its number in the low word, or -1.
 * The original, ignoring case, ran past the end of the list when the key started with
 * the whole last entry; ours stops there. */
uint32_t __stdcall LinearSearch(const char *key, const char *list, uint32_t n, uint32_t nocase)
{
    const uint8_t *e = (const uint8_t *)list;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *p = e, *k = (const uint8_t *)key;
        while (*p && (nocase ? lower(*p) == lower(*k) : *p == *k))
            p++, k++;
        if (!*p && !*k)
            return (uint32_t)(e - (const uint8_t *)list) << 16 | (i & 0xffff);
        while (*p)
            p++;
        e = p + 1;
    }
    return 0xffffffffu;
}
