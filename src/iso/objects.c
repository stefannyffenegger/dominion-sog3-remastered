/* The game logic: IsoAddObject (0x100aca6e-0x100ad1d9), placing a map object (iso+0x192c,
 * 0x800 slots of 0x12 bytes: +0 its sprite object, +4/+6 its position, +8 its type, +0xa its
 * flags, +0xc its slot). Object types come from a table (iso+0x1930: +0 the resource, +4 the
 * frames, +8 the frame delay). */
#include "iso_core.h"
#include "objects.h"

#define MSG ((uint32_t *)ORIG_PTR(0x101572c0))
#define OBJ(k) (I(uint8_t *, 0x192c) + (k) * 0x12)
#define TYPE(t, off) P(int32_t, P(uint8_t *, I(uint8_t *, 0x1930), 0), (off) + P(int32_t, I(uint8_t *, 0x1930), 0x2c) * (t) * 4)
#define IsoDeleteObject ORIG(int32_t(__stdcall *)(uint8_t *, uint32_t), 0x100adbde)
#define object_cells_mark ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x100ad5f2)
#define object_radar ORIG(void(__cdecl *)(uint32_t, int), 0x100b2623)
#define IsoSetBlitProc ORIG(void(__cdecl *)(uint8_t *, void *, int), 0x100944b2)

/* IsoAddObject: the slot, or -1. A beacon (0x122, 0x123) at a place that has one returns
 * that one; a 0x123 replaces a 0x122 there. Objects 0xb4-0x103 use the map's variant
 * (iso+0xc340); 0xaa-0xb1 are placed by a footprint offset of `kind`; 0x10b gets a 0x118
 * above it. In a network game the objects are shown only once the game runs */
int32_t __stdcall IsoAddObjectC(uint8_t *iso, int32_t type, int32_t x, int32_t y, uint32_t flags, int32_t kind,
                                int32_t level)
{
    g_iso = iso;
    int32_t px = x + 0x20, py = y + 0x10;
    uint32_t k;
    if (type == 0x123 || type == 0x122)
        for (k = 0; (int32_t)k < 0x800; k++) {
            uint16_t t = P(uint16_t, OBJ(k), 8);
            if ((t == 0x123 || t == 0x122) && P(int16_t, OBJ(k), 4) == px && P(int16_t, OBJ(k), 6) == py) {
                if (type != 0x123 || P(uint16_t, OBJ(k), 8) != 0x122)
                    return (int32_t)k;
                IsoDeleteObject(iso, k);
                break;
            }
        }
    for (k = 0; (int32_t)k < 0x800 && P(int32_t, OBJ(k), 0); k++)
        ;
    if ((int32_t)k >= 0x800) {
        DebugStr((const char *)ORIG_PTR(0x10157b34));
        return -1;
    }
    int32_t res = TYPE(type, 0);
    if (res < 1)
        return -1;
    if (type > 0xb3 && type < 0x104)
        res += I(uint16_t, 0xc340) * 2;
    uint32_t obj = RtCopy(I(void *, ISO_APP), (unsigned)res);
    RtLoad(I(void *, ISO_APP), obj);
    uint8_t *s = ((uint8_t **)RtGetResource(I(void *, ISO_APP), obj))[1];
    if (!s || RtGetResType(I(void *, ISO_APP), obj) != 0x14)
        return -1;
    I(uint32_t, 0x1858) = obj;
    if (flags & 2) {
        G(uint32_t, 0x1015732c) = 1;
        SpriteSendMessage(I(void *, 0x181e), s, 3, (uint32_t *)ORIG_PTR(0x10157328));
    }
    int32_t lv = level != -1 ? level : (flags & 8) ? -30000 : py + 0x2710;
    SpriteSetLevel(I(void *, 0x181e), s, lv);
    SpriteSetPosition(I(void *, 0x181e), s, px, py, 0);
    if (type > 0xa9 && type < 0xb2 && kind != -1) {
        uint8_t *fp = I(uint8_t *, 0x1980);
        px += P(int32_t, P(uint8_t *, fp, 0), P(int32_t, fp, 0x2c) * (kind - 0x40) * 4);
        py += P(int32_t, P(uint8_t *, fp, 0), 4 + P(int32_t, fp, 0x2c) * (kind - 0x40) * 4);
        SpriteSetPosition(I(void *, 0x181e), s, px, py, 0);
        SpriteSetLevel(I(void *, 0x181e), s, P(int32_t, s, 0x40) + 0x2711);
    }
    if (!I(int32_t, 0x175) || I(uint8_t, 0xd90f)) {
        if (type < 0x6f || type > 0x77 || !I(int32_t, ISO_HAVE_SPRITES)) {
            if (!(I(uint8_t, 0xeea) & 8)) {
                if (type < 0x104 || type > 0x107 || I(int32_t, ISO_HAVE_SPRITES))
                    SpriteShow(I(void *, 0x181e), s, 0, 1);
                else
                    SpriteShow(I(void *, 0x181e), s, SpriteGetNumCels(s) - 1, 1);
            } else {
                G(uint32_t, 0x1015728c) = 0;
                G(uint32_t, 0x10157290) = 1;
                SpriteSendMessage(I(void *, 0x181e), s, 5, (uint32_t *)ORIG_PTR(0x10157288));
            }
        }
        if ((flags & 4) && I(int32_t, 0x11c)) {
            /* an animated object loops through its frames */
            SpriteSendMessage(I(void *, 0x181e), s, 1, (uint32_t *)ORIG_PTR(0x10157334));
            MSG[1] = 0;
            MSG[2] = (uint32_t)(TYPE(type, 4) - 1);
            MSG[3] = (uint32_t)TYPE(type, 8);
            SpritePostMessage(I(void *, 0x181e), s, 4, MSG);
            SpritePostMessage(I(void *, 0x181e), s, 4, (uint32_t *)ORIG_PTR(0x10157338));
        }
    }
    I(int32_t, 0x1b4d)++;
    P(uint32_t, OBJ(k), 0) = obj;
    P(int16_t, OBJ(k), 4) = (int16_t)px;
    P(int16_t, OBJ(k), 6) = (int16_t)py;
    P(int16_t, OBJ(k), 8) = (int16_t)type;
    P(int16_t, OBJ(k), 0xa) = (int16_t)flags;
    P(int16_t, OBJ(k), 0xc) = (int16_t)k;
    object_cells_mark(iso, k);
    if (I(int32_t, ISO_HAVE_SPRITES) && !(flags & 4))
        SpriteSetProcessMsgs(I(void *, 0x181e), s, 0);
    object_radar(k, 1);
    if (type == 0x10b)
        IsoAddObjectC(iso, 0x118, x + 4, y - 0x44, flags, -1, P(int32_t, s, 0x98) + 1);
    if (type == 0x118 && I(int32_t, 0x13c))
        IsoSetBlitProc(g_iso, s, 0x1f9);
    return (int32_t)k;
}
