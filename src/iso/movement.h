/* Game logic: the movement process and the game tick (movement.c) */
#ifndef DOMINION_ISO_MOVEMENT_H
#define DOMINION_ISO_MOVEMENT_H

#include <stdint.h>

int32_t __stdcall IsoSetMoveDelay(uint8_t *iso, uint32_t delay, int dir);
int32_t __stdcall IsoInitMoveProcess(uint8_t *iso);
void __stdcall IsoStopMoveProcess(uint8_t *iso);
void __stdcall IsoPauseGame(uint8_t *iso, uint32_t paused, uint32_t unused);
void __stdcall IsoMovementLoop(uint8_t *iso);

#endif
