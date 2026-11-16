/* The game logic, 0x100147d0-0x10014d9e: where a scouting unit goes next: the corner of
 * the map (of the three other quarters) its player has seen least of, by a sector on the
 * way that it can reach. */
#include "iso_core.h"
#include "recon.h"

/* not rewritten yet (names ours, provisional) */
#define quarter_unseen ORIG(int(__cdecl *)(uint32_t, int16_t), 0x10015efc)
#define sector_reachable ORIG(int(__cdecl *)(int, uint32_t, int, uint32_t), 0x1001c62c)
#define IsoGetSectorCoordinates ORIG(void(__stdcall *)(uint8_t *, int, int *, int *), 0x1001bce0)
#define direction_to ORIG(int(__cdecl *)(int16_t, int16_t, int16_t, int16_t), 0x1009188a)

#define U_SECTOR 0x1a1
#define SECTORS_W I(uint16_t, 0xc2fd)
#define SECTORS_H I(uint16_t, 0xc2ef)

/* 0x100147d0: the sector to scout next, or -1 (mode 0; other modes were never finished
 * and give -1) */
int __cdecl recon_next_sector(uint32_t id, int16_t mode)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    int quarter = P(int16_t, u, U_X) < 1 ? (P(int16_t, u, U_Y) < 1 ? 0 : 2) : (P(int16_t, u, U_Y) < 1 ? 1 : 3);
    DebugStr((const char *)ORIG_PTR(0x10118fc4), quarter);
    if (mode)
        return -1;
    const uint8_t *others = (const uint8_t *)ORIG_PTR(0x10118fb8) + quarter * 3; /* the other three */
    uint32_t most = 0, target = 0xffffffff;
    uint8_t pick = 0;
    for (int i = 0; i < 3; i++) {
        uint32_t q = others[i];
        uint32_t left = (uint32_t)quarter_unseen(q, P(uint8_t, u, U_PLAYER)) & 0xffff;
        DebugStr((const char *)ORIG_PTR(0x10118fd4), q, left);
        if ((int)most < (int)left) {
            most = left;
            target = q;
            pick = (uint8_t)i;
            DebugStr((const char *)ORIG_PTR(0x10118ff0), q);
        }
    }
    int turned = 0;
    for (;;) {
        int s;
        switch (target) {
        case 0: s = 0; break;
        case 1: s = SECTORS_W - 1; break;
        case 2: s = (SECTORS_H - 1) * SECTORS_W; break;
        case 3: s = SECTORS_W * SECTORS_H - 1; break;
        default:
            DebugStr((const char *)ORIG_PTR(0x10119000));
            return -1;
        }
        DebugStr((const char *)ORIG_PTR(0x10119014), id & 0xffff, target, s);
        /* from the corner towards the unit until a sector it can reach */
        int found = 0, tries = 0;
        while (!found && P(int16_t, u, U_SECTOR) != s && s != -1 && tries < (SECTORS_W >> 1)) {
            found = sector_reachable(P(int16_t, u, U_X), (uint32_t)(int)P(int16_t, u, U_Y), s, id);
            if (!found) {
                int sx, sy;
                IsoGetSectorCoordinates(g_iso, s, &sx, &sy);
                int d = direction_to((int16_t)sx, (int16_t)sy, P(int16_t, u, U_X), P(int16_t, u, U_Y));
                s = P(int16_t, I(uint8_t *, 0xc2eb) + s * 0x1e, 6 + d * 2);
                tries++;
            }
        }
        if (!found && (SECTORS_W >> 1) <= tries)
            s = P(int16_t, u, U_SECTOR);
        if (P(int16_t, u, U_SECTOR) != s && s != -1)
            return s;
        /* the next of the three, then back down */
        if (pick < 2 && !turned) {
            pick++;
            target = others[pick];
            if (pick == 2)
                turned = 1;
        } else {
            if (!turned || !pick)
                return -1;
            pick--;
            target = others[pick];
        }
    }
}
