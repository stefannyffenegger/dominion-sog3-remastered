/* Game logic: the scenario description (sdl.c) */
#ifndef DOMINION_ISO_SDL_H
#define DOMINION_ISO_SDL_H

#include <stdint.h>

int __cdecl sdl_load(char *file, char *dir, int check);
int __cdecl IsoLoadSDLFile(uint8_t *iso, uint32_t file_id, uint32_t dir_id, int check);

#endif
