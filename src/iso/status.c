/* The game logic, 0x10053e3b-0x10054b89 (the parts every game runs): the status line,
 * the scroll region, memory use, the game state and centring the view. */
#include "iso_core.h"
#include "view.h"
#include "status.h"

#define orig_strcpy ORIG(char *(__cdecl *)(char *, const char *), 0x100f4c70)
#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)
#define debug_log ORIG(void(__cdecl *)(int, const char *, ...), 0x10095f64)
/* BmpFont's BFDrawTextShadow through the original's import */
#define bf_draw_text_shadow                                                                        \
    ORIG(int(__cdecl *)(void *, uint32_t, int32_t, int32_t, int, int, int, uint32_t, int, int), 0x101120bc)

#define TEXT_BUF ((char *)(g_iso + 0xdea))
#define STATUS_MSG ((uint32_t *)ORIG_PTR(0x101572c0)) /* the "redraw" message to the status sprite */

static void status_redraw(void *sprite)
{
    STATUS_MSG[1] = 0;
    STATUS_MSG[2] = 0;
    STATUS_MSG[3] = 0;
    SpriteSendMessage(I(void *, 0x181e), sprite, 4, STATUS_MSG);
}

/* 0x10053ef5 (formerly status_text in the callers): the status line's text (NULL:
 * cleared), shown for `ms` milliseconds (0: until replaced). BFDrawTextShadow's result */
int __cdecl status_text(const char *text, int ms)
{
    int rc = 1;
    if (I(int32_t, 0x16c))
        return 1;
    char *var = (char *)RtGetVariableString(I(void *, 0x180e), I(uint32_t, 0xc34a));
    if (!var || !text) {
        if (!text && I(uint32_t, 0xc342)) {
            uint8_t *dib = RtGetResource(I(void *, 0x180e), I(uint32_t, 0xc342))[0];
            if (!dib)
                return 1;
            orig_memset(P(void *, dib, 0x20), 0, (uint32_t)(P(int32_t, dib, 8) * P(int32_t, dib, 0xc)));
            void *sprite = RtGetResource(I(void *, 0x180e), I(uint32_t, 0xc346))[1];
            if (!sprite)
                return 1;
            status_redraw(sprite);
        }
    } else {
        orig_strcpy(var, text);
        rc = bf_draw_text_shadow(I(void *, 0x1812), I(uint32_t, 0xc342), I(int32_t, 0x17bd), -1, 0, 0, 0x23,
                                 I(uint32_t, 0xc34a), 0, 0);
        void *sprite = RtGetResource(I(void *, 0x180e), I(uint32_t, 0xc346))[1];
        if (!sprite || !rc)
            return rc;
        status_redraw(sprite);
    }
    if (!ms)
        I(uint32_t, 0xd8d0) = 0;
    else
        I(uint32_t, 0xd8d0) = (uint32_t)ms + timeGetTime();
    return rc;
}

/* 0x10053e3b: whether the point is inside the scroll region */
int32_t __stdcall IsoMouseInScrollRegion(uint8_t *iso, int32_t x, int32_t y)
{
    g_iso = iso;
    if (x < I(int32_t, 0xd787) || I(int32_t, 0xd78f) <= x || y < I(int32_t, 0xd78b) || I(int32_t, 0xd793) <= y)
        return 0;
    return 1;
}

/* 0x10053eb1: a string resource on the status line */
void __cdecl IsoDebugStrIntf(uint8_t *iso, uint32_t str)
{
    g_iso = iso;
    status_text(RtGetString(I(void *, 0x180e), str), 0);
}

/* 0x1005419f: the memory in use; with `log`, written to the debug log under a label */
uint32_t __cdecl IsoMemUsed(uint8_t *iso, uint32_t label, int32_t log)
{
    g_iso = iso;
    uint32_t n = MemMgrSize();
    if (log) {
        if (!label)
            wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x1013dac8), (const char *)ORIG_PTR(0x1013dabc), n);
        else
            wsprintfA(TEXT_BUF, (const char *)ORIG_PTR(0x1013daa0), RtGetString(I(void *, 0x180e), label), n);
        debug_log(7, TEXT_BUF);
    }
    return n;
}

/* 0x1005424b */
void __cdecl IsoSetGameState(uint8_t *iso, int32_t state)
{
    g_iso = iso;
    DebugStr((const char *)ORIG_PTR(0x1013dae4), I(int32_t, 0x17f5), state);
    I(int32_t, 0x17f5) = state;
}

/* 0x100549b0 (formerly view_follow in loop.c): the view's scroll position for a map
 * point (offset by half the view; an odd x made even), the view's edges, and the radar's
 * view box */
void __cdecl view_scroll_to(int32_t x, int32_t y)
{
    int32_t h = I(int32_t, 0xd71a) < 0 ? -(I(int32_t, 0xd71a) >> 1) : I(int32_t, 0xd71a) >> 1;
    x += h;
    h = I(int32_t, 0xd72e) < 0 ? -(I(int32_t, 0xd72e) >> 1) : I(int32_t, 0xd72e) >> 1;
    y += h;
    if ((x < 0 ? -x : x) & 1)
        x++;
    SpriteSetScrollPos(I(void *, 0x181e), -x, -y);
    int32_t pos[2];
    SpriteGetScrollPos(I(void *, 0x181e), pos);
    I(int32_t, 0xcd82) = -pos[0];
    I(int32_t, 0xcd86) = -pos[1];
    I(int32_t, 0xbfc1) = pos[0];
    I(int32_t, 0xbfc5) = pos[1];
    I(int32_t, 0xd6bb) = I(int32_t, 0xd78b) + I(int32_t, 0xcd86) - 0x80;
    I(int32_t, 0xd6c3) = I(int32_t, 0xd793) + I(int32_t, 0xcd86) + 0x80;
    I(int32_t, 0xd6bf) = I(int32_t, 0xd78f) + I(int32_t, 0xcd82) + 0x80;
    I(int32_t, 0xd6b7) = I(int32_t, 0xd787) + I(int32_t, 0xcd82) - 0x80;
    if (I(int32_t, 0x154) && I(void *, 0xbf0a))
        IsoCheckRadarScreenAreaUpdate(g_iso);
}
