/* The engine's compression exports (compress.c); the codecs are in codec.c */
#ifndef DOMINION_RTLIB_COMPRESS_H
#define DOMINION_RTLIB_COMPRESS_H

#include <stdint.h>
#include <windows.h>
#include "codec.h"

void *__cdecl DecompressBitmapInit(void);
int __stdcall DecompressBitmapExit(void *u);
int __stdcall DecompressBitmapData(void *u, const uint8_t *src, uint8_t *dst, uint32_t size);
void *__cdecl CompressBitmapInit(void);
void __stdcall CompressBitmapExit(void *p);
int __stdcall CompressBitmapWriteEOD(void *p, int(__stdcall *out)(void *, uint32_t, uint32_t),
                                     uint32_t user);
int __stdcall CompressBitmapData(void *p, uint32_t n, const uint8_t *src,
                                 int(__stdcall *out)(void *, uint32_t, uint32_t), uint32_t user);
uint32_t __stdcall ima2pcm(ImaState *s, int16_t *out, const uint8_t *in, uint32_t n, int stereo);
uint32_t __stdcall pcm2ima(ImaState *s, uint8_t *out, const int16_t *in, uint32_t n, int stereo);
struct WaveUnpacker;
struct WavePacker;
struct WaveUnpacker *__stdcall DecompressWaveInit(int16_t channels, uint32_t buf_size,
                                                  int(__stdcall *read)(uint32_t, void *, uint32_t),
                                                  uint32_t user, void *info);
uint32_t __stdcall DecompressWaveData(struct WaveUnpacker *u, int16_t *out, uint32_t n);
int __stdcall DecompressWaveReset(struct WaveUnpacker *u);
void __stdcall DecompressWaveExit(struct WaveUnpacker *u);
struct WavePacker *__stdcall CompressWaveInit(int16_t channels, uint32_t pcm_bytes,
                                              int(__stdcall *write)(uint32_t, void *, uint32_t),
                                              uint32_t user, int header);
int __stdcall CompressWaveData(struct WavePacker *p, const int16_t *in, uint32_t n);
void __stdcall CompressWaveExit(struct WavePacker *p);

#endif
