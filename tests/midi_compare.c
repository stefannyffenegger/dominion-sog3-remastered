/* Preparing MIDI data: our C (src/rtlib/midi_prepare.c) against the original's
 * AudioPrepareMidiData on random Standard MIDI Files. Windows program (runs under Wine):
 *   wine midi_compare.exe <unmodified Rtlib32.dll> */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../src/rtlib/midi_prepare.h"

/* the engine's allocator, for ours */
void *__stdcall winrealloc(const void *p, unsigned long size, int zero)
{
    (void)zero;
    return p ? HeapReAlloc(GetProcessHeap(), 0, (void *)p, size) : HeapAlloc(GetProcessHeap(), 0, size);
}
void *__stdcall winfree(const void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, (void *)p);
    return NULL;
}

typedef uint32_t *(__stdcall *Prepare)(const uint8_t *, uint32_t, uint32_t *);

static uint32_t seed = 99;
static uint32_t rnd(uint32_t n)
{
    seed = seed * 1103515245 + 12345;
    return (seed >> 8) % n;
}

static uint8_t *put_be(uint8_t *p, uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; i--)
        *p++ = (uint8_t)(v >> (i * 8));
    return p;
}
static uint8_t *put_varlen(uint8_t *p, uint32_t v)
{
    uint8_t tmp[5];
    int n = 0;
    do
        tmp[n++] = v & 0x7f, v >>= 7;
    while (v);
    while (n--)
        *p++ = tmp[n] | (n ? 0x80 : 0);
    return p;
}

/* a random file; sometimes broken */
static uint32_t make_file(uint8_t *buf)
{
    int tracks = 1 + (int)rnd(4);
    uint8_t *p = buf;
    memcpy(p, "MThd", 4), p += 4;
    p = put_be(p, 6, 4);
    p = put_be(p, rnd(10) ? rnd(2) : 2, 2);
    p = put_be(p, (uint32_t)tracks, 2);
    p = put_be(p, rnd(20) ? 24 + rnd(960) : 0x8000 | rnd(100), 2);
    for (int t = 0; t < tracks; t++) {
        memcpy(p, rnd(30) ? "MTrk" : "MTrx", 4), p += 4;
        uint8_t *len = p;
        p += 4;
        uint8_t *start = p;
        int events = (int)rnd(40), running = 0;
        for (int e = 0; e < events; e++) {
            p = put_varlen(p, rnd(8) ? rnd(24) : rnd(4) ? rnd(400) : rnd(20000));
            int kind = (int)rnd(10);
            if (kind == 0) { /* tempo */
                *p++ = 0xff, *p++ = 0x51, *p++ = 3;
                p = put_be(p, 200000 + rnd(1000000), 3);
                running = 0;
            } else if (kind == 1) { /* another meta event */
                *p++ = 0xff, *p++ = (uint8_t)rnd(0x50);
                int n = (int)rnd(6);
                *p++ = (uint8_t)n;
                for (int i = 0; i < n; i++)
                    *p++ = (uint8_t)rnd(256);
                running = 0;
            } else if (kind == 2) { /* SysEx */
                *p++ = 0xf0;
                *p++ = 0x41;
                int n = (int)rnd(5);
                *p++ = (uint8_t)n;
                for (int i = 0; i < n; i++)
                    *p++ = (uint8_t)rnd(128);
                running = 0;
            } else {
                static const uint8_t st[] = {0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0};
                uint8_t s = (uint8_t)(st[rnd(7)] | rnd(16));
                int len = (s & 0xf0) == 0xc0 || (s & 0xf0) == 0xd0 ? 1 : 2;
                if (running && rnd(2)) { /* running status */
                    len = running;
                } else {
                    *p++ = s;
                    running = len;
                }
                for (int i = 0; i < len; i++)
                    *p++ = (uint8_t)rnd(128);
            }
        }
        put_be(len, (uint32_t)(p - start), 4);
    }
    uint32_t size = (uint32_t)(p - buf);
    if (!rnd(20))
        size = rnd(size + 1); /* cut short */
    return size;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    HMODULE m = argc > 1 ? LoadLibraryA(argv[1]) : NULL;
    Prepare orig = m ? (Prepare)GetProcAddress(m, "AudioPrepareMidiData") : NULL;
    if (!orig) {
        printf("usage: midi_compare <unmodified Rtlib32.dll>\n");
        return 2;
    }
    static uint8_t file[1 << 20];
    int failures = 0, made = 0;
    for (int i = 0; i < 20000; i++) {
        uint32_t size = make_file(file);
        uint32_t oa = 0x5a5a5a5a, ob = 0x5a5a5a5a;
        uint32_t *a = orig(file, size, &oa);
        uint32_t *b = AudioPrepareMidiData(file, size, &ob);
        int same = !a == !b && (!a || (oa == ob && !memcmp(a, b, oa)));
        made += a != NULL;
        if (!same && failures++ < 5)
            printf("  file %d (%u bytes): %s / %s, %u / %u bytes\n", i, size, a ? "ok" : "failed", b ? "ok" : "failed",
                   oa, ob);
        winfree(b);
    }
    printf("%d files prepared by both\n", made);
    printf(failures ? "FAILED: %d differences\n" : "all identical\n", failures);
    return failures != 0;
}
