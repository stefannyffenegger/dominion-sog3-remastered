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
    }
    return TRUE;
}
