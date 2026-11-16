/* Game logic: map cells and tiles for the scripts (maptiles.c) */
#ifndef DOMINION_ISO_MAPTILES_H
#define DOMINION_ISO_MAPTILES_H

#include <stdint.h>

void __stdcall IsoMessageBox(uint8_t *iso, uint32_t title_id, uint32_t text_id, uint32_t flags);
int __stdcall IsoRescanMap(uint8_t *iso);
void __cdecl map_clear_marks(void);
int __stdcall IsoSetTileLib(uint8_t *iso, int slot, uint32_t name_id);
int32_t __stdcall IsoGetTile(uint8_t *iso, int x, int y);
int __stdcall IsoSetTile(uint8_t *iso, uint32_t tile, int x, int y);
int __stdcall IsoSetTileSet(uint8_t *iso, int16_t tile, int x, int y);

#endif
