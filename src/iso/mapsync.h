/* Multiplayer: both players checking that they have the same map files (mapsync.c) */
#ifndef DOMINION_ISO_MAPSYNC_H
#define DOMINION_ISO_MAPSYNC_H

#include <stdint.h>

uint32_t __cdecl map_receive_name(uint8_t *iso, uint32_t player, const char *name);
void __cdecl map_receive_info(uint8_t *iso, uint32_t player, const uint8_t *info);
uint32_t __cdecl map_send_info(uint8_t *iso, uint32_t player, const char *name);
uint32_t __cdecl map_note_exists(uint8_t *iso, uint32_t player, uint32_t exists);
uint32_t __cdecl map_note_valid(uint8_t *iso, uint32_t player, uint32_t valid);

#endif
