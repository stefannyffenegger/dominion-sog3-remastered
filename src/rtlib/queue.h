/* The sprite engine's script queues (queue.c) */
#ifndef DOMINION_RTLIB_QUEUE_H
#define DOMINION_RTLIB_QUEUE_H

#include <stdint.h>
#include <windows.h>

void __stdcall sprite_callback(uint8_t *s);
uint32_t __stdcall cmd_size(const uint16_t *c);
void __stdcall line_steps(uint8_t *c);
void __cdecl queue_init_sizes(void);
int __stdcall cmd_start(uint8_t *s);
uint8_t *__stdcall cmd_next(uint8_t *s);
uint32_t __stdcall sprite_step(uint8_t *s);
void __stdcall cmd_flip(uint8_t *s);
void __stdcall cmd_level(uint8_t *s, uint8_t *c);
void __stdcall cmd_jump(uint8_t *s, uint8_t *c);
uint32_t __stdcall cmd_send_msg(uint8_t *s, uint32_t size);
void __stdcall SpriteEmptyQueue(uint8_t *hsl, uint8_t *s);
void __stdcall SpriteSetMessageProc(uint8_t *hsl, uint32_t proc);
int __stdcall SpriteSetQueue(uint8_t *hsl, uint8_t *s, const void *cmds, SIZE_T size);

#endif
