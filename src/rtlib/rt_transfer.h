/* Engine core: sprite transfer (rt_transfer.c) */
#ifndef DOMINION_RTLIB_RT_TRANSFER_H
#define DOMINION_RTLIB_RT_TRANSFER_H

#include <stdint.h>
#include <windows.h>

typedef uint32_t U;
void __stdcall res_unref(U id);
void __stdcall sprite_extra_release(U id);
void __stdcall sprite_images_keep(U id, const uint8_t *info);
void __stdcall sprite_free_old(U id, const uint8_t *info);
U __stdcall sprite_images_take(U id, const uint8_t *info);
U __stdcall sprite_swap(U from, U to, U destroy);
U __stdcall sprite_transfer(U from, U to, U queue, U destroy);

#endif
