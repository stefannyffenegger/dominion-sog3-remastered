/* The game logic, 0x100a621e-0x100a655f: energy wells appearing, and finding a well by its
 * position (see mining.c for a well's fields; +0x15 is its appearing frame, +0x17 the time of
 * the next one, +0x1b the time between them). */
#include "iso_core.h"
#include "mining.h"
#include "radarmap.h"
#include "wells.h"

#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))

/* 0x100a621e: wells that are appearing (state 1) step through frames 1-5 when their time has
 * come; after frame 5 they show their fill frames, become active (state 2, frame 9), are
 * plotted on the radar (not in the map editor) and lose their extra sprite */
void __cdecl wells_appear_tick(void)
{
    for (int32_t k = 0; k < 0x28; k++) {
        uint8_t *w = g_iso + k * 0x2b + 0x1097;
        if (P(uint8_t, w, 0x14) != 1 || P(uint32_t, w, 0x17) >= I(uint32_t, 0x17dd))
            continue;
        if (P(uint8_t, w, 0x15) == 5) {
            uint32_t f = well_fill_frame(w, 0, 1);
            SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0));
            uint32_t first = f * 7 - 1;
            SpriteSendMessage(I(void *, 0x181e), P(void *, w, 0), 1, (uint32_t *)ORIG_PTR(0x10157334));
            MSG[1] = first;
            MSG[2] = first + 6;
            MSG[3] = 0x60;
            SpritePostMessage(I(void *, 0x181e), P(void *, w, 0), 4, MSG);
            SpritePostMessage(I(void *, 0x181e), P(void *, w, 0), 4, (uint32_t *)ORIG_PTR(0x10157338));
            P(uint8_t, w, 0x14) = 2;
            P(uint8_t, w, 0x15) = 9;
            if (I(int32_t, ISO_HAVE_SPRITES))
                radar_plot_object(w, 0, 1);
            if (P(void *, w, 0x1f)) {
                SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0x1f));
                SpriteHide(I(void *, 0x181e), P(void *, w, 0x1f), 1);
                RtDelete(I(void *, ISO_APP), P(uint32_t, w, 0x23));
                P(uint32_t, w, 0x23) = 0;
                P(void *, w, 0x1f) = 0;
            }
        } else {
            P(uint32_t, w, 0x17) = I(uint32_t, 0x17dd) + P(uint32_t, w, 0x1b);
            P(uint8_t, w, 0x15)++;
            SpriteShow(I(void *, 0x181e), P(void *, w, 0), P(uint8_t, w, 0x15), 1);
        }
    }
}

/* IsoFindActiveEnergyWell: the well at (x, y), with its state in *state; in the map editor
 * (iso+0x17f5 == 1) any well, else only placed ones (state not 0). -1 if none */
int32_t __cdecl IsoFindActiveEnergyWellC(uint8_t *iso, int32_t x, int32_t y, uint32_t *state)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x10154268), x, y, state);
    for (int32_t k = 0; k < 0x28; k++) {
        uint8_t *w = g_iso + k * 0x2b + 0x1097;
        DebugStr((const char *)ORIG_PTR(0x1015428c), (uint32_t)P(uint8_t, w, 0x14), P(int32_t, w, 4), P(int32_t, w, 8));
        if ((I(int32_t, 0x17f5) == 1 || P(uint8_t, w, 0x14)) && P(int32_t, w, 4) == x && P(int32_t, w, 8) == y) {
            *state = P(uint8_t, w, 0x14);
            return k;
        }
    }
    return -1;
}
