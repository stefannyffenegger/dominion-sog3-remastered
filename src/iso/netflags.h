/* Game logic: network flags (netflags.c) */
#ifndef DOMINION_ISO_NETFLAGS_H
#define DOMINION_ISO_NETFLAGS_H

#include <stdint.h>

int32_t __cdecl net_delay(void);
void __stdcall IsoSetServerFlag(uint8_t *iso, int32_t server);
int32_t __stdcall IsoGetServerFlag(uint8_t *iso);
void __stdcall IsoSetNetworkPlay(uint8_t *iso, int32_t on, int32_t value);
int32_t __stdcall IsoGetNetworkPlay(uint8_t *iso);
int32_t __stdcall net_pump(uint8_t *iso);
void __stdcall IsoInitSDLShowText(uint8_t *iso, uint32_t res1, int32_t a, uint32_t res2, int32_t b, int32_t c);
uint8_t __stdcall IsoGetWXPInvalid(uint8_t *iso);
int32_t __stdcall IsoInitCRCCheck(uint8_t *iso, int32_t which);
void __stdcall IsoDebugScriptMessageProc(uint8_t *iso, int32_t starting);
void __stdcall IsoInitNetworkSprites(uint8_t *iso, int32_t a, int32_t b);

#endif
