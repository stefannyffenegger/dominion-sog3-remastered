/* The game logic, 0x100d3c1e-0x100d4450: the pointer over the popup menu, hiding it, and
 * drawing one of its items. */
#include "iso_core.h"
#include "popupitem.h"

#define dib_merge ORIG(void(__stdcall *)(uint8_t *, uint8_t *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t), 0x100f49d0)
#define dib_set ORIG(void(__stdcall *)(uint8_t *, int32_t, int32_t, int32_t, int32_t, int32_t), 0x100f4ae4)
#define blast_text ORIG(void(__cdecl *)(uint32_t, uint8_t *, uint32_t, const void *, int32_t, int32_t, const char *, int32_t), 0x101120b6)
#define overlay_register ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x100f4ade)
#define sprite_position ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t, int32_t, int32_t), 0x100f496a)
#define sprite_show ORIG(void(__stdcall *)(uint32_t, uint32_t, uint32_t, int32_t), 0x100f48e0)
#define sprite_empty ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x100f48c2)
#define sprite_hide ORIG(void(__stdcall *)(uint32_t, uint32_t, int32_t), 0x100f48c8)
#define g_strncpy ORIG(char *(__cdecl *)(char *, const char *, size_t), 0x100f5430)
#define g_memset ORIG(void *(__cdecl *)(void *, int, size_t), 0x100f4c10)
#define HAS(off, i) ((I(uint64_t, off) >> (i) & 1) != 0)
#define RECT_OF(i) (&I(int32_t, 0xdb08 + (i) * 16))
#define NAMES(k) ((const char *)ORIG_PTR(0x101804c0) + (k) * 0x16)

/* 0x100d42ea: draws item `i`'s text at row `y`: `text`, or its name from the table at
 * 0x1012ff00. Grey when not usable, else highlighted when `hi`. Indented by the local
 * player's side (+0x1b9f: 0 by 4, 1 by 6). `checked` is not used. */
void __cdecl popup_item_draw(uint32_t i, uint8_t *dib, int32_t y, int32_t usable, int32_t checked, int32_t hi,
                             const char *text)
{
    (void)checked;
    const void *colour = 0;
    int32_t tx = 0x18;
    char t[0x17];
    g_memset(t, 0, 0x17);
    if (i < 0x40) {
        if (!usable)
            colour = ORIG_PTR(0x10141cd8);
        else if (hi)
            colour = ORIG_PTR(0x101418d8);
        g_strncpy(t, text ? text : (const char *)ORIG_PTR(0x1012ff00) + i * 0x16, 0x16);
    }
    switch (I(uint8_t, 0x1b9f + I(int32_t, 0x1b65) * 0x1461)) {
    case 0: tx += 4; break;
    case 1: tx += 6; break;
    }
    blast_text(I(uint32_t, 0x1812), dib, I(uint32_t, 0xdaec), colour, tx, y + 2, t, 0);
}

/* redraws item `i`, cleared first, plain or highlighted */
static void item_redraw(int32_t i, int32_t hi)
{
    uint8_t *dib = I(uint8_t *, 0xdad0);
    int32_t *r = RECT_OF(i);
    int32_t h = r[3] - r[1], y = I(int32_t, 0xdf08 + i * 4);
    dib_set(dib, 0, y, P(int32_t, dib, 4), h, 0);
    int32_t usable = HAS(0xe010, i), on = HAS(0xe018, i);
    uint8_t *src = i < 0x30 ? I(uint8_t *, 0xdad8) : on ? I(uint8_t *, 0xdadc) : I(uint8_t *, 0xdae0);
    dib_merge(dib, src, P(int32_t, src, 4), P(int32_t, src, 0xc), 0, y, 0, 0);
    const char *text = 0;
    if (i >= 0x20 && i < 0x30)
        text = NAMES(i - 0x20);
    else if (i == 0x10 || i == 0x12)
        text = NAMES(i - 8);
    popup_item_draw(i, dib, y, usable, on, hi, text);
}

/* 0x100d3c1e: the pointer at (x, y) over the menu: moves the highlight to the item under it.
 * With `click`, returns that item, or -2 when it is not usable or there is none; without,
 * -1. When +0xd79f is set a pointer over no item also clears the highlight. */
int32_t __stdcall IsoBrowsePopupMenuC(uint8_t *iso, int32_t x, int32_t y, int32_t click)
{
    g_iso = iso;
    int32_t leaving = 0;
    for (int32_t i = 0; i < 0x40 || leaving; i++) {
        if (!leaving) {
            int32_t *r = RECT_OF(i);
            if (r[0] > x || r[2] <= x || r[1] > y || r[3] <= y) {
                if (I(int32_t, 0xd79f) && i == 0x3f)
                    leaving = 1;
                continue;
            }
        }
        if (leaving || I(int32_t, 0xdaf4) != i) {
            int32_t old = I(int32_t, 0xdaf4);
            if (old != -1)
                item_redraw(old, 0);
            if (leaving) {
                I(int32_t, 0xdaf4) = -1;
                break;
            }
            I(int32_t, 0xdaf4) = i;
            if (!click) {
                item_redraw(i, 1);
                sprite_position(I(uint32_t, 0x181e), I(uint32_t, 0xdab6), 0, 0, 1);
                sprite_show(I(uint32_t, 0x181e), I(uint32_t, 0xdab6), 0, 1);
            }
        }
        if (!click)
            return -1;
        return HAS(0xe010, i) ? i : -2;
    }
    return click ? -2 : -1;
}

/* 0x100d4263: hides the menu */
void __stdcall IsoHidePopupMenuC(uint8_t *iso)
{
    g_iso = iso;
    I(int32_t, 0xdaf0) = 0;
    I(int32_t, 0xdaf4) = -1;
    overlay_register(I(uint32_t, 0x181e), 0);
    sprite_empty(I(uint32_t, 0x181e), I(uint32_t, 0xdab6));
    sprite_hide(I(uint32_t, 0x181e), I(uint32_t, 0xdab6), 1);
}
