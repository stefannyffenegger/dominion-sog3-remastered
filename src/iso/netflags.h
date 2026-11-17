/* Game logic: network flags (netflags.c) */
#ifndef DOMINION_ISO_NETFLAGS_H
#define DOMINION_ISO_NETFLAGS_H

#include <stdint.h>

int32_t __cdecl net_delay(void);
void __stdcall IsoSetServerFlag(uint8_t *iso, int32_t server);
int32_t __stdcall IsoGetServerFlag(uint8_t *iso);
void __stdcall IsoSetNetworkPlay(uint8_t *iso, int32_t on, int32_t value);
int32_t __stdcall IsoGetNetworkPlay(uint8_t *iso);

#endif
