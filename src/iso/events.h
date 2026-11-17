/* Game logic: the scenario's timers and events (events.c) */
#ifndef DOMINION_ISO_EVENTS_H
#define DOMINION_ISO_EVENTS_H

#include <stdint.h>

int __cdecl sdc_file_info(char *path, uint32_t *info);
int __stdcall IsoGetSDCInfo(uint8_t *iso, int index, uint32_t dir_var, uint32_t name_var, uint32_t *info);
void __cdecl hotspot_entered(int s, uint32_t id);
void __cdecl event_count(int id);
void __cdecl timers_sort(void);
int __cdecl timer_compare(const void *pa, const void *pb);
void __cdecl timer_set(uint16_t id, int32_t at, int32_t period, void *cb, int32_t *args);
void __cdecl timer_disable(uint32_t id);
int __cdecl timer_enable(int id);
void __cdecl timer_fire(int n);
void __cdecl destroy_units(uint32_t obj, int effects);

#endif
