/* PCM conversion between formats (resample.c) */
#ifndef DOMINION_RTLIB_RESAMPLE_H
#define DOMINION_RTLIB_RESAMPLE_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif

/* the fields of a WAVEFORMAT / PCMWAVEFORMAT */
typedef struct WaveFormat {
    uint16_t tag, channels;
    uint32_t rate, bytes_per_sec;
    uint16_t block_align, bits;
} WaveFormat;

/* st: 9 bytes of state */
uint32_t __stdcall resample_setup(uint8_t *st, const WaveFormat *out, const WaveFormat *in, uint32_t len);
uint32_t __stdcall resample(const uint8_t *st, uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len,
                            uint32_t *used);

#endif
