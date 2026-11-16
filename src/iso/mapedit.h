/* Game logic: tile background, tile library, map editor helpers (mapedit.c) */
#ifndef DOMINION_ISO_MAPEDIT_H
#define DOMINION_ISO_MAPEDIT_H

#include <stdint.h>

int __cdecl tiles_free(void);
int __stdcall tiles_background(uint8_t *iso, uint32_t w, uint32_t h, int unused);
int __cdecl map_free(void);
int __stdcall tiles_load(char *name, int32_t *sizes, int32_t total, int whole);
int __cdecl IsoUnloadMaskSubSystem(uint8_t *iso);
int __stdcall IsoCompileTileLibrary(uint8_t *iso, uint32_t list, uint16_t tag, uint32_t name_id);
void __stdcall IsoAutofillMap(uint8_t *iso, int w, int h, int unused, uint32_t first, int n, int only_range);

#endif
