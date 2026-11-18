/* Game logic: the plugin's start and end (plugin.c) */
#ifndef DOMINION_ISO_PLUGIN_H
#define DOMINION_ISO_PLUGIN_H

#include <stdint.h>

uint32_t __cdecl plugin_free(void);
void __cdecl lib_free(uint8_t *iso);
int __stdcall plugin_setup(void *app, uint8_t *info);
uint8_t *__cdecl lib_init(void *app, uint8_t *info);

#endif
