/* Unpacks every bitmap of a resource file with our codec (src/rtlib/codec.c) and prints
 * one line per bitmap: id width height checksum. Native; compare with bitmap_ref.exe.
 *   bitmap_dump DOMINION.bin */
#include "../src/rtlib/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    while (n--)
        h = (h ^ *p++) * 1099511628211ull;
    return h;
}

static uint32_t u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int main(int argc, char **argv)
{
    FILE *f, *data[100] = {0};
    uint8_t h[0x1dc], *index;
    uint32_t n, idx_off, idx_size;
    char base[512], name[520];
    static LzwDecoder z;
    if (argc < 2 || !(f = fopen(argv[1], "rb")) || fread(h, 1, sizeof h, f) != sizeof h)
        return 1;
    idx_off = u32(h + 0x164);
    idx_size = u32(h + 0x168);
    n = idx_size / 10;
    index = malloc(idx_size);
    fseek(f, idx_off, SEEK_SET);
    if (fread(index, 1, idx_size, f) != idx_size)
        return 1;
    strcpy(base, argv[1]);
    *strrchr(base, '.') = 0;
    for (uint32_t id = 0; id < n; id++) {
        uint8_t *e = index + id * 10, rec[0x18], *packed, *pix;
        uint32_t off = u32(e + 2), size = u32(e + 6), w, ht, flags, pitch;
        if (e[0] != 1)
            continue;
        if (!data[e[1]]) {
            sprintf(name, "%s.%03d", base, e[1]);
            data[e[1]] = fopen(name, "rb");
        }
        fseek(data[e[1]], off, SEEK_SET);
        packed = malloc(size);
        if (fread(packed, 1, size, data[e[1]]) != size)
            return 1;
        memcpy(rec, packed, sizeof rec);
        w = u32(rec), ht = u32(rec + 4), flags = u32(rec + 8), pitch = (w + 3) & ~3u;
        if (flags & 0x80) { /* run-length sprite: kept packed by the engine */
            printf("%u %u %u rle\n", id, w, ht);
        } else {
            pix = malloc(pitch * ht + 1);
            if (bitmap_unpack(&z, packed + 0x18, size - 0x18, pix, pitch * ht))
                printf("%u %u %u %016llx\n", id, w, ht, (unsigned long long)fnv(pix, pitch * ht));
            else
                printf("%u %u %u FAILED\n", id, w, ht);
            free(pix);
        }
        free(packed);
    }
    return 0;
}
