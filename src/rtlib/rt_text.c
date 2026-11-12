/* Engine core, 0x100236a0-0x100242b0: text objects (resource type 0x15). The text is
 * drawn with GDI into a bitmap that becomes the image of up to eight sprites; fonts
 * are shared through a small cache. Dominion draws its text another way and never
 * creates one. Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_text.h"

#define RU(off) R(U, off)
#define RB(off) R(uint8_t, off)
#define HSL R(uint8_t *, RT_HSL)
#define RT_FOCUS_TEXT 0x3e77
#define RT_PAUSED 0x95f1


/* A text object, 0x277 bytes */
#define T_ID 0x00
#define T_FONT 0x04
#define T_COLOR 0x08
#define T_RES 0x24        /* the resource's 10 values: */
#define T_FONT_NAME 0x28  /*   font name (string id) */
#define T_TEXT_ID 0x2c    /*   text (string id) */
#define T_HEIGHT 0x30     /*   font height, or a variable holding it */
#define T_X 0x34
#define T_Y 0x38
#define T_W 0x3c          /*   box; 0 measures the text */
#define T_H 0x40
#define T_FORMAT 0x44     /*   DrawText flags */
#define T_HEIGHT_VAR 0x49
#define T_SINGLE 0x4a     /*   no word wrap */
#define T_PASSWORD 0x4b
#define T_TEXT 0x52
#define T_SPRITES 0x253   /* 8 sprites showing it */
#define T_SHOW_LAST 0x273 /* password: the last character stays visible */
#define T_SIZE 0x277

/* the font cache: 10 entries of 0x2c bytes */
typedef struct CachedFont {
    HFONT font;
    U refs;
    int height;
    char name[0x20];
} CachedFont;
#define g_font_count G(U, 0x10086eec)
#define g_fonts ((CachedFont *)ORIG_PTR(0x10086ef0))
#define g_password_buf ((char *)ORIG_PTR(0x10086ce8))
#define g_color_map ((uint8_t *)ORIG_PTR(0x100870a8))

/* 0x10024290 */
U __cdecl var_value(U n) { return (U)*script_var(n); }

/* 0x10023857: a font of `height` named `name`, shared */
HFONT __stdcall font_get(LPCSTR name, int height)
{
    U i;
    for (i = 0; i < g_font_count; i++) {
        if (g_fonts[i].height == height && !lstrcmpiA(name, g_fonts[i].name)) {
            g_fonts[i].refs++;
            return g_fonts[i].font;
        }
    }
    if (i >= 10)
        return NULL;
    LOGFONTA lf;
    memset(&lf, 0, sizeof lf);
    lstrcpyA(lf.lfFaceName, name);
    lf.lfHeight = height;
    lf.lfCharSet = DEFAULT_CHARSET;
    HFONT f = CreateFontIndirectA(&lf);
    if (!f)
        return NULL;
    CachedFont *c = &g_fonts[g_font_count];
    c->font = f;
    c->refs = 1;
    c->height = height;
    lstrcpyA(c->name, name);
    g_font_count++;
    return f;
}

/* 0x100239ca: lets go of a font; always 0 */
U __stdcall font_release(HFONT font)
{
    U i;
    for (i = 0; i < g_font_count; i++)
        if (g_fonts[i].font == font)
            break;
    if (i >= g_font_count)
        return 0;
    if (--g_fonts[i].refs)
        return 0;
    DeleteObject(font);
    g_font_count--;
    if (!g_font_count || g_font_count == i)
        return 0;
    memcpy(&g_fonts[i], &g_fonts[i + 1], (g_font_count - i) * sizeof(CachedFont));
    return 0;
}

/* 0x100237ca: (re)selects the object's font */
void __stdcall text_font_update(uint8_t *t)
{
    const char *name = string_get(P(U, t, T_FONT_NAME));
    if (!name)
        return;
    if (P(U, t, T_FONT))
        P(U, t, T_FONT) = font_release(P(HFONT, t, T_FONT));
    U h = P(U, t, T_HEIGHT);
    if (t[T_HEIGHT_VAR])
        h = var_value(h);
    P(HFONT, t, T_FONT) = font_get(name, (int)h);
}

/* 0x10023e30: draws the text into a new bitmap the size of its box (or of the text) */
HBITMAP __stdcall text_draw_bitmap(uint8_t *t, HDC dc)
{
    const char *s;
    if (!t[T_TEXT]) {
        s = (const char *)ORIG_PTR(0x10075604); /* " " */
    } else if (!t[T_PASSWORD]) {
        s = (const char *)(t + T_TEXT);
    } else {
        lstrcpyA(g_password_buf, (const char *)(t + T_TEXT));
        int n = lstrlenA(g_password_buf);
        memset(g_password_buf, '*', (!n || !P(U, t, T_SHOW_LAST)) ? (U)n : (U)n - 1);
        s = g_password_buf;
    }
    RECT r = {0, 0, P(int, t, T_W), P(int, t, T_H)};
    U fmt = P(U, t, T_FORMAT);
    if (!t[T_SINGLE])
        fmt |= DT_WORDBREAK;
    if (!P(int, t, T_W) || !P(int, t, T_H))
        DrawTextA(dc, s, -1, &r, fmt | DT_CALCRECT | DT_NOPREFIX);
    RECT box = {r.left, r.top, r.right + 1, r.bottom + 1};
    HBITMAP bm = CreateCompatibleBitmap(R(HDC, RT_DC), box.right - r.left, box.bottom - r.top);
    if (!bm)
        return NULL;
    HPALETTE pal = SpriteSelectPalette(HSL, dc);
    HGDIOBJ old = SelectObject(dc, bm);
    COLORREF bk = RB(RT_PALETTE_DEVICE) ? 0x1000000 : 0;
    HBRUSH br = CreateSolidBrush(bk);
    if (br) {
        FillRect(dc, &box, br);
        DeleteObject(br);
    }
    SetBkColor(dc, bk);
    SetTextColor(dc, RB(RT_PALETTE_DEVICE) ? (P(uint16_t, t, T_COLOR) | 0x1000000u) : 0xffffffu);
    SetBkMode(dc, OPAQUE);
    DrawTextA(dc, s, -1, &box, fmt | DT_NOPREFIX | DT_NOCLIP);
    if (old)
        SelectObject(dc, old);
    if (pal)
        SelectPalette(dc, pal, FALSE);
    return bm;
}

/* 0x10023cea: the text as engine bitmap data; without a palette device the white
 * text is mapped to the object's color */
U *__stdcall text_bitmap(uint8_t *t)
{
    HDC dc = CreateCompatibleDC(R(HDC, RT_DC));
    if (!dc)
        return NULL;
    HGDIOBJ old = P(U, t, T_FONT) ? SelectObject(dc, P(HGDIOBJ, t, T_FONT)) : NULL;
    HBITMAP bm = text_draw_bitmap(t, dc);
    U *bits = bm ? (U *)dib_from_bitmap(bm, 0, 0, 0) : NULL;
    if (bits && !RB(RT_PALETTE_DEVICE)) {
        memset(g_color_map + 1, (int)P(U, t, T_COLOR), 0xff);
        g_color_map[0] = 0;
        G(U, 0x10075608) = 1;
        MapBytes((uint8_t *)(uintptr_t)bits[8], (int)(bits[2] * bits[3]), g_color_map);
    }
    if (bm)
        DeleteObject(bm);
    if (P(U, t, T_FONT))
        SelectObject(dc, old);
    DeleteDC(dc);
    return bits;
}

/* 0x10023c28: places the bitmap by the box position and the alignment flags */
void __stdcall text_place(const uint8_t *t, uint8_t *bits)
{
    U fmt = P(U, t, T_FORMAT);
    P(U, bits, 0x14) = P(U, t, T_X);
    P(U, bits, 0x18) = P(U, t, T_Y);
    if (fmt & DT_RIGHT)
        P(U, bits, 0x14) -= P(U, bits, 4) >> 1;
    else if (!(fmt & DT_CENTER))
        P(U, bits, 0x14) += (P(U, bits, 4) - 1) >> 1;
    if (fmt & DT_BOTTOM)
        P(U, bits, 0x18) -= P(U, bits, 0xc) >> 1;
    else if (!(fmt & DT_VCENTER))
        P(U, bits, 0x18) += (P(U, bits, 0xc) - 1) >> 1;
}

/* 0x100240e6: the sprites showing the old image get the new one */
void __stdcall text_replace(const uint8_t *t, void *old, U new_bits)
{
    if (!old)
        return;
    for (int i = 0; i < 8; i++)
        if (P(U, t, T_SPRITES + i * 4))
            SpriteReplaceBitmap(HSL, P(uint8_t *, t, T_SPRITES + i * 4), (U)(uintptr_t)old, new_bits);
    winfree(old);
}

/* 0x10023ba4: draws the text again and shows it */
U __stdcall text_render(uint8_t *t)
{
    U *bits = text_bitmap(t);
    if (!bits)
        return 0;
    bits[7] = P(U, t, T_ID);
    text_place(t, (uint8_t *)bits);
    U *e = obj_entry(P(U, t, T_ID));
    void *old = (void *)(uintptr_t)e[0];
    e[0] = (U)(uintptr_t)bits;
    text_replace(t, old, (U)(uintptr_t)bits);
    return 1;
}

/* 0x10023ac8: frees a text object; always 0 */
U __stdcall text_free(uint8_t *t)
{
    if (!t)
        return 0;
    if (P(U, t, T_FONT))
        P(U, t, T_FONT) = font_release(P(HFONT, t, T_FONT));
    if (RU(RT_FOCUS_TEXT) == P(U, t, T_ID))
        RU(RT_FOCUS_TEXT) = 0;
    LocalFree(t);
    return 0;
}

/* 0x1002371b: a text object for resource `id` from its 10 values */
uint8_t *__stdcall text_create(const U *res, U id)
{
    uint8_t *t = LocalAlloc(LPTR, T_SIZE);
    if (!t)
        return NULL;
    P(U, t, T_ID) = id;
    memcpy(t + T_RES, res, 40);
    P(U, t, T_COLOR) = P(U, t, T_RES);
    const char *s = string_get(res[2]);
    if (s)
        lstrcpyA((LPSTR)(t + T_TEXT), s);
    text_font_update(t);
    if (!text_render(t))
        t = (uint8_t *)(uintptr_t)text_free(t);
    return t;
}

/* 0x10023b44, 0x100236a0: a sprite stops or starts showing the text (8 at most) */
void __stdcall text_detach(uint8_t *t, U sprite)
{
    for (int i = 0; i < 8; i++)
        if (P(U, t, T_SPRITES + i * 4) == sprite)
            P(U, t, T_SPRITES + i * 4) = 0;
}

U __stdcall text_attach(uint8_t *t, U sprite)
{
    text_detach(t, sprite);
    for (int i = 0; i < 8; i++) {
        if (!P(U, t, T_SPRITES + i * 4)) {
            P(U, t, T_SPRITES + i * 4) = sprite;
            return 1;
        }
    }
    return 0;
}

/* 0x10024171: command: new text from string +10, optionally formatted with +6 */
U __stdcall text_set(const uint8_t *c)
{
    if (!obj_is(P(U, c, 2), 0x15))
        return 0;
    uint8_t *t = (uint8_t *)(uintptr_t)obj_entry(P(U, c, 2))[2];
    const char *s = string_get(P(U, c, 10));
    if (!s)
        return 0;
    if (!c[0x11])
        lstrcpyA((LPSTR)(t + T_TEXT), s);
    else
        wsprintfA((LPSTR)(t + T_TEXT), s, P(U, c, 6));
    return text_render(t);
}

/* 0x10024225: command: new color +6 */
U __stdcall text_set_color(const uint8_t *c)
{
    if (!obj_is(P(U, c, 2), 0x15))
        return 0;
    uint8_t *t = (uint8_t *)(uintptr_t)obj_entry(P(U, c, 2))[2];
    P(U, t, T_COLOR) = P(U, c, 6);
    return text_render(t);
}

/* ---- text entry: one text object at a time takes the keyboard ---- */

#define T_CMD 0x0c          /* the focus command's first 24 bytes: */
#define T_DONE_HANDLER 0x12 /*   handler told when entry ends (1 accepted, 0 cancelled) */
#define T_DEST_STRING 0x16  /*   string that receives the text (bit 31: none) */
#define T_MAX_LEN 0x1e      /*   16-bit maximum length (0: 512) */
#define T_DIGITS 0x20       /*   digits only */
#define T_LEAD_PENDING 0x4c /* a double-byte character's first byte came */
#define T_LEAD_BYTE 0x50
#define RT_EDIT_SAVED 0x9b55 /* the text before editing, for cancel */
#define flush_messages(m) flush_messages(m)

/* 0x10024560 */
uint8_t *__stdcall text_of(U id) { return (uint8_t *)(uintptr_t)obj_entry(id)[2]; }

/* 0x100246f2 */
U __cdecl edit_beep(void)
{
    MessageBeep(0xffffffff);
    return 0;
}

/* 0x1002443d: the cursor "_" goes after the text */
void __stdcall edit_cursor_add(uint8_t *t)
{
    lstrcatA((LPSTR)(t + T_TEXT), (LPCSTR)ORIG_PTR(0x1007560c));
    P(U, t, T_SHOW_LAST) = 1;
}

/* 0x1002446e: ends entry: the cursor goes (or, cancelled, the old text comes back),
 * the text is stored and the handler told */
U __stdcall edit_end(uint8_t *t, U cancel)
{
    P(U, t, T_LEAD_PENDING) = 0;
    P(U, t, T_SHOW_LAST) = 0;
    if (!cancel)
        t[T_TEXT + lstrlenA((LPCSTR)(t + T_TEXT)) - 1] = 0;
    else
        lstrcpyA((LPSTR)(t + T_TEXT), (LPCSTR)(g_rt + RT_EDIT_SAVED));
    char *dest = (P(U, t, T_DEST_STRING) & 0x80000000u) ? NULL : (char *)string_get(P(U, t, T_DEST_STRING));
    if (dest)
        lstrcpyA(dest, (LPCSTR)(t + T_TEXT));
    if (P(U, t, T_DONE_HANDLER))
        event_send(P(U, t, T_DONE_HANDLER), cancel == 0, 0);
    text_render(t);
    RU(RT_FOCUS_TEXT) = 0;
    return 0;
}

/* 0x10024366: ends the current entry (the first argument is not used) */
void __stdcall edit_stop(U unused, U cancel)
{
    (void)unused;
    uint8_t *t = text_of(RU(RT_FOCUS_TEXT));
    if (t)
        edit_end(t, cancel);
}

/* 0x100243a6: starts entry in the text object of command `c` */
U __stdcall edit_begin(const uint8_t *c)
{
    uint8_t *t = text_of(P(U, c, 2));
    if (!t)
        return 0;
    memcpy(t + T_CMD, c, 24);
    if (!P(uint16_t, t, T_MAX_LEN))
        P(uint16_t, t, T_MAX_LEN) = 0x200;
    lstrcpyA((LPSTR)(g_rt + RT_EDIT_SAVED), (LPCSTR)(t + T_TEXT));
    edit_cursor_add(t);
    RU(RT_FOCUS_TEXT) = P(U, c, 2);
    return 1;
}

/* 0x100242b0: command: gives text object +2 the keyboard (0: nobody); the current
 * entry ends first, accepted */
U __stdcall edit_focus(const uint8_t *c)
{
    if (P(U, c, 2) && !obj_is(P(U, c, 2), 0x15))
        return 0;
    if (RU(RT_FOCUS_TEXT))
        edit_stop(RU(RT_FOCUS_TEXT), 0);
    if (!P(U, c, 2))
        return 1;
    flush_messages(WM_CHAR);
    if (!edit_begin(c))
        return 0;
    text_render(text_of(RU(RT_FOCUS_TEXT)));
    return 1;
}

/* 0x1002467d: removes the last character (all of them with `all`) */
U __stdcall edit_backspace(uint8_t *t, U all)
{
    char *p = (char *)(t + T_TEXT);
    if (!all) {
        int n = lstrlenA(p);
        if (n - 1 < 1)
            return edit_beep();
        p = CharPrevA((LPCSTR)(t + T_TEXT), p + n - 1);
    }
    *p = 0;
    edit_cursor_add(t);
    return 1;
}

/* 0x1002470c: adds a typed character (double-byte characters come in two parts) */
U __stdcall edit_type(uint8_t *t, U ch)
{
    if (!P(U, t, T_LEAD_PENDING)) {
        if (IsDBCSLeadByte((BYTE)ch)) {
            P(uint16_t, t, T_LEAD_BYTE) = (uint16_t)ch;
            P(U, t, T_LEAD_PENDING) = 1;
            return 0;
        }
    } else {
        ch = (U)(t[T_LEAD_BYTE] << 8 | (uint8_t)ch);
    }
    if (t[T_DIGITS] && !((ch & 0xffff) > 0x2f && (ch & 0xffff) < 0x3a))
        return edit_beep();
    int n = lstrlenA((LPCSTR)(t + T_TEXT));
    if (n - 1 >= (int)P(uint16_t, t, T_MAX_LEN))
        return edit_beep();
    char *p = (char *)(t + T_TEXT);
    if (n != 1) {
        while (*p)
            p = CharNextA(p);
        p--; /* onto the cursor */
    }
    if (P(U, t, T_LEAD_PENDING)) {
        *p++ = (char)(ch >> 8);
        P(U, t, T_LEAD_PENDING) = 0;
    }
    p[0] = (char)ch;
    p[1] = 0;
    edit_cursor_add(t);
    return 1;
}

/* 0x1002457e: a typed character: Esc cancels, Enter accepts, Backspace, or text.
 * (The original tests an unset variable for other keys; here they redraw nothing.) */
void __stdcall edit_char(U ch)
{
    if (RB(RT_PAUSED))
        return;
    uint8_t *t = text_of(RU(RT_FOCUS_TEXT));
    if (!t)
        return;
    U changed = 0;
    if (ch == 0x1b || ch == 0xd)
        changed = edit_end(t, ch == 0x1b);
    else if (ch == 8)
        changed = edit_backspace(t, 0);
    else if ((ch == 0xd || ch == 10 || ch > 0x1f) && ch != 9)
        changed = edit_type(t, ch);
    if (changed && RU(RT_FOCUS_TEXT))
        text_render(t);
}

/* 0x10024855: Delete clears the text */
void __stdcall edit_key(U vk)
{
    if (RB(RT_PAUSED))
        return;
    uint8_t *t = text_of(RU(RT_FOCUS_TEXT));
    if (!t)
        return;
    U changed = 0;
    if (vk == VK_DELETE)
        changed = edit_backspace(t, 1);
    if (changed && RU(RT_FOCUS_TEXT))
        text_render(t);
}
