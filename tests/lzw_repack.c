/* Re-packs every LZW block of a resource file's bitmaps with our packer and checks that
 * the result is byte-identical to what the original packer stored.
 *   lzw_repack DOMINION.bin */
#include "../src/rtlib/codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int main(int argc, char **argv)
{
    FILE *f, *data[100] = {0};
    uint8_t h[0x1dc], *index;
    uint32_t n;
    char base[512], name[520];
    static LzwDecoder dz;
    static LzwEncoder ez;
    static uint8_t plain[0x10000], again[0x10000];
    int blocks[8] = {0}, same = 0, differ = 0;
    if (argc < 2 || !(f = fopen(argv[1], "rb")) || fread(h, 1, sizeof h, f) != sizeof h)
        return 1;
    n = u32(h + 0x168) / 10;
    index = malloc(n * 10);
    fseek(f, u32(h + 0x164), SEEK_SET);
    if (fread(index, 10, n, f) != n)
        return 1;
    strcpy(base, argv[1]);
    *strrchr(base, '.') = 0;
    for (uint32_t id = 0; id < n; id++) {
        uint8_t *e = index + id * 10, *packed, *p;
        uint32_t size = u32(e + 6);
        if (e[0] != 1)
            continue;
        if (!data[e[1]]) {
            sprintf(name, "%s.%03d", base, e[1]);
            data[e[1]] = fopen(name, "rb");
        }
        fseek(data[e[1]], u32(e + 2), SEEK_SET);
        packed = malloc(size);
        if (fread(packed, 1, size, data[e[1]]) != size)
            return 1;
        if (u32(packed + 8) & 0x80) {
            free(packed);
            continue;
        }
        for (p = packed + 0x18; p + 2 <= packed + size;) {
            uint16_t head;
            unsigned m, len;
            memcpy(&head, p, 2);
            m = head & PACK_METHOD;
            len = head & PACK_SIZE;
            blocks[m >> 13]++;
            if (m == PACK_END)
                break;
            if (m == PACK_LZW10 || m == PACK_LZW11 || m == PACK_LZW12) {
                int bits = m == PACK_LZW10 ? 10 : m == PACK_LZW11 ? 11 : 12;
                size_t k = lzw_unpack_block(&dz, bits, p + 2, len, plain, sizeof plain);
                size_t r = lzw_pack(&ez, bits, plain, k, again);
                if (r == len && !memcmp(again, p + 2, len))
                    same++;
                else
                    differ++;
            }
            p += 2 + len;
        }
        free(packed);
    }
    printf("blocks: raw %d rle %d lzw10 %d lzw11 %d lzw12 %d end %d\n", blocks[0], blocks[1],
           blocks[2], blocks[3], blocks[4], blocks[7]);
    printf("LZW blocks re-packed identically: %d, different: %d\n", same, differ);
    return differ != 0;
}
