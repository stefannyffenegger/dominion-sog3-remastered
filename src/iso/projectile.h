/* Game logic: projectiles (projectile.c) */
#ifndef DOMINION_ISO_PROJECTILE_H
#define DOMINION_ISO_PROJECTILE_H

#include <stdint.h>

int32_t __cdecl projectile_launch(uint32_t id, uint32_t unit, uint32_t x, uint32_t y, uint32_t target,
                                  uint32_t weapon);
int32_t __cdecl projectile_fly(uint32_t id, int32_t trail);
void __cdecl projectile_explode(uint32_t id);
void __cdecl projectile_explode_report(uint32_t id);
int32_t __cdecl projectile_beam_step(uint32_t id);
int32_t __cdecl projectile_launch_beam(uint32_t id, uint32_t unit, uint32_t x, uint32_t y, uint32_t target,
                                       uint32_t weapon);

#endif
