/* Game logic: new targets and squads of a computer player (squadnote.c) */
#ifndef DOMINION_ISO_SQUADNOTE_H
#define DOMINION_ISO_SQUADNOTE_H

#include <stdint.h>

int32_t __cdecl target_note_new(int32_t player, int32_t t, int32_t quiet);
int32_t __cdecl squad_link(int32_t player, int32_t k);

#endif
