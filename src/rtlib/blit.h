/* The sprite engine's assembly routines in portable C (blit.c) */
#ifndef DOMINION_RTLIB_BLIT_H
#define DOMINION_RTLIB_BLIT_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif

int __stdcall rect_intersect(int32_t *out, const int32_t *a, const int32_t *b);
int __stdcall rect_union(int32_t *out, const int32_t *a, const int32_t *b);
void __stdcall palette_fade(uint8_t *dst, const uint8_t *src, uint32_t step, uint32_t steps, uint32_t n);
void __stdcall dib_fill8(void *bm, int x, int y, int w, int h, uint32_t color);

#endif
