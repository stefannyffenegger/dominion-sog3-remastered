/* Our Rtlib32.dll, stage 1: a shell around the original engine.
 *
 * All exports forward to Rtlib32_orig.dll (rtlib32.def). Each function we have rewritten
 * is listed in ported.h; at load time the original's copy of it is patched to jump to
 * ours. So both outside callers and the engine's own internal calls run our code, and
 * the original's data stays the single copy everyone uses.
 *
 * DOMINION_ORIGINAL=ResOpenFile,ResCloseFile (or "all") leaves those functions original,
 * for comparing behavior. */
#include "orig.h"
#include "rt.h" /* prototypes of everything in ported.h */
#include <stdarg.h>

uint8_t *g_orig;

/* Makes Rtlib32_orig.dll a load-time dependency, so it is loaded (and its C runtime
 * initialized) before our DllMain runs. */
__declspec(dllimport) int __cdecl RtStartup(void);
void *volatile g_keep_orig_import;

void rt_debug(const char *fmt, ...)
{
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}

__attribute__((naked)) uint32_t __cdecl orig_call2_unsafe(void *fn, uint32_t a, uint32_t b)
{
    (void)fn, (void)a, (void)b;
    __asm__ volatile("pushl %ebx\n\t"
                     "pushl %esi\n\t"
                     "pushl %edi\n\t"
                     "pushl %ebp\n\t"
                     "movl 20(%esp), %eax\n\t" /* fn */
                     "pushl 28(%esp)\n\t"      /* b */
                     "pushl 28(%esp)\n\t"      /* a */
                     "call *%eax\n\t"           /* stdcall: pops a and b */
                     "popl %ebp\n\t"
                     "popl %edi\n\t"
                     "popl %esi\n\t"
                     "popl %ebx\n\t"
                     "ret");
}

typedef struct Hook {
    uint32_t addr;   /* original address */
    void *func;      /* our rewrite */
    const char *name;
} Hook;

static const Hook hooks[] = {
#define PORTED(addr, name) {addr, (void *)name, #name},
#include "ported.h"
#undef PORTED
    {0, NULL, NULL},
};

/* Is name in the comma-separated list? */
static int listed(const char *list, const char *name)
{
    size_t n = (size_t)lstrlenA(name);
    if (!lstrcmpA(list, "all"))
        return 1;
    while (*list) {
        const char *end = list;
        while (*end && *end != ',')
            end++;
        if ((size_t)(end - list) == n && !memcmp(list, name, n))
            return 1;
        list = *end ? end + 1 : end;
    }
    return 0;
}

static void install_hooks(void)
{
    char keep[1024] = "";
    int installed = 0, skipped = 0;
    GetEnvironmentVariableA("DOMINION_ORIGINAL", keep, sizeof keep);
    for (const Hook *h = hooks; h->func; h++) {
        uint8_t *p = ORIG(uint8_t *, h->addr);
        DWORD old;
        int32_t rel;
        if (keep[0] && listed(keep, h->name)) {
            skipped++;
            continue;
        }
        rel = (int32_t)((uint8_t *)h->func - (p + 5));
        VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
        p[0] = 0xE9; /* jmp rel32 */
        memcpy(p + 1, &rel, 4);
        VirtualProtect(p, 5, old, &old);
        installed++;
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    rt_debug("RTLIB: %d functions rewritten, %d left original\n", installed, skipped);
}

/* ---- first-call tracing ----
 * DOMINION_TRACE=<file>: the file lists original addresses (hex, one per line, an optional
 * name after a space). Each listed function logs "RTLIB: first call <address> <name>" the
 * first time it runs, then is restored and runs normally. Finds the code the game uses. */
#define MAX_TRACE 4096
typedef struct Trace {
    uint8_t *at;
    uint8_t saved[5];
    char name[40];
} Trace;
static Trace *g_trace;
static uint8_t *g_thunks;

void __cdecl trace_hit(uint32_t i)
{
    Trace *t = &g_trace[i];
    DWORD old;
    VirtualProtect(t->at, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(t->at, t->saved, 5);
    VirtualProtect(t->at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), t->at, 5);
    rt_debug("RTLIB: first call %08x %s\n", (unsigned)(t->at - g_orig + ORIG_BASE), t->name);
}

static void install_trace(const char *path)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size, got, old;
    char *text;
    uint32_t n = 0;
    if (f == INVALID_HANDLE_VALUE)
        return;
    size = GetFileSize(f, NULL);
    text = HeapAlloc(GetProcessHeap(), 0, size + 1);
    g_trace = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_TRACE * sizeof *g_trace);
    g_thunks = VirtualAlloc(NULL, MAX_TRACE * 24, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ReadFile(f, text, size, &got, NULL);
    CloseHandle(f);
    text[got] = 0;
    for (char *p = text; *p && n < MAX_TRACE;) {
        uint32_t a = 0;
        int k = 0;
        while (*p == ' ' || *p == '\r' || *p == '\n')
            p++;
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X'))
            p += 2;
        for (;; p++) {
            char c = *p;
            int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (v < 0)
                break;
            a = a * 16 + (uint32_t)v;
        }
        while (*p == ' ')
            p++;
        while (*p && *p != '\r' && *p != '\n') {
            if (k < 39)
                g_trace[n].name[k++] = *p;
            p++;
        }
        if (a >= ORIG_BASE && a < ORIG_BASE + 0x9c000) {
            Trace *t = &g_trace[n];
            uint8_t *th = g_thunks + n * 24;
            int32_t rel;
            t->at = ORIG(uint8_t *, a);
            if (t->at[0] == 0xE9) /* already rewritten: not traced */
                continue;
            memcpy(t->saved, t->at, 5);
            /* pushad; push n; call trace_hit; add esp,4; popad; push at; ret */
            th[0] = 0x60;
            th[1] = 0x68;
            memcpy(th + 2, &n, 4);
            th[6] = 0xE8;
            rel = (int32_t)((uint8_t *)trace_hit - (th + 11));
            memcpy(th + 7, &rel, 4);
            th[11] = 0x83, th[12] = 0xC4, th[13] = 0x04;
            th[14] = 0x61;
            th[15] = 0x68;
            memcpy(th + 16, &t->at, 4);
            th[20] = 0xC3;
            VirtualProtect(t->at, 5, PAGE_EXECUTE_READWRITE, &old);
            t->at[0] = 0xE9;
            rel = (int32_t)(th - (t->at + 5));
            memcpy(t->at + 1, &rel, 4);
            VirtualProtect(t->at, 5, old, &old);
            n++;
        }
    }
    HeapFree(GetProcessHeap(), 0, text);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    rt_debug("RTLIB: tracing %d functions\n", n);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_keep_orig_import = (void *)RtStartup;
        g_orig = (uint8_t *)GetModuleHandleA("Rtlib32_orig.dll");
        if (!g_orig) {
            MessageBoxA(NULL, "Rtlib32_orig.dll (the original engine) is missing.", "Dominion", 0);
            return FALSE;
        }
        install_hooks();
        {
            char path[MAX_PATH];
            if (GetEnvironmentVariableA("DOMINION_TRACE", path, sizeof path))
                install_trace(path);
        }
    }
    return TRUE;
}
