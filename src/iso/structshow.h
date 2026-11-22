/* Game logic: the animation of a structure's top object (structshow.c) */
#ifndef DOMINION_ISO_STRUCTSHOW_H
#define DOMINION_ISO_STRUCTSHOW_H

#include <stdint.h>

int32_t __cdecl structure_show(uint32_t id, int32_t mode);
void __cdecl structure_overlay(uint32_t id, int32_t on);

#endif
