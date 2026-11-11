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

int __stdcall cmd_random_script(uint8_t *c);
void __stdcall cmd_save_resource(const uint8_t *c);
uint32_t __stdcall cmd_set_position(const uint8_t *c);
int __stdcall format_arg_count(const char *fmt);
void __stdcall cmd_format(const uint8_t *c);
uint32_t __stdcall math_step(const uint8_t *e, uint32_t acc);
void __stdcall cmd_math(const uint8_t *c);
uint32_t __stdcall volume_step(uint32_t vol, int up);
void __stdcall cmd_volume(const uint8_t *c);
int __stdcall cmd_sequence(uint8_t *c);
uint32_t __stdcall cmd_set_queue(uint32_t id, uint32_t queue, int hide);
void __stdcall cmd_stop_midi(uint32_t id);
void __stdcall cmd_strcmp(const uint8_t *c);
uint32_t __stdcall cmd_list_entry(uint8_t *c);
uint32_t __stdcall cmd_switch(const uint8_t *c);
uint32_t __stdcall cmd_switch_expr(const uint8_t *c);
void __stdcall cmd_is_visible(const uint8_t *c);
uint32_t __stdcall cmd_if_expr2(const uint8_t *c);

#endif
