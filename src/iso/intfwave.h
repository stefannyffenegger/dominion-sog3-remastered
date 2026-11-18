/* Game logic: the interface's spoken messages (intfwave.c) */
#ifndef DOMINION_ISO_INTFWAVE_H
#define DOMINION_ISO_INTFWAVE_H

#include <stdint.h>

void __cdecl player_announce(uint32_t player, uint32_t wave, int32_t queued);
void __cdecl IsoAddIntfWaveToQueue(uint8_t *iso, uint32_t player, uint32_t wave, int32_t value, int32_t direct);
void __cdecl IsoJustPlayTheDamnWaveFile(uint8_t *iso, uint32_t str);
void __cdecl intf_wave_queue_clear(void);
void __cdecl IsoIntfWaveQueueDone(uint8_t *iso, int32_t wave);

#endif
