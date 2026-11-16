/* Game logic: loading a scenario (scenario.c) */
#ifndef DOMINION_ISO_SCENARIO_H
#define DOMINION_ISO_SCENARIO_H

#include <stdint.h>

int __stdcall scenario_load(uint8_t *iso, const char *name, int no_embedded, int ai);
int __stdcall IsoLoadScenario(uint8_t *iso, uint32_t name_id, int no_embedded, int ai);

#endif
