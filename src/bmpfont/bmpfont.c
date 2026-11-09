/* BmpFont.r32 replacement: draws text with the game's bitmap fonts.
 *
 * Each font is a run of glyph sprites in the resource file, one per character
 * starting at ' ' (0x20); "font_base" is the resource id of the space glyph. Text is
 * drawn into an 8-bit engine bitmap (RtDib), then the screen area is repainted.
 *
 * This is a clean reimplementation of the original module's behavior, including its
 * quirks, so the game looks exactly the same. Quirks are marked "original:".
 * Bug fixes that change behavior belong in Phase 2.
 *
 * Callers: the menu script (DOMINION.bin) and the game logic (ISO.R32). They pass the
 * context returned by RtLibInit as the first argument of every call. */
#include <stdint.h>
#include <windows.h>
#include "rtlib32.h"

#include <mmsystem.h>

typedef struct BmpFont {
    void *app; /* engine application handle */
    void *hsl; /* sprite/screen object, for repaints */
} BmpFont;

/* Flags shared by the draw calls */
enum {
    TXT_UPPERCASE = 0x01,
    TXT_CENTER = 0x04,
    TXT_RIGHT = 0x08,
    TXT_AT_POS = 0x10, /* use the x/y arguments */
    TXT_CLEAR = 0x20,  /* clear the whole bitmap first */
    TXT_REPAINT = 0x40 /* repaint the screen area afterwards */
};

enum { SHADOW_PIXEL = 1 }; /* palette index used to mark shadow pixels */

/* No C runtime (see CMakeLists.txt): the few helpers we need. */
void *memset(void *dst, int c, size_t n)
{
    uint8_t *p = dst;
    while (n--)
        *p++ = (uint8_t)c;
    return dst;
}

static int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

/* The game never sets a locale, so the original toupper only changed a-z. */
static int ascii_upper(int c)
{
    return (c >= 'a' && c <= 'z') ? c - 0x20 : c;
}

/* Glyph offset of a character. Characters are signed: anything below ' ' and every
 * byte >= 0x80 maps to the space glyph. */
static int glyph_offset(char ch, int upper)
{
    int c = (signed char)ch;
    if (upper)
        c = ascii_upper(c);
    c -= 0x20;
    return c < 0 ? 0 : c;
}

static RtDib *get_dib(const BmpFont *bf, unsigned id)
{
    void **handle = RtGetResource(bf->app, id);
    return handle ? (RtDib *)*handle : NULL;
}

static int is_palette_index(int c)
{
    return c >= 0 && c < 0x100;
}

static void repaint(const BmpFont *bf, int left, int top, int right, int bottom)
{
    RECT rc = {left, top, right, bottom};
    SpritePaintRect(bf->hsl, &rc, 1);
}

/* ---- lifetime ---- */

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst, (void)reason, (void)reserved;
    return TRUE;
}

BmpFont *__cdecl RtLibInit(void *app)
{
    BmpFont *bf = LocalAlloc(LPTR, sizeof *bf);
    if (!bf)
        return NULL;
    bf->app = app;
    bf->hsl = RtGetHSL(app);
    return bf;
}

void __cdecl RtLibFree(BmpFont *bf)
{
    if (bf)
        LocalFree(bf);
}

int __cdecl WEP(void)
{
    return 1;
}

/* ---- measuring ---- */

/* Width in pixels of string resource string_id, without letter spacing. -1 if a glyph is missing. */
int __cdecl BFStringPixelWidth(BmpFont *bf, unsigned string_id, int font_base, uint8_t flags)
{
    const char *s = RtGetString(bf->app, string_id);
    if (!s)
        return -1;
    int len = str_len(s), total = 0;
    for (int i = 0; i < len; i++) {
        RtDib *g = get_dib(bf, font_base + glyph_offset(s[i], flags & TXT_UPPERCASE));
        if (!g)
            return -1;
        total += g->width;
    }
    return total;
}

/* ---- drawing ---- */

/* Draw text straight into a bitmap at (x, y), no letter spacing. x == -1 centers it.
 * color_map != 0 draws through a color map (team colors). */
int __cdecl BF_BlastText(BmpFont *bf, RtDib *dst, int font_base, int color_map, uint32_t x, int y,
                         const char *s, int upper)
{
    int len = str_len(s);
    if (!dst || len == 0)
        return 0;

    /* original: characters are unsigned here, unlike in the other calls */
    int total = 0;
    for (int i = 0; i < len; i++) {
        int c = (uint8_t)s[i];
        if (upper)
            c = ascii_upper(c);
        c -= 0x20;
        RtDib *g = get_dib(bf, font_base + (c < 0 ? 0 : c));
        if (!g)
            return 0;
        total += g->width;
    }
    if (x == 0xffffffffu)
        x = (uint32_t)(dst->pitch - total) >> 1;

    int pen = 0;
    for (int i = 0; i < len; i++) {
        int c = (uint8_t)s[i];
        if (upper)
            c = ascii_upper(c);
        c -= 0x20;
        RtDib *g = get_dib(bf, font_base + (c < 0 ? 0 : c));
        if (!g)
            return 0;
        if (dst->pitch < pen + x + g->width)
            return 0;
        if (color_map == 0)
            SpriteDibMerge(dst, g, g->width, g->height, pen + x, y, 0, 0);
        else
            SpriteDibColorMapMerge(dst, g, g->width, g->height, pen + x, y, 0, 0, 0, color_map);
        pen += g->width;
    }
    return 1;
}

/* Draw uppercase text (1 pixel letter spacing) into bitmap resource dst_id, vertically
 * centered. bg fills the text area first; every non-transparent pixel of the bitmap is
 * then recolored to color. Pass -1 to skip either. */
int __cdecl BF_DrawText(BmpFont *bf, unsigned dst_id, int font_base, int color, uint32_t bg,
                        uint8_t flags, const char *s)
{
    RtDib *dst = get_dib(bf, dst_id);
    int len = str_len(s);
    if (!dst)
        return 0;
    if (flags & TXT_CLEAR)
        memset(dst->bits, 0, dst->pitch * dst->height);
    if (len == 0)
        return 0;

    /* original: this call always uppercases and only knows ' ' .. 'Z' */
    uint32_t total = 0, max_h = 0;
    for (int i = 0; i < len; i++) {
        int c = ascii_upper((signed char)s[i]) - 0x20;
        if (c < 0 || c > 0x3a)
            c = 0;
        RtDib *g = get_dib(bf, c + font_base);
        if (!g)
            return 0;
        if (max_h < g->height)
            max_h = g->height;
        total += g->width + 1;
    }
    if (dst->pitch < total)
        return 0;

    uint32_t x = 0;
    uint32_t y = (dst->height - max_h) >> 1;
    if (flags & TXT_CENTER)
        x = (dst->pitch - total) >> 1;
    else if (flags & TXT_RIGHT)
        x = dst->pitch - total - 1;
    RECT rc = {x - 1, y - 1, total + x + 1, max_h + y + 1};

    if (is_palette_index((int)bg))
        SpriteDibSet(dst, x, 0, total, dst->height, bg);

    int pen = 0;
    for (int i = 0; i < len; i++) {
        int c = ascii_upper((signed char)s[i]) - 0x20;
        if (c < 0 || c > 0x3a)
            c = 0;
        RtDib *g = get_dib(bf, c + font_base);
        if (!g)
            return 0;
        SpriteDibMerge(dst, g, g->width, g->height, x + pen, y, 0, 0);
        pen += g->width + 1;
    }

    if (is_palette_index(color)) {
        int n = (int)dst->pitch * (int)dst->height;
        uint8_t *p = dst->bits;
        for (int i = 0; i < n; i++)
            if (p[i] != 0 && p[i] != bg)
                p[i] = (uint8_t)color;
    }
    if (flags & TXT_REPAINT)
        repaint(bf, rc.left, rc.top, rc.right, rc.bottom);
    return 1;
}

int __cdecl BFDrawText(BmpFont *bf, unsigned dst_id, int font_base, int color, uint32_t bg,
                       uint8_t flags, unsigned string_id)
{
    const char *s = RtGetString(bf->app, string_id);
    if (!s)
        return 0;
    return BF_DrawText(bf, dst_id, font_base, color, bg, flags, s);
}

/* Draw text with a drop shadow `offset` pixels down-right. The shadow is drawn first
 * and its pixels set to palette index 1, then the text is drawn on top and recolored
 * to color. bg fills the text area first (-1 = none). */
int __cdecl BF_DrawTextShadow(BmpFont *bf, unsigned dst_id, int font_base, int color, uint32_t bg,
                              int offset, uint8_t flags, const char *s, uint32_t pos_x,
                              uint32_t pos_y)
{
    RtDib *dst = get_dib(bf, dst_id);
    int len = str_len(s);
    if (!dst)
        return 0;
    if (flags & TXT_CLEAR)
        memset(dst->bits, 0, dst->pitch * dst->height);
    if (len == 0)
        return 0;

    int total = 0;
    uint32_t max_h = 0;
    for (int i = 0; i < len; i++) {
        RtDib *g = get_dib(bf, font_base + glyph_offset(s[i], flags & TXT_UPPERCASE));
        if (!g)
            return 0;
        if (max_h < g->height)
            max_h = g->height;
        total += g->width;
    }

    uint32_t x = 0, y;
    if (flags & TXT_CENTER)
        x = (uint32_t)(dst->pitch - total) >> 1;
    else if (flags & TXT_RIGHT)
        x = dst->pitch - total - 1;
    else if (flags & TXT_AT_POS)
        x = pos_x;
    y = (flags & TXT_AT_POS) ? pos_y : (dst->height - max_h) >> 1;

    RECT rc = {0, 0, offset + total + x, offset + y + max_h};
    rc.left = offset < 1 ? offset + x : x;
    rc.top = offset < 1 ? offset + y : y;

    if (dst->pitch < offset + total + x) {
        DebugStr("string length is too wide");
        return 0;
    }
    if (dst->height < offset + max_h + y) {
        DebugStr("string height is too tall");
        DebugStr("lpDstDib->Height=%d  string height=%d", dst->height, offset + max_h + y);
        return 0;
    }
    if (is_palette_index((int)bg))
        SpriteDibSet(dst, x, y, offset + total, offset + max_h, bg);

    /* pass 0 draws the shadow at +offset, pass 1 the text itself */
    for (int pass = 0; pass < 2; pass++) {
        int pen = 0;
        for (int i = 0; i < len; i++) {
            RtDib *g = get_dib(bf, font_base + glyph_offset(s[i], flags & TXT_UPPERCASE));
            if (!g)
                return 0;
            /* original: y is re-centered per glyph and keeps the last glyph's value */
            y = (flags & TXT_AT_POS) ? pos_y : (uint32_t)(dst->height - g->height) >> 1;
            SpriteDibMerge(dst, g, g->width, g->height, offset + x + pen, offset + y, 0, 0);
            pen += g->width;
        }

        int row = dst->height - (offset + max_h + y);
        int row_end = dst->height - y;
        if (pass == 0) {
            if (offset == 0)
                break; /* original: no shadow also means no recoloring */
            for (; row < row_end; row++) {
                uint8_t *line = dst->bits + dst->pitch * row;
                for (uint32_t col = x; (int)col < (int)(offset + total + x); col++)
                    if (line[col] != bg)
                        line[col] = SHADOW_PIXEL;
            }
        } else if (is_palette_index(color)) {
            for (; row < row_end; row++) {
                uint8_t *line = dst->bits + dst->pitch * row;
                for (uint32_t col = x; (int)col < (int)(offset + total + x); col++)
                    if (line[col] != bg && line[col] != SHADOW_PIXEL)
                        line[col] = (uint8_t)color;
            }
        }
        offset = 0;
    }

    if (flags & TXT_REPAINT)
        repaint(bf, rc.left, rc.top, rc.right, rc.bottom);
    return 1;
}

int __cdecl BFDrawTextShadow(BmpFont *bf, unsigned dst_id, int font_base, int color, uint32_t bg,
                             int offset, uint8_t flags, unsigned string_id, uint32_t pos_x,
                             uint32_t pos_y)
{
    const char *s = RtGetString(bf->app, string_id);
    if (!s)
        return 0;
    return BF_DrawTextShadow(bf, dst_id, font_base, color, bg, offset, flags, s, pos_x, pos_y);
}

/* Fill the rectangle (x1,y1)-(x2,y2) of bitmap resource dst_id with color. */
int __cdecl BFClearRect(BmpFont *bf, unsigned dst_id, int color, int x1, int y1, int x2, int y2)
{
    RtDib *dst = get_dib(bf, dst_id);
    if (!dst)
        return 0;
    if (x1 < 0 || (int)dst->pitch < x2) {
        DebugStr("x values out of range");
        DebugStr("x1=%d x2=%d width=%d", x1, x2, dst->pitch);
        return 0;
    }
    if (y1 < 0 || (int)dst->height < y2) {
        DebugStr("y values out of range");
        return 0;
    }
    int w = x2 - x1, h = y2 - y1;
    if (w < 0)
        w = -w;
    if (h < 0)
        h = -h;
    if (w == 0 || h == 0) {
        DebugStr("invalid height or width!");
        return 0;
    }
    if (is_palette_index(color))
        SpriteDibSet(dst, x1, y1, w, h, color);
    return 1;
}

int __cdecl BFRepaintRect(BmpFont *bf, int left, int top, int right, int bottom)
{
    repaint(bf, left, top, right, bottom);
    return 1;
}

/* Animated text: each word is drawn 4 times, recolored with colors[0..3] (a list of 4
 * palette indexes, 0 = keep) and shown with delay_ms between steps, so words "light
 * up" one after another. Returns 1, or a negative error code like the original. */
int __cdecl BFBrightWrite(BmpFont *bf, unsigned dst_id, int font_base, const int32_t *colors,
                          int shadow, uint8_t flags, unsigned string_id, uint32_t pos_x,
                          uint32_t pos_y, int delay_ms)
{
    uint8_t step_color[4];
    for (int i = 0; i < 4; i++)
        step_color[i] = (uint8_t)colors[i];

    const char *s = RtGetString(bf->app, string_id);
    RtDib *dst = get_dib(bf, dst_id);
    if (!dst)
        return -1;
    int len = s ? str_len(s) : 0;
    if (flags & TXT_CLEAR)
        memset(dst->bits, 0, dst->pitch * dst->height);
    if (len == 0)
        return -2;

    int total = 0, space_advance = 0;
    uint32_t max_h = 0;
    for (int i = 0; i < len; i++) {
        int id = font_base + glyph_offset(s[i], flags & TXT_UPPERCASE);
        RtDib *g = get_dib(bf, id);
        if (!g)
            return -3;
        if (max_h < g->height)
            max_h = g->height;
        total += g->width + 1;
        if (id == font_base)
            space_advance = g->width + 1;
    }

    uint32_t x = 0, y;
    if (flags & TXT_CENTER)
        x = (uint32_t)(dst->pitch - total) >> 1;
    else if (flags & TXT_RIGHT)
        x = dst->pitch - total - 1;
    else if (flags & TXT_AT_POS)
        x = pos_x;
    y = (flags & TXT_AT_POS) ? pos_y : (dst->height - max_h) >> 1;

    if (dst->pitch < x + shadow + total) {
        DebugStr("string length is too wide");
        return -5;
    }
    if (dst->height < y + shadow + max_h) {
        DebugStr("string height is too tall");
        DebugStr("lpDstDib->Height=%d  string height=%d", dst->height, y + shadow + max_h);
        return -6;
    }

    DWORD t0 = timeGetTime();
    int pos = 0, more = 1;
    while (more) {
        int word_start = pos;
        while (s[pos] != ' ' && s[pos] != '\0')
            pos++;
        if (s[pos] == '\0')
            more = 0;

        int word_w = 0;
        for (int step = 0; step < 4; step++) {
            int pen = 0;
            word_w = 0;
            int row0 = dst->height - (y + shadow + max_h);
            int row_end = dst->height - y;
            for (int i = word_start; i < pos; i++) {
                RtDib *g = get_dib(bf, font_base + glyph_offset(s[i], flags & TXT_UPPERCASE));
                if (!g)
                    return -7;
                if (shadow != 0) {
                    SpriteDibMerge(dst, g, g->width, g->height, x + pen + shadow, y + shadow, 0, 0);
                    /* original: the column range ends at the first glyph's width, so only
                     * the first letter of each word gets its shadow marked */
                    for (int row = row0; row < row_end; row++) {
                        uint8_t *line = dst->bits + dst->pitch * row;
                        for (uint32_t col = x + pen + shadow; (int)col < (int)(g->width + x + shadow);
                             col++)
                            if (line[col] != 0)
                                line[col] = SHADOW_PIXEL;
                    }
                }
                SpriteDibMerge(dst, g, g->width, g->height, x + pen, y, 0, 0);
                pen += g->width + 1;
                word_w += g->width + 1;
            }
            if (step_color[step] != 0) {
                for (int row = row0; row < row_end; row++) {
                    uint8_t *line = dst->bits + dst->pitch * row;
                    for (uint32_t col = x; (int)col < (int)(x + word_w); col++)
                        if (line[col] != 0 && line[col] != SHADOW_PIXEL)
                            line[col] = step_color[step];
                }
            }
            repaint(bf, x + shadow, y + shadow, x + word_w + shadow, y + shadow + max_h);
            DWORD until = delay_ms + t0;
            while (timeGetTime() < until)
                Sleep(0);
            t0 = timeGetTime();
        }
        x += word_w + space_advance;
        pos++;
    }
    return 1;
}
