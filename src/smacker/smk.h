/* Smacker (SMK2/SMK4) video decoder. Portable C, no OS calls.
 *
 * Works on a whole file in memory. Frames must be decoded in order (frame 0 first);
 * smk_rewind() starts again. Output per frame: an 8-bit image, the palette, and the
 * decoded PCM of each audio track. */
#ifndef DOMINION_SMK_H
#define DOMINION_SMK_H

#include <stddef.h>
#include <stdint.h>

#define SMK_TRACKS 7

typedef struct SmkAlloc {
    void *(*alloc)(size_t size); /* must return zeroed memory */
    void (*free)(void *p);
} SmkAlloc;

typedef struct SmkTrack {
    int present;
    unsigned rate;     /* samples per second */
    int channels;      /* 1 or 2 */
    int bits;          /* 8 or 16 */
    uint8_t *pcm;      /* decoded audio of the current frame (16-bit: signed LE, 8-bit: unsigned) */
    uint32_t pcm_size; /* bytes in pcm */
} SmkTrack;

typedef struct Smk {
    uint32_t width, height;
    uint32_t frames;    /* frames to play (a looping "ring frame" is not counted) */
    uint32_t frame_us;  /* microseconds per frame */
    uint32_t cur_frame; /* last decoded frame, or UINT32_MAX before the first */
    uint8_t *image;     /* width * height palette indexes */
    uint8_t palette[768];
    int palette_changed; /* the last decoded frame changed the palette */
    SmkTrack track[SMK_TRACKS];
    struct SmkPriv *priv;
} Smk;

/* Returns NULL if the data is not a supported Smacker file. data must stay valid. */
Smk *smk_open(const uint8_t *data, size_t size, const SmkAlloc *alloc);
void smk_close(Smk *s);

/* Decode the next frame. Returns 1, or 0 at the end or on a broken frame. */
int smk_next_frame(Smk *s);
void smk_rewind(Smk *s);

#endif
