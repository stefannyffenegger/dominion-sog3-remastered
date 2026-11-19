/* Game logic: wall segments (walls.c) */
#ifndef DOMINION_ISO_WALLS_H
#define DOMINION_ISO_WALLS_H

#include <stdint.h>

int32_t __cdecl unit_is_post(int16_t unit);
int16_t __cdecl wall_segment_alloc(void);
int16_t __cdecl wall_segment_build(uint32_t a, uint32_t b);
void __cdecl wall_crush_between(uint8_t *a, uint8_t *b);
int32_t __cdecl walls_connect(uint32_t a, uint32_t b, uint32_t unused, int check);

int16_t __cdecl wall_piece_alloc(void);
void __cdecl wall_cell_set(int32_t x, int32_t y);
void __cdecl wall_side_cell_set(int32_t x, int32_t y);
void __cdecl wall_sides_set(int32_t x, uint32_t y, int16_t dir);
int16_t __cdecl wall_piece_at(int32_t x, uint32_t y, uint16_t dir, uint32_t owner);

int32_t __cdecl walls_connect_new(uint32_t a, uint32_t b);
void __cdecl IsoHideBeaconDirections(uint8_t *iso);
void __cdecl wall_piece_frame(int16_t k);
void __cdecl wall_pieces_refresh(void);

void __stdcall IsoInitEnergyWalls(uint8_t *iso, uint32_t wall, uint32_t arrow, uint32_t diamond, uint32_t nop);

void __cdecl post_connect_all(uint32_t post, int check);
void __cdecl post_update_walls(uint32_t post);
void __cdecl post_new(uint32_t post);
void __cdecl post_find_walls(uint32_t post);

#endif
