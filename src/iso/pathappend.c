/* The game logic, 0x100d4e05-0x100d4f51: adding a list of path nodes to a unit's path. */
#include "iso_core.h"
#include "pathappend.h"
#include "pathlink.h"

#define NEXT(k) P(int16_t, I(uint8_t *, 0x1984) + (k) * 0x12, 0xe)

/* 0x100d4e05: appends the node list `list` to the end of unit `id`'s second path (+0x119).
 * The list's last node takes over what the path's last node led to. Returns 0 without a unit */
int32_t __cdecl path_append(uint32_t id, int32_t list)
{
    uint8_t *u = I(uint8_t **, ISO_UNITS)[id & 0xffff];
    if (!u)
        return 0;
    int32_t tail = list;
    while (NEXT(tail) > 0)
        tail = NEXT(tail);
    if (P(int16_t, u, 0x119) > 0) {
        int32_t last = P(int16_t, u, 0x119);
        while (NEXT(last) > 0)
            last = NEXT(last);
        vector_link(tail, NEXT(last), 1);
        vector_link(last, list, 1);
    } else {
        vector_link(tail, P(int16_t, u, 0x119), 1);
        P(int16_t, u, 0x119) = (int16_t)list;
    }
    return 1;
}
