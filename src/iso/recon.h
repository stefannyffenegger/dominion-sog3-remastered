/* Game logic: scouting (recon.c) */
#ifndef DOMINION_ISO_RECON_H
#define DOMINION_ISO_RECON_H

#include <stdint.h>

int __cdecl recon_next_sector(uint32_t id, int16_t mode);
int __cdecl cell_blocked(int x, uint32_t y);
int __cdecl recon_target_in_sector(uint32_t id, uint32_t sector);
uint32_t __cdecl sector_seen_by(uint8_t *s, int16_t player);
int __cdecl tile_line_clear(int16_t *to, int16_t *from);
int __cdecl sector_line_clear(int16_t a, int16_t b);
int16_t __cdecl find_fogged_sector(uint8_t *u);
int __cdecl recon_double_click(uint32_t id);
void __cdecl recon_double_click_x(uint32_t id);
int16_t __cdecl percent_unseen(int16_t player);

#endif
