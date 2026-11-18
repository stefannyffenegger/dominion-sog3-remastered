/* Game logic: the status line, memory use, scrolling the view (status.c) */
#ifndef DOMINION_ISO_STATUS_H
#define DOMINION_ISO_STATUS_H

#include <stdint.h>

int __cdecl status_text(const char *text, int ms);
int32_t __stdcall IsoMouseInScrollRegion(uint8_t *iso, int32_t x, int32_t y);
void __cdecl IsoDebugStrIntf(uint8_t *iso, uint32_t str);
uint32_t __cdecl IsoMemUsed(uint8_t *iso, uint32_t label, int32_t log);
void __cdecl IsoSetGameState(uint8_t *iso, int32_t state);
void __cdecl view_scroll_to(int32_t x, int32_t y);

#endif
