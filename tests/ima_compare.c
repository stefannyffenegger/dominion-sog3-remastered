/* IMA ADPCM: our codec vs the original engine's on the same pseudo-random input.
 * Native build prints our checksums; the Windows build (-DREF) loads an unmodified
 * Rtlib32.dll and prints the original's:
 *   ima_compare            vs   wine ima_compare_ref.exe Rtlib32.dll */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef REF
#include <windows.h>
typedef struct ImaState { int32_t sample[2]; uint16_t index[2]; } ImaState;
typedef uint32_t(__stdcall *Fn)(ImaState *, void *, const void *, uint32_t, int);
static Fn unpack, pack;
#else
#include "../src/rtlib/codec.h"
#endif

static uint64_t fnv(const void *v, size_t n)
{
    const uint8_t *p = v;
    uint64_t h = 1469598103934665603ull;
    while (n--)
        h = (h ^ *p++) * 1099511628211ull;
    return h;
}

int main(int argc, char **argv)
{
    static uint8_t in[65536];
    static int16_t pcm[131072], wave[65536];
    static uint8_t ima[32768]; /* 2 samples per byte */
    uint32_t x = 12345;
#ifdef REF
    HMODULE m = LoadLibraryA(argv[1]);
    unpack = (Fn)GetProcAddress(m, "ima2pcm");
    pack = (Fn)((uint8_t *)m + 0x47c78);
#else
    (void)argc, (void)argv;
#endif
    for (size_t i = 0; i < sizeof in; i++)
        in[i] = (uint8_t)((x = x * 1103515245 + 12345) >> 16);
    for (size_t i = 0; i < 65536; i++) /* a loud sweep with noise */
        wave[i] = (int16_t)((int)((i * i / 7) % 60000) - 30000 + (int)((x = x * 1103515245 + 12345) >> 20) % 300);
    for (int stereo = 0; stereo < 2; stereo++) {
        ImaState s;
        memset(&s, 0, sizeof s);
#ifdef REF
        unpack(&s, pcm, in, sizeof in, stereo);
#else
        ima_unpack(&s, pcm, in, sizeof in, stereo);
#endif
        printf("unpack stereo=%d %016llx\n", stereo, (unsigned long long)fnv(pcm, sizeof pcm));
        memset(&s, 0, sizeof s);
#ifdef REF
        pack(&s, ima, wave, sizeof wave, stereo);
#else
        ima_pack(&s, ima, wave, sizeof wave, stereo);
#endif
        printf("pack   stereo=%d %016llx\n", stereo, (unsigned long long)fnv(ima, sizeof ima));
    }
    return 0;
}
