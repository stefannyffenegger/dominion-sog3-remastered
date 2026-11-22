/* The game logic, 0x100e2df0-0x100e2f68: a computer player's flags and its 9 lists. */
#include "iso_core.h"
#include "ailists.h"

#define PLAYER(k) (G(uint8_t *, 0x1018b604) + (k) * 0x143 + 0xc)
#define LIST_FIRST(k, i) P(int32_t, PLAYER(k), 0x16 + (i) * 4)
#define LIST_LAST(k, i) P(int32_t, PLAYER(k), 0x3a + (i) * 4)

/* 0x100e2df0: whether player `k` is set up and run by the computer */
int32_t __cdecl player_is_ai(int32_t k)
{
    if (!G(uint8_t *, 0x1018b604))
        return 0;
    return P(int32_t, PLAYER(k), 2) && P(int32_t, PLAYER(k), 6);
}

/* 0x100e2e62: the last record of list `i` */
int32_t __cdecl ai_list_last(int32_t k, int32_t i)
{
    return LIST_LAST(k, i);
}

/* 0x100e2e92: sets it */
int32_t __cdecl ai_list_set_last(int32_t k, int32_t i, int32_t v)
{
    LIST_LAST(k, i) = v;
    return LIST_LAST(k, i);
}

/* 0x100e2ee5: the first record of list `i` */
int32_t __cdecl ai_list_first(int32_t k, int32_t i)
{
    return LIST_FIRST(k, i);
}

/* 0x100e2f15: sets it */
int32_t __cdecl ai_list_set_first(int32_t k, int32_t i, int32_t v)
{
    LIST_FIRST(k, i) = v;
    return LIST_FIRST(k, i);
}
