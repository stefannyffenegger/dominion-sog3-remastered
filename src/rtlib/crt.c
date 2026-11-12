/* The few C runtime functions the engine used from its own (statically linked, 1998
 * debug) C library, so we no longer borrow them from the original DLL. */
#include <windows.h>
#include "crt.h"

/* rand and srand: the C library's generator, kept per thread as the original's is.
 * A thread starts at seed 1; the TLS slot holds the seed XOR 1 so that its initial 0
 * means 1. */
static DWORD g_rand_slot = TLS_OUT_OF_INDEXES;

static DWORD rand_slot(void)
{
    if (g_rand_slot == TLS_OUT_OF_INDEXES) {
        DWORD s = TlsAlloc();
        if (InterlockedCompareExchange((LONG *)&g_rand_slot, (LONG)s, (LONG)TLS_OUT_OF_INDEXES) !=
            (LONG)TLS_OUT_OF_INDEXES)
            TlsFree(s);
    }
    return g_rand_slot;
}

void crt_srand(unsigned seed)
{
    TlsSetValue(rand_slot(), (void *)(uintptr_t)(seed ^ 1));
}

int crt_rand(void)
{
    DWORD slot = rand_slot();
    uint32_t seed = (uint32_t)(uintptr_t)TlsGetValue(slot) ^ 1;
    seed = seed * 214013 + 2531011;
    TlsSetValue(slot, (void *)(uintptr_t)(seed ^ 1));
    return (int)(seed >> 16 & 0x7fff);
}

/* atoi: leading white space, a sign, digits; overflow wraps as in the original */
int crt_atoi(const char *s)
{
    while (*s == ' ' || (*s >= '\t' && *s <= '\r'))
        s++;
    char sign = *s;
    if (sign == '-' || sign == '+')
        s++;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (uint32_t)(*s++ - '0');
    return (int)(sign == '-' ? 0u - v : v);
}

char *crt_strstr(const char *hay, const char *needle)
{
    if (!*needle)
        return (char *)hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *h == *n)
            h++, n++;
        if (!*n)
            return (char *)hay;
    }
    return NULL;
}

char *crt_strrchr(const char *s, int c)
{
    const char *r = NULL;
    do {
        if (*s == (char)c)
            r = s;
    } while (*s++);
    return (char *)r;
}

int crt_isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

/* reads up to n hexadecimal numbers separated by white space (sscanf "%x %x ...");
 * returns how many were read */
int crt_scan_hex(const char *s, uint32_t *out, int n)
{
    int got = 0;
    while (got < n) {
        while (*s == ' ' || (*s >= '\t' && *s <= '\r'))
            s++;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            s += 2;
        uint32_t v = 0;
        int digits = 0;
        for (;; s++, digits++) {
            char c = *s;
            if (c >= '0' && c <= '9')
                v = v << 4 | (uint32_t)(c - '0');
            else if ((c | 0x20) >= 'a' && (c | 0x20) <= 'f')
                v = v << 4 | (uint32_t)((c | 0x20) - 'a' + 10);
            else
                break;
        }
        if (!digits)
            break;
        out[got++] = v;
    }
    return got;
}

/* _getdcwd for the current drive (0) */
char *crt_getdcwd(int drive, char *buf, int n)
{
    (void)drive;
    return GetCurrentDirectoryA((DWORD)n, buf) ? buf : NULL;
}

/* A failed assertion of the engine's debug build. The original's debug C library
 * offered Abort/Retry/Ignore; ours notes it in the debug output and goes on. */
int crt_assert_failed(const char *file, int line, const char *expr)
{
    char msg[300];
    wsprintfA(msg, "Assertion failed: %s, %s line %d\r\n", expr, file, line);
    OutputDebugStringA(msg);
    return 0;
}
