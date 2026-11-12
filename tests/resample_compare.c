/* PCM conversion: our C (src/rtlib/resample.c) against the original's assembly through
 * its AudioResampleWave export, on random formats and data. Windows program (Wine):
 *   wine resample_compare.exe <unmodified Rtlib32.dll> */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "../src/rtlib/resample.h"

typedef uint32_t(__stdcall *ResampleWave)(const WaveFormat *out, const WaveFormat *in, uint32_t len, void *dst,
                                          const void *src);

static uint32_t seed = 4242;
static uint32_t rnd(uint32_t n)
{
    seed = seed * 1103515245 + 12345;
    return (seed >> 8) % n;
}

static void format(WaveFormat *f)
{
    static const uint32_t rates[] = {11025, 22050, 44100, 11025, 22050, 44100, 8000, 32000, 48000};
    f->tag = 1;
    f->channels = (uint16_t)(1 + rnd(2));
    f->rate = rates[rnd(9)];
    f->bits = rnd(2) ? 16 : 8;
    f->block_align = (uint16_t)(f->channels * f->bits / 8);
    f->bytes_per_sec = f->rate * f->block_align;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    HMODULE m = argc > 1 ? LoadLibraryA(argv[1]) : NULL;
    ResampleWave orig = m ? (ResampleWave)GetProcAddress(m, "AudioResampleWave") : NULL;
    if (!orig) {
        printf("usage: resample_compare <unmodified Rtlib32.dll>\n");
        return 2;
    }
    static uint8_t src[70000], a[2000000], b[2000000];
    int failures = 0;
    for (int i = 0; i < 20000; i++) {
        WaveFormat in, out;
        format(&in);
        format(&out);
        uint32_t len = rnd(4) ? rnd(64) : rnd(65536);
        for (uint32_t k = 0; k < len + 8; k++)
            src[k] = (uint8_t)rnd(256);
        memset(a, 0x5a, sizeof a);
        memset(b, 0x5a, sizeof b);
        uint32_t ra = orig(&out, &in, len, a, src);
        uint8_t st[9];
        uint32_t n = resample_setup(st, &out, &in, len);
        uint32_t rb = resample(st, b, n, src, len, NULL);
        if (ra != rb || memcmp(a, b, sizeof a)) {
            if (failures++ < 8)
                printf("  %u Hz %u ch %u bit -> %u Hz %u ch %u bit, %u bytes: %u / %u\n", in.rate, in.channels,
                       in.bits, out.rate, out.channels, out.bits, len, ra, rb);
        }
    }
    printf(failures ? "FAILED: %d differences\n" : "all identical\n", failures);
    return failures != 0;
}
