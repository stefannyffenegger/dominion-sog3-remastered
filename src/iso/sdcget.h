/* Game logic: scenario description getters, energy wall links (sdcget.c) */
#ifndef DOMINION_ISO_SDCGET_H
#define DOMINION_ISO_SDCGET_H

#include <stdint.h>

int __cdecl IsoGetBriefTextLine(uint8_t *iso, uint32_t n, uint32_t v);
int __cdecl IsoGetBriefWav(uint8_t *iso, uint32_t v);
int __cdecl IsoGetMissionCine(uint8_t *iso, int n, uint32_t v);
int __cdecl IsoGetNextSDLFilename(uint8_t *iso, uint32_t v);
void __cdecl energywall_link(int x1, int y1, int x2, int y2);
int __cdecl sdc_load(char *file, char *dir, int run);
int __cdecl IsoLoadSDCFile(uint8_t *iso, uint32_t file_id, uint32_t dir_id, int run);

#endif
