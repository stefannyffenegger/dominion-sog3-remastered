/* Decode a Smacker file with our decoder and write raw output for comparison with
 * ffmpeg (see tests/smk_compare.sh):
 *   smk_dump FILE.SMK frames.rgb audio.pcm [max_frames]
 * frames.rgb: every frame as 24-bit RGB; audio.pcm: track 0 as decoded. */
#include <stdio.h>
#include <stdlib.h>

#include "smk.h"

static void *zalloc(size_t n) { return calloc(1, n); }

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s FILE.SMK frames.rgb audio.pcm [max_frames]\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(size);
    if (fread(data, 1, size, f) != (size_t)size)
        return 1;
    fclose(f);

    SmkAlloc a = {zalloc, free};
    Smk *s = smk_open(data, size, &a);
    if (!s) {
        fprintf(stderr, "not a supported Smacker file\n");
        return 1;
    }
    unsigned long max = argc > 4 ? strtoul(argv[4], NULL, 10) : s->frames;
    fprintf(stderr, "%ux%u, %u frames, %u us/frame, audio %u Hz %d ch %d bit\n", s->width,
            s->height, s->frames, s->frame_us, s->track[0].rate, s->track[0].channels,
            s->track[0].bits);

    FILE *fv = fopen(argv[2], "wb"), *fa = fopen(argv[3], "wb");
    unsigned char *rgb = malloc((size_t)s->width * s->height * 3);
    unsigned long n = 0;
    while (n < max && n < s->frames && smk_next_frame(s)) {
        for (size_t i = 0; i < (size_t)s->width * s->height; i++) {
            const unsigned char *c = s->palette + s->image[i] * 3;
            rgb[i * 3] = c[0];
            rgb[i * 3 + 1] = c[1];
            rgb[i * 3 + 2] = c[2];
        }
        fwrite(rgb, 3, (size_t)s->width * s->height, fv);
        fwrite(s->track[0].pcm, 1, s->track[0].pcm_size, fa);
        n++;
    }
    fprintf(stderr, "decoded %lu frames\n", n);
    fclose(fv);
    fclose(fa);
    smk_close(s);
    free(rgb);
    free(data);
    return 0;
}
