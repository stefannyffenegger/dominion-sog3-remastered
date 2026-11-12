/* Our Rtlib32.dll: the whole 7th Level engine, rewritten.
 *
 * The original engine stays in the game folder as Rtlib32_orig.dll, but none of its code
 * runs: it is mapped as data (no imports resolved, no entry point called). It provides
 * the starting values of the engine's globals (copied into our own memory: orig.h), the
 * strings and tables those point to, and its resources (strings, cursors, icons). No
 * original data is part of this source. */
#include "orig.h"
#include "rt.h"
#include <stdarg.h>

uint8_t *g_orig;
uint8_t *g_odata;

void rt_debug(const char *fmt, ...)
{
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}

/* Calls a 2-argument stdcall callback that may change EBX, ESI or EDI */
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

/* Same for any number of arguments: stdcall fn(args[0], ..., args[n-1]). The stack is
 * restored afterwards whatever the callee removed (game callbacks are not all careful). */
__attribute__((naked)) uint32_t __cdecl orig_stdcall_unsafe(void *fn, uint32_t n, const uint32_t *args)
{
    (void)fn, (void)n, (void)args;
    __asm__ volatile("pushl %ebx\n\t"
                     "pushl %esi\n\t"
                     "pushl %edi\n\t"
                     "pushl %ebp\n\t"
                     "movl %esp, %ebp\n\t"
                     "movl 20(%ebp), %eax\n\t"
                     "movl 24(%ebp), %ecx\n\t"
                     "movl 28(%ebp), %edx\n\t"
                     "1: testl %ecx, %ecx\n\t"
                     "jz 2f\n\t"
                     "decl %ecx\n\t"
                     "pushl (%edx,%ecx,4)\n\t"
                     "jmp 1b\n\t"
                     "2: call *%eax\n\t"
                     "movl %ebp, %esp\n\t"
                     "popl %ebp\n\t"
                     "popl %edi\n\t"
                     "popl %esi\n\t"
                     "popl %ebx\n\t"
                     "ret");
}

/* Maps the original engine (next to us) as data */
static int map_original(HINSTANCE self)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(self, path, MAX_PATH);
    while (n && path[n - 1] != '\\' && path[n - 1] != '/')
        n--;
    if (n + sizeof "Rtlib32_orig.dll" > MAX_PATH)
        return 0;
    lstrcpyA(path + n, "Rtlib32_orig.dll");
    g_orig = (uint8_t *)LoadLibraryExA(path, NULL, DONT_RESOLVE_DLL_REFERENCES);
    return g_orig != NULL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    if (!map_original(inst)) {
        MessageBoxA(NULL, "Rtlib32_orig.dll (the original engine's data) is missing.", "Dominion", 0);
        return FALSE;
    }
    /* the engine's globals: our copy of the original's data section, as the file has it */
    g_odata = VirtualAlloc(NULL, ORIG_DATA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_odata)
        return FALSE;
    memcpy(g_odata, g_orig + (ORIG_DATA - ORIG_BASE), ORIG_DATA_SIZE);
    /* what the original's entry point did besides starting its C library: the module
     * handle, here the mapped original's (the engine's resources are there) */
    rt_set_cef0((uint32_t)(uintptr_t)g_orig, DLL_PROCESS_ATTACH, 0);
    return TRUE;
}
