/* Engine core: sound built-ins, joystick and controller keys (rt_joystick.c) */
#ifndef DOMINION_RTLIB_RT_JOYSTICK_H
#define DOMINION_RTLIB_RT_JOYSTICK_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
U __cdecl builtin_play_midi(void);
U __cdecl builtin_play_wave(void);
U __cdecl builtin_play_wave_ex(void);
void __cdecl joystick_capture(void);
int __stdcall joy_button_slot(U buttons);
int __stdcall joy_dir4(U pos, const uint16_t *center);
int __stdcall joy_dir8(U pos, const uint16_t *center);
int __stdcall joy_axis_slot(U pos, const uint16_t *center, int base);
int __stdcall joy_slot(U msg, U buttons, U pos);
U __stdcall joy_button_handler(U buttons, int base);
U __stdcall joy_axis_handler(U pos, const uint16_t *center, int base);
void __stdcall joy_slot_remember(uint8_t slot, uint8_t player);
void __stdcall joy_controller(U msg, U buttons, U pos);
void __stdcall joy_message(U msg, U buttons, U pos);
void __cdecl joystick_release(void);
U __stdcall key_filter_down(U key);
void __stdcall key_dir_settle(uint8_t player);
U __stdcall key_filter_up(U key);
void __stdcall keys_bind(uint8_t player, uint8_t on);
void __stdcall keymap_set(uint8_t player, uint8_t which, uint8_t vk);

#endif
