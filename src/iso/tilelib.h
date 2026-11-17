/* Game logic: the mask and tile libraries (tilelib.c) */
#ifndef DOMINION_ISO_TILELIB_H
#define DOMINION_ISO_TILELIB_H

#include <stdint.h>

int __cdecl IsoLoadMaskLibrary(uint8_t *iso, uint32_t name_id);
int __cdecl tilelib_free(void);
int __stdcall tilelib_load(char *name, int32_t *used, int32_t unused, uint32_t n, int whole);
int __stdcall tilelib_background(uint8_t *iso, uint32_t w, uint32_t h, int32_t unused);

#endif
