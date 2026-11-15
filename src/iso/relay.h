/* Game logic: power relays and their ring (relay.c) */
#ifndef DOMINION_ISO_RELAY_H
#define DOMINION_ISO_RELAY_H

#include <stdint.h>

int __cdecl relay_add(uint16_t id);
void __cdecl relay_remove(uint16_t id);
void __cdecl ring_show(int16_t id, uint32_t until, int16_t radius);
void __cdecl ring_hide(int16_t id);
void __cdecl ring_tick(uint32_t id);
void __cdecl ring_draw(int16_t id, int16_t angle, int16_t radius);
int __cdecl relay_selected(int16_t id);
uint16_t __cdecl relay_nearest(int16_t x, int16_t y, uint16_t player);
void __stdcall IsoShowClosestUmbilical(uint8_t *iso, int16_t x, int16_t y, uint16_t player);
void __cdecl unit_set_16b(uint8_t *p);

#endif
