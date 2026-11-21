/* The game logic, 0x100d2e41-0x100d3c1e: building and showing the popup menu. */
#include "iso_core.h"
#include "playerinfo.h"
#include "popupshow.h"

#define STR(a) ((const char *)ORIG_PTR(a))
#define dib_merge ORIG(void(__stdcall *)(uint8_t *, uint8_t *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t), 0x100f49d0)
#define blast_text ORIG(void(__cdecl *)(uint32_t, uint8_t *, uint32_t, const void *, int32_t, int32_t, const char *, int32_t), 0x101120b6)
#define overlay_register ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x100f4ade)
#define sprite_position ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100f496a)
#define sprite_show ORIG(void(__stdcall *)(uint32_t, uint32_t, uint32_t, int32_t), 0x100f48e0)
#define g_strncpy ORIG(char *(__cdecl *)(char *, const char *, size_t), 0x100f5430)
#define g_strlen ORIG(size_t(__cdecl *)(const char *), 0x100f5750)
#define g_strpbrk ORIG(char *(__cdecl *)(char *, const char *), 0x100f7ec0)
#define g_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define g_memset ORIG(void *(__cdecl *)(void *, int, size_t), 0x100f4c10)
#define unit_cost ORIG(void(__cdecl *)(uint32_t, int32_t *, int32_t *), 0x100a5423)
#define item_draw ORIG(void(__cdecl *)(int32_t, uint8_t *, int32_t, int32_t, int32_t, int32_t, const char *), 0x100d42ea)
#define HAS(m, i) (((m) >> (i) & 1) != 0)
#define RECT_OF(i) (&I(int32_t, 0xdb08 + (i) * 16))
#define DIB(off) I(uint8_t *, off)
#define NAMES(k) ((char *)ORIG_PTR(0x101804c0) + (k) * 0x16)
#define SELECTED I(int32_t, 0x1b23 + I(int32_t, 0x1b65) * 4)

static void merge_at(uint8_t *dib, uint8_t *src, int32_t y)
{
    dib_merge(dib, src, P(int32_t, src, 4), P(int32_t, src, 0xc), 0, y, 0, 0);
}

static void set_rect(int32_t i, uint8_t *src, int32_t x, int32_t py, int32_t y)
{
    int32_t *r = RECT_OF(i);
    r[0] = x;
    r[1] = y + py;
    r[2] = P(int32_t, src, 4) + x;
    r[3] = y + py + P(int32_t, src, 0xc);
}

/* 0x100d2e41: builds the menu for the items in `mask` (`enabled` of them usable, `checked`
 * ticked) into its DIB, unit `id` or none: the title, the orders (0-0x1f), then after a
 * divider the transfers to allies (0x20-0x2f, "%d to %s", with the upgrade and sell prices
 * at items 0x12 and 0x10), the toggles (0x30-0x3f) and the health of a single unit. It is
 * moved to stay on screen and shown. */
void __stdcall IsoShowPopupMenuC(uint8_t *iso, uint64_t mask, uint64_t enabled, uint64_t checked, int32_t px,
                                 int32_t py, uint32_t id)
{
    g_iso = iso;
    uint8_t *u = id & 0xffff ? I(uint8_t **, ISO_UNITS)[id & 0xffff] : 0;
    int32_t y = 0, items = 0, toggles = 0, divided = 0;
    char buf[0x40], hp[0x44];
    if (u && (P(uint32_t, u, 0x3e) >> 25 & 1))
        enabled = 0;
    I(uint64_t, 0xe008) = mask;
    I(uint64_t, 0xe010) = enabled;
    I(uint64_t, 0xe018) = checked;
    I(int32_t, 0xdaf4) = -1;
    I(int32_t, 0xdaf8) = px;
    I(int32_t, 0xdb00) = P(int32_t, DIB(0xdae8), 4) + px;
    I(int32_t, 0xdafc) = py;

    uint8_t *dib = DIB(0xdad0), *src = DIB(0xdad4);
    g_memset(P(void *, dib, 0x20), 0, P(int32_t, dib, 8) * P(int32_t, dib, 0xc));
    merge_at(dib, src, y);
    if (SELECTED > 1)
        g_strncpy(buf, STR(0x101591bc), 0x40);
    else if (u && P(int32_t, u, 0x10) == 0x6c)
        g_strncpy(buf, STR(0x101591cc), 0x40);
    else if (u)
        g_strncpy(buf, STR(0x1013bee8 + P(uint8_t, u, 0x16) * 0x520 + (P(int32_t, u, 0x10) & ~3) * 8), 0x40);
    else
        g_strncpy(buf, STR(I(int32_t, 0x175) ? 0x101591d8 : 0x101591e8), 0x40);
    char *title = buf;
    while (g_strlen(title) > 0x10)
        title = g_strpbrk(title + 1, STR(0x101591f8));
    if (*title == ' ')
        title++;
    blast_text(I(uint32_t, 0x1812), dib, I(uint32_t, 0xdaec), 0, -1, y + 5, title, 1);
    y += P(int32_t, src, 0xc);

    src = DIB(0xdad8);
    for (int32_t i = 0; i < 0x30; i++) {
        if (i >= 0x20 && HAS(mask, i) && !divided && items) {
            src = DIB(0xdae8);
            merge_at(dib, src, y);
            y += P(int32_t, src, 0xc);
            divided = 1;
            src = DIB(0xdad8);
        }
        g_memset(RECT_OF(i), 0, 16);
        if (!HAS(mask, i))
            continue;
        set_rect(i, src, px, py, y);
        merge_at(dib, src, y);
        I(int32_t, 0xdf08 + i * 4) = y;
        if (u && i >= 0x20) {
            const char *name = (const char *)g_iso + 0x1b6d + (i - 0x20) * 0x1461;
            if (P(int32_t, u, 0x10) == 0x5c)
                wsprintfA(buf, STR(0x101591fc), 0x3e8, name);
            else
                wsprintfA(buf, STR(0x10159208), 0x32, name);
            buf[0x15] = 0;
            g_strcpy(NAMES(i - 0x20), buf);
            item_draw(i, dib, y, HAS(enabled, i), 0, 0, buf);
        } else if (u && (i == 0x10 || i == 0x12)) {
            int32_t a, b;
            if (i == 0x12) {
                int32_t k = P(int32_t, I(uint8_t *, 0x18ec), 0x2c) * P(int32_t, u, 0xc);
                uint8_t *t = P(uint8_t *, I(uint8_t *, 0x18ec), 0);
                a = P(int32_t, t, k * 4 + 0x28);
                b = P(int32_t, t, k * 4 + 0x2c);
                wsprintfA(buf, STR(0x10159214), a, b);
            } else {
                unit_cost(id & 0xffff, &a, &b);
                wsprintfA(buf, STR(0x10159224), a, b);
            }
            g_strcpy(NAMES(i - 8), buf);
            item_draw(i, dib, y, HAS(enabled, i), 0, 0, buf);
        } else {
            item_draw(i, dib, y, HAS(enabled, i), 0, 0, 0);
        }
        y += P(int32_t, src, 0xc);
        items++;
    }

    for (int32_t i = 0x30; i < 0x40; i++) {
        g_memset(RECT_OF(i), 0, 16);
        if (!HAS(mask, i))
            continue;
        if (items && !toggles) {
            src = DIB(0xdae8);
            merge_at(dib, src, y);
            y += P(int32_t, src, 0xc);
        }
        int32_t on = HAS(checked, i);
        src = on ? DIB(0xdadc) : DIB(0xdae0);
        set_rect(i, src, px, py, y);
        merge_at(dib, src, y);
        I(int32_t, 0xdf08 + i * 4) = y;
        item_draw(i, dib, y, HAS(enabled, i), on, 0, 0);
        y += P(int32_t, src, 0xc);
        toggles++;
    }

    if (u && SELECTED == 1) {
        src = DIB(0xdae8);
        merge_at(dib, src, y);
        y += P(int32_t, src, 0xc);
        src = DIB(0xdad8);
        merge_at(dib, src, y);
        int16_t max = (int16_t)unit_max_health(u, 0), now = P(int16_t, u, 0xd6);
        wsprintfA(hp, STR(0x10159230), (int32_t)now, (int32_t)max);
        const void *colour = ORIG_PTR(now < max >> 1 ? 0x101418d8 : now < max ? 0x10142cd8 : 0x101428d8);
        blast_text(I(uint32_t, 0x1812), dib, I(uint32_t, 0xdaec), colour, -1, y + 2, hp, 1);
        y += P(int32_t, src, 0xc);
    }

    src = DIB(0xdae4);
    merge_at(dib, src, y);
    y += P(int32_t, src, 0xc);
    I(int32_t, 0xdb04) = y + py;

    int32_t w = P(int32_t, src, 8);
    int32_t nx = I(int32_t, 0xd6bf) - 0x80 < px + w ? px - w : px;
    int32_t ny = I(int32_t, 0xd6c3) - 0x80 < y + py ? py - y : py;
    if (I(int32_t, 0xd6bb) + 0x80 > ny)
        ny = I(int32_t, 0xd6bb) + 0x80;
    int32_t dx = nx - px, dy = ny - py;
    if (dx || dy) {
        for (int32_t i = 0; i < 0x40; i++) {
            if (!HAS(mask, i))
                continue;
            int32_t *r = RECT_OF(i);
            r[0] += dx;
            r[2] += dx;
            r[1] += dy;
            r[3] += dy;
        }
        I(int32_t, 0xdaf8) += dx;
        I(int32_t, 0xdb00) += dx;
        I(int32_t, 0xdafc) += dy;
        I(int32_t, 0xdb04) += dy;
    }
    overlay_register(I(uint32_t, 0x181e), I(uint32_t, 0xdab6));
    nx += P(uint32_t, dib, 4) >> 1;
    ny += P(uint32_t, dib, 0xc) >> 1;
    sprite_position(I(uint32_t, 0x181e), I(uint32_t, 0xdab6), nx, ny, 0);
    sprite_show(I(uint32_t, 0x181e), I(uint32_t, 0xdab6), 0, 1);
    I(int32_t, 0xdaf0) = 1;
}
