/* The game logic, 0x100a7447-0x100a78fe: free unit ids and object slots, the list of units
 * on the map (iso+0xd67a, count iso+0x1b43, at most 2000) and of active objects (iso+0xd6e3,
 * count iso+0x1b47, at most 256), and scrolling to a player's start. */
#include "iso_core.h"
#include "status.h"
#include "unitlists.h"

#define g_memmove ORIG(void *(__cdecl *)(void *, const void *, size_t), 0x100f5bd0)

/* 0x100a7447: a free unit id (1-2000), searching on from 250 past the last one used
 * (iso+0xd67e); 0 if none */
uint32_t __cdecl unit_id_free(void)
{
    uint16_t id = I(int32_t, 0xd67e) ? (uint16_t)((I(int32_t, 0xd67e) + 0xfa) % 2000) : 1;
    for (uint16_t n = 0; n < 2000; n++) {
        if (id && !I(uint8_t **, ISO_UNITS)[id])
            return id;
        id = id < 2000 ? (uint16_t)(id + 1) : 1;
    }
    return 0;
}

/* 0x100a751c: the first free object slot (iso+0xd6db, 0x44 bytes each); 0 also when none */
uint32_t __cdecl object_slot_free(void)
{
    for (uint16_t k = 0; k < 0x100; k++)
        if (!P(int32_t, I(uint8_t *, 0xd6db), k * 0x44))
            return k;
    return 0;
}

static int list_add(uint32_t list, uint32_t count, int32_t max, uint32_t id)
{
    if (I(int32_t, count) >= max)
        return 0;
    I(uint32_t *, list)[I(int32_t, count)] = id & 0xffff;
    I(int32_t, count)++;
    return 1;
}

static int list_remove(uint32_t list, uint32_t count, uint32_t id)
{
    int32_t k = 0;
    int found = 0;
    while (k < I(int32_t, count) && !found)
        found = I(uint32_t *, list)[k++] == (id & 0xffff);
    if (found) {
        k--;
        /* moves the rest down, including the entry after the last */
        g_memmove(I(uint32_t *, list) + k, I(uint32_t *, list) + k + 1, (size_t)(I(int32_t, count) - k) * 4);
        I(int32_t, count)--;
        I(int32_t *, list)[I(int32_t, count)] = -1;
    }
    return found;
}

/* 0x100a7587, 0x100a75e6: adds a unit to / takes it out of the units on the map */
int32_t __cdecl unit_list_add(uint32_t id) { return list_add(ISO_UNIT_LIST, ISO_UNIT_COUNT, 2000, id); }
int32_t __cdecl unit_list_remove(uint32_t id) { return list_remove(ISO_UNIT_LIST, ISO_UNIT_COUNT, id); }

/* 0x100a76e3, 0x100a7742: adds an object to / takes it out of the active objects */
int32_t __cdecl object_list_add(uint32_t id) { return list_add(0xd6e3, 0x1b47, 0x100, id); }
int32_t __cdecl object_list_remove(uint32_t id) { return list_remove(0xd6e3, 0x1b47, id); }

/* 0x100a783d: scrolls the view to the first start marker (iso+0x192c, 0x12 bytes each:
 * +0 in use, +4/+6 its position, +0xa a mask of players) of the player. 0 if it has none */
int32_t __cdecl view_to_player_start(int32_t player)
{
    for (int32_t k = 0; k < 0x800; k++) {
        uint8_t *m = I(uint8_t *, 0x192c) + k * 0x12;
        if (P(int32_t, m, 0) > 0 && (G(uint32_t, 0x10157250 + player * 4) & P(uint16_t, m, 0xa))) {
            view_scroll_to(P(int16_t, m, 4), P(int16_t, m, 6));
            return 1;
        }
    }
    return 0;
}
