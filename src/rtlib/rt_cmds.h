/* Engine core: script command handlers (rt_cmds.c) */
#ifndef DOMINION_RTLIB_RT_CMDS_H
#define DOMINION_RTLIB_RT_CMDS_H

#include <stdint.h>

void __stdcall cmd_break_loops(const uint8_t *c);
void __stdcall cmd_comp_to_background(const uint8_t *c);
uint32_t __stdcall cmd_collide(uint8_t *c);
void __stdcall cmd_debug_string(const uint8_t *c);
void __stdcall cmd_new_scene(uint8_t *c);
void __stdcall cmd_get_bounds(const uint8_t *c);
void __stdcall cmd_get_bitmap_ids(const uint8_t *c);
void __stdcall cmd_get_date(const uint8_t *c);
void __stdcall cmd_get_position(const uint8_t *c);
void __stdcall cmd_get_time(const uint8_t *c);
int __stdcall cmd_if(const uint8_t *c, uint32_t *next);
uint32_t __stdcall cond_test(uint8_t op, uint32_t a, uint32_t b);
uint32_t __stdcall cmd_if_goto(const uint8_t *c);
uint32_t __stdcall cmd_if_expr(const uint8_t *c);
void __stdcall cmd_key_state(const uint8_t *c);
void __stdcall cmd_set_level(const uint8_t *c);
void __stdcall cmd_offset(const uint8_t *c);
void __stdcall cmd_cd_track(uint8_t track);
void __stdcall cmd_play_midi(uint8_t *c);
uint32_t __stdcall cmd_play_wave(uint32_t id, int32_t *params);
void __stdcall cmd_random(const uint8_t *c);

#endif
