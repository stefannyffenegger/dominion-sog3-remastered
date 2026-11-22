/* Game logic: the AI's current record, setting up one computer player (aiplayer.c) */
#ifndef DOMINION_ISO_AIPLAYER_H
#define DOMINION_ISO_AIPLAYER_H

#include <stdint.h>

int32_t __cdecl ai_current_value(void);
int32_t __cdecl ai_player_init(int32_t k);

#endif
