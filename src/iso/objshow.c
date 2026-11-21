/* The game logic: IsoInitObjectDestructionInfo (0x100ad985) and IsoShowObjects (0x100ae0ae),
 * showing or hiding all map objects (when the game starts, after it was paused). */
#include "iso_core.h"
#include "objshow.h"

#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))
#define OBJ(k) (I(uint8_t *, 0x192c) + (k) * 0x12)
#define TYPE(t, off) P(int32_t, P(uint8_t *, I(uint8_t *, 0x1930), 0), (off) + P(int32_t, I(uint8_t *, 0x1930), 0x2c) * (t) * 4)

/* IsoInitObjectDestructionInfo: the objects' destruction table (iso+0xd9e2) */
void __stdcall IsoInitObjectDestructionInfoC(uint8_t *iso, uint32_t res)
{
    g_iso = iso;
    if (RtLoad(I(void *, ISO_APP), res))
        I(uint32_t, 0xd9e2) = *(uint32_t *)RtGetResource(I(void *, ISO_APP), res);
}

/* IsoShowObjects: hides all objects, or shows them as IsoAddObject does (restarting the
 * animated ones), and the appearing wells and their glows */
int32_t __stdcall IsoShowObjectsC(uint8_t *iso, int32_t show)
{
    g_iso = iso;
    if (I(uint8_t *, 0x192c))
        for (int k = 0; k < 0x800; k++) {
            int32_t res = P(int32_t, OBJ(k), 0);
            if (res <= 0)
                continue;
            uint8_t *s = ((uint8_t **)RtGetResource(I(void *, ISO_APP), (uint32_t)res))[1];
            if (!s)
                continue;
            if (!show) {
                SpriteHide(I(void *, 0x181e), s, 1);
                continue;
            }
            uint16_t type = P(uint16_t, OBJ(k), 8), flags = P(uint16_t, OBJ(k), 0xa);
            if (type < 0x6f || type > 0x77 || !I(int32_t, ISO_HAVE_SPRITES)) {
                if (!(I(uint8_t, 0xeea) & 8)) {
                    if (type < 0x104 || type > 0x107 || I(int32_t, ISO_HAVE_SPRITES))
                        SpriteShow(I(void *, 0x181e), s, P(uint32_t, s, 0x74), 1);
                    else
                        SpriteShow(I(void *, 0x181e), s, SpriteGetNumCels(s) - 1, 1);
                } else {
                    G(uint32_t, 0x1015728c) = 0;
                    G(uint32_t, 0x10157290) = 1;
                    SpriteSendMessage(I(void *, 0x181e), s, 5, (uint32_t *)ORIG_PTR(0x10157288));
                }
            }
            if ((flags & 4) && I(int32_t, 0x11c)) {
                SpriteEmptyQueue(I(void *, 0x181e), s);
                SpriteHide(I(void *, 0x181e), s, 1);
                SpriteSendMessage(I(void *, 0x181e), s, 1, (uint32_t *)ORIG_PTR(0x10157334));
                MSG[1] = 0;
                MSG[2] = (uint32_t)(TYPE(type, 4) - 1);
                MSG[3] = (uint32_t)TYPE(type, 8);
                SpritePostMessage(I(void *, 0x181e), s, 4, MSG);
                SpritePostMessage(I(void *, 0x181e), s, 4, (uint32_t *)ORIG_PTR(0x10157338));
            }
        }
    if (show)
        for (int n = 0; n < 0x28; n++) {
            uint8_t *w = g_iso + n * 0x2b + 0x1097;
            if (P(uint8_t, w, 0x14) != 1)
                continue;
            SpriteShow(I(void *, 0x181e), P(void *, w, 0), P(uint8_t, w, 0x15), 1);
            if (!P(void *, w, 0x1f))
                continue;
            SpriteEmptyQueue(I(void *, 0x181e), P(void *, w, 0x1f));
            SpriteHide(I(void *, 0x181e), P(void *, w, 0x1f), 1);
            SpriteSendMessage(I(void *, 0x181e), P(void *, w, 0x1f), 1, (uint32_t *)ORIG_PTR(0x10157334));
            MSG[1] = 0;
            MSG[2] = SpriteGetNumCels(P(void *, w, 0x1f));
            MSG[2] = MSG[2] - 1;
            MSG[3] = 0x60;
            SpritePostMessage(I(void *, 0x181e), P(void *, w, 0x1f), 4, MSG);
            SpritePostMessage(I(void *, 0x181e), P(void *, w, 0x1f), 4, (uint32_t *)ORIG_PTR(0x10157338));
        }
    return 1;
}
