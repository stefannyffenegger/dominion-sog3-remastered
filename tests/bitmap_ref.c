/* Reference for bitmap_dump: loads the ORIGINAL engine (pass the path of an unmodified
 * Rtlib32.dll) and unpacks every bitmap with its own ResLoadResource. Run under Wine:
 *   wine bitmap_ref.exe Rtlib32.dll DOMINION.bin */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef void *(__stdcall *OpenFn)(LPSTR, DWORD);
typedef uint8_t *(__stdcall *LoadFn)(void *, uint32_t, uint32_t *);
typedef uint8_t(__stdcall *TypeFn)(void *, uint32_t);
typedef uint32_t(__stdcall *CountFn)(void *, uint32_t *);
typedef void *(__stdcall *FreeFn)(void *);

static uint64_t fnv(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    while (n--)
        h = (h ^ *p++) * 1099511628211ull;
    return h;
}

int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA(argv[1]);
    OpenFn open = (OpenFn)GetProcAddress(m, "ResOpenFile");
    LoadFn load = (LoadFn)GetProcAddress(m, "ResLoadResource");
    TypeFn type = (TypeFn)GetProcAddress(m, "ResGetType");
    CountFn count = (CountFn)GetProcAddress(m, "ResCountResources");
    FreeFn wfree = (FreeFn)GetProcAddress(m, "winfree");
    void *rf;
    uint32_t n, stored;
    if (!m || !open || !(rf = open(argv[2], 0))) {
        fprintf(stderr, "cannot open\n");
        return 1;
    }
    n = count(rf, &stored);
    for (uint32_t id = 0; id < stored; id++) {
        uint32_t size;
        uint8_t *p;
        if (type(rf, id) != 1)
            continue;
        p = load(rf, id, &size);
        if (!p) {
            printf("%u FAILED\n", id);
            continue;
        }
        {
            uint32_t w = *(uint32_t *)(p + 4), pitch = *(uint32_t *)(p + 8);
            uint32_t h = *(uint32_t *)(p + 12), flags = *(uint32_t *)(p + 16);
            uint8_t *bits = *(uint8_t **)(p + 0x20);
            if (flags & 0x80)
                printf("%u %u %u rle\n", id, w, h);
            else
                printf("%u %u %u %016llx\n", id, w, h, (unsigned long long)fnv(bits, pitch * h));
        }
        wfree(p);
    }
    (void)n;
    return 0;
}
