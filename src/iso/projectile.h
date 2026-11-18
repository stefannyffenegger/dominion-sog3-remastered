/* Game logic: projectiles (projectile.c) */
#ifndef DOMINION_ISO_PROJECTILE_H
#define DOMINION_ISO_PROJECTILE_H

#include <stdint.h>

int32_t __cdecl projectile_launch(uint32_t id, uint32_t unit, uint32_t x, uint32_t y, uint32_t target,
                                  uint32_t weapon);
int32_t __cdecl projectile_fly(uint32_t id, int32_t trail);

#endif
