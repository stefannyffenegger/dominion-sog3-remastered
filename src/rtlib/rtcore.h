/* The engine core ("Rt"): its state block and the helpers shared by the rt_*.c files.
 *
 * The core keeps everything in one packed block of about 38 KB that the game allocates
 * (RtStartup) and passes to every Rt* export; a global points at the current one. Script
 * commands arrive as packed records: a 16-bit command number, then fields at odd offsets. */
#ifndef DOMINION_RTLIB_RTCORE_H
#define DOMINION_RTLIB_RTCORE_H

#include "orig.h"

#define g_rt G(uint8_t *, 0x1007cee8)
#define R(type, off) (*(type *)(g_rt + (off)))
#define P(type, p, off) (*(type *)((uint8_t *)(p) + (off)))

/* state fields */
#define RT_DC 0x0cb8
#define RT_RES 0x0cc0          /* the open resource file */
#define RT_HSL 0x0cc8          /* the sprite engine's state */
#define RT_COMPOSE_SPRITE 0x0cd4
#define RT_HWND 0x0cdc
#define RT_BACKGROUND 0x3247   /* background bitmap */
#define RT_SCROLL_DIB 0x324b   /* bitmap behind a composed background */
#define RT_TILE_MAP 0x32a7
#define RT_PALETTE 0x32af      /* 256 colors, 4 bytes each */
#define RT_BKG_COLOR 0x3dc3
#define RT_BKG_H 0x3dc7
#define RT_BKG_W 0x3dcb
#define RT_SCREEN_W 0x3de3
#define RT_DEPTH 0x3de7
#define RT_SCREEN_H 0x3e0f
#define RT_8D77 0x8d77
#define RT_95CC 0x95cc
#define RT_COMPOSE 0x95d1      /* bytes from here on */
#define RT_BKG_IS_COLOR 0x95d5
#define RT_95DB 0x95db
#define RT_95E7 0x95e7
#define RT_PALETTE_DEVICE 0x95ed
#define RT_SCROLLABLE 0x95f8
#define RT_95F9 0x95f9
#define RT_BKG_SCROLLS 0x9600

/* objects: see rt_obj.h */
#define obj_12000 obj_check_type
#define obj_14409 obj_loaded
#define rt_message_box ORIG(void(__stdcall *)(HWND, uint32_t, uint32_t), 0x10025da8)

#endif
