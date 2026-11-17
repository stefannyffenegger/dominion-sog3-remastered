/* The game logic's state: one big block the engine hands to every Iso* export (the
 * global at 0x1017c584 points at it), plus the shared helpers of the iso_*.c files.
 * Fields are at the original's (unaligned) offsets. */
#ifndef DOMINION_ISO_CORE_H
#define DOMINION_ISO_CORE_H

#include "orig.h"
#include "rtlib32.h"

#define g_iso G(uint8_t *, 0x1017c584)
#define I(type, off) (*(type *)(g_iso + (off)))
#define P(type, p, off) (*(type *)((uint8_t *)(p) + (off)))

/* state fields */
#define ISO_HAVE_SPRITES 0x0154   /* sprites are shown: 0 in the map editor */
#define ISO_FLAGS 0x0174           /* byte: power hints given (1 low, 8 none, 0x10 back); 0x80 scrolled by keys */
#define ISO_APP 0x180e            /* the engine's app (rt) */
#define ISO_HSL 0x181e            /* the engine's sprite state */
#define ISO_MAP 0x1904            /* map cells: 32 bits, row y at (y << ISO_MAP_SHIFT) */
#define ISO_MAP_H 0x1a98
#define ISO_MAP_W 0x1a9c
#define ISO_MAP_SHIFT 0x1aa0
#define ISO_UNIT_COUNT 0x1b43     /* entries in the unit list */
#define ISO_LOCAL_PLAYER 0x1b65
#define ISO_PLAYERS 0x1b69        /* per player, 0x1461 bytes */
#define ISO_PLAYER_SIZE 0x1461
#define ISO_UNITS 0xd656          /* unit pointers by id */
#define ISO_UNIT_LIST 0xd67a      /* ids of the units in play (low 16 bits) */

/* a unit */
#define U_ID_TYPE 0x0c            /* its type is this >> 2 */
#define U_CLASS 0x10
#define U_PLAYER 0x14             /* byte */
#define U_FLAGS 0x3e
#define U_POWER 0x44              /* short: its power grid, or negative */
#define U_POWER_ICON 0x4a         /* short: its "no power" icon, or -1 */
#define U_X 0x52                  /* short, in pixels */
#define U_Y 0x54

#define CLASS_ENERGY_WELL 0x50
#define CLASS_RELAY 0x84

static inline uint8_t *iso_unit(int id) { return I(uint8_t **, ISO_UNITS)[id]; }
static inline uint32_t *iso_cell(int x, int y)
{
    return &I(uint32_t *, ISO_MAP)[((uint32_t)y << (I(uint8_t, ISO_MAP_SHIFT) & 0x1f)) + (uint32_t)x];
}
static inline int iso_on_map(int x, int y)
{
    return x >= 0 && x < I(int, ISO_MAP_W) && y >= 0 && y < I(int, ISO_MAP_H);
}

/* the unit type table: the value at `off` for type t (0x20 power given, 0x24 used) */
static inline int32_t type_value(int32_t type, int off)
{
    int32_t *t = I(int32_t *, 0x18ec);
    return P(int32_t, (uintptr_t)t[0], off + t[0xb] * type * 4);
}
#define TYPE_GIVES 0x20
#define TYPE_USES 0x24

/* per unit type: a handler run when the unit is drawn or deselected (17 entries a type) */
typedef void(__cdecl *UnitHandler)(uint32_t id);
#define unit_type_handler(type) G(UnitHandler, 0x101545c0 + (type) * 0x44)

/* each player's selected units */
#define ISO_SELECTED_N 0x1b23 /* per player: how many */
#define ISO_SELECTED 0x193c   /* per player: their ids (16 bits) */
#define SELECTED_N(p) I(int32_t, ISO_SELECTED_N + (p) * 4)
static inline uint16_t *selected(uint32_t player) { return I(uint16_t *, ISO_SELECTED + player * 4); }
#define U_SEL_STATE 0x2d /* byte */
#define U_SELECTED 0x2e  /* byte */
#define U_GROUP 0x15     /* byte: its numbered group */

/* data of the original */
#define g_power_radius ((int16_t *)ORIG_PTR(0x10118798))  /* per unit type */
#define g_radius_masks ((const char **)ORIG_PTR(0x10118038)) /* per radius, (2r+1)^2 bytes */
/* steps on the isometric grid: (dx, dy) pairs for even and odd rows */
#define g_step_ne ((const int8_t *)ORIG_PTR(0x1013b46c))
#define g_step_dirs ((const int8_t *)ORIG_PTR(0x1013b468))    /* 8 directions x 2 parities */
#define g_step_n ((const int8_t *)ORIG_PTR(0x1013b478))
#define g_step_se ((const int8_t *)ORIG_PTR(0x1013b484))
#define g_msg_hide ((const uint32_t *)ORIG_PTR(0x10157324)) /* sprite message */

/* original functions not rewritten yet. Check each one's convention (work/tmp/iso/
 * retinfo.py): the Iso* exports are not all cdecl (these three are stdcall). */
/* pixel position to cell (in place); returns the cell index */
#define IsoVTilePos ORIG(int32_t(__stdcall *)(uint8_t *, int *, int *, int *, int *), 0x100aed6a)
void __stdcall IsoRefreshScreen(uint8_t *iso); /* screen.c */
int __stdcall IsoResetTileSet(uint8_t *iso);    /* maptiles.c */

#endif
