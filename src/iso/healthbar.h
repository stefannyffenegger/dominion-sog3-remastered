/* Game logic: unit health bars (healthbar.c) */
#ifndef DOMINION_ISO_HEALTHBAR_H
#define DOMINION_ISO_HEALTHBAR_H

#include <stdint.h>

void __cdecl unit_health_bar(uint32_t id, int32_t hit);
void __cdecl unit_health_bar_init(uint32_t id);
void __cdecl unit_set_report_kind(uint16_t id);

#endif
