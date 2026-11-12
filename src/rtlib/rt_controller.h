/* Engine core: the two-player controller layer (rt_controller.c) */
#ifndef DOMINION_RTLIB_RT_CONTROLLER_H
#define DOMINION_RTLIB_RT_CONTROLLER_H

#include <stdint.h>

typedef uint32_t U;
void __stdcall combo_check2(uint8_t n);
void __stdcall combo_check(U key);
U __stdcall controller_event(U slot, U code);
void __stdcall combos_set_handler(uint8_t *list, U count, U handler);
void __stdcall combos_enable(uint8_t *list, U count, U on);
U __stdcall combo_remove(U i, U count, uint8_t *list);
U __stdcall combo_insert(const uint8_t *c, uint8_t *list, U count);
U __stdcall combo_update(const uint8_t *c, uint8_t *list, U count, U max);
void __stdcall cmd_combo(const uint8_t *c);
void __stdcall cmd_controller_state(const uint8_t *c);
void __stdcall keyset_enable(U n, U on);
void __stdcall device_enable(uint8_t *b, U on);
void __stdcall release_all(uint8_t *b, U n);
void __stdcall cmd_controller_device(const uint8_t *c);
U __stdcall cmd_controller_handler(const uint8_t *c);
void __stdcall controllers_focus(U lost);
void __stdcall controller_take(U n, U exclusive);

#endif
