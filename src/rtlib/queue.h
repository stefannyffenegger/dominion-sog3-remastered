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

void __stdcall queue_rewind(uint8_t *s);
uint8_t *__stdcall cmd_append(uint8_t *s, uint32_t type, uint32_t size);
uint8_t *__stdcall cmd_alloc(uint8_t *s, uint32_t type, uint32_t size);
void __stdcall msg_run(uint8_t *s, uint8_t *c);
int __stdcall msg_cels(uint8_t *s, const uint32_t *d, int now);
int __stdcall msg_type2(uint8_t *s, const uint32_t *d, uint32_t count, int now);
int __stdcall msg_loop_start(uint8_t *s, int now);
int __stdcall msg_to_background(uint8_t *s, int now);
int __stdcall msg_curve(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_loop(uint8_t *s, const uint32_t *d, uint32_t count, int now);
int __stdcall msg_flip(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_hide(uint8_t *s, int now);
int __stdcall msg_level(uint8_t *s, const uint32_t *d, int now);
int __stdcall msg_line(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_type10(uint8_t *s, const uint32_t *d, uint32_t count, int now);
int __stdcall msg_game(uint8_t *s, const uint32_t *d, uint32_t count, int now);
int __stdcall msg_wait(uint8_t *s, const uint32_t *d, int now);
int __stdcall msg_jump(uint8_t *s, const int32_t *d, uint32_t count, int now);
int __stdcall msg_priority(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_cel_range(uint8_t *s, const uint32_t *d, int now);
int __stdcall msg_redraw(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_motion(uint8_t *s, const int32_t *d, int now);
int __stdcall msg_speed(uint8_t *s, const int32_t *d, uint32_t count, int now);
int __stdcall msg_show(uint8_t *s, const uint32_t *d, uint32_t count, int now);
int __stdcall msg_dispatch(uint8_t *s, const uint32_t *msg, uint32_t count, int now);
void __stdcall SpritePostMessage(uint8_t *hsl, uint8_t *s, uint32_t count, const uint32_t *msg);
void __stdcall SpriteSendMessage(uint8_t *hsl, uint8_t *s, uint32_t count, const uint32_t *msg);

void __stdcall set_fx_table(uint8_t *hsl, int which, uint32_t table);
void __stdcall SpriteSetBlitFX(uint8_t *s, uint32_t fx, uint32_t arg);
void __stdcall SpriteSetMovieProc(uint8_t *hsl, uint32_t proc);
int __stdcall movie_call(uint8_t *s);
uint32_t __stdcall cmd_movie(uint8_t *s);

#endif
