/* The game logic, 0x10005bb1-0x100065c3: marker sprites, straight lines between units,
 * the message lines at the top of the screen, and the display process's timing. */
#include "iso_core.h"
#include "display.h"

/* not rewritten yet (names ours, provisional) */
#define cell_passable ORIG(int(__cdecl *)(int, uint32_t, uint16_t, uint16_t), 0x1007bd4c)
/* BmpFont's, through the original's import */
#define BF_BlastText                                                                         \
    ORIG(int(__cdecl *)(void *, void *, uint32_t, void *, int, int, const char *, int), 0x101120b6)

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define ISO_MARKERS 0x0eee /* four sprites */

/* 0x10005bb1 */
void __cdecl marker_show(int i)
{
    if (i < 4)
        SpriteShow(HSL, I(void *, ISO_MARKERS + i * 4), 0, 1);
}

/* 0x10005bf1 */
void __cdecl marker_hide(int i)
{
    if (i < 4)
        SpriteSendMessage(HSL, I(void *, ISO_MARKERS + i * 4), 1, g_msg_hide);
}

/* 0x10005c34: would show a number (0-999) on a marker; the original jumps past its body */
void __cdecl marker_number(int n, int i)
{
    (void)n, (void)i;
}

/* 0x10005ef5: the direction (0-7) from unit a to unit b when b lies straight along one,
 * else -1 */
int16_t __cdecl unit_direction(uint32_t a, uint32_t b)
{
    uint8_t *ua = iso_unit((int)(a & 0xffff)), *ub = iso_unit((int)(b & 0xffff));
    int ax = P(int16_t, ua, U_X), ay = P(int16_t, ua, U_Y);
    int bx = P(int16_t, ub, U_X), by = P(int16_t, ub, U_Y);
    if (by == ay)
        return ax < bx ? 0 : 4;
    if (bx == ax)
        return ay < by ? 2 : 6;
    int dx = ax - bx, dy = ay - by;
    if ((dx < 0 ? -dx : dx) / 2 != (dy < 0 ? -dy : dy))
        return -1;
    if (ay < by)
        return ax < bx ? 1 : 3;
    return ax < bx ? 7 : 5;
}

/* 0x10005d13: whether unit a has a clear way in a straight line to unit b: 1 yes, 0 no
 * (blocked, or 20 empty cells in a row), -1 not on a straight line */
int16_t __cdecl unit_line_clear(uint32_t a, uint32_t b)
{
    int tx, ty;
    int x = P(int16_t, iso_unit((int)(a & 0xffff)), U_X);
    int y = P(int16_t, iso_unit((int)(a & 0xffff)), U_Y);
    IsoVTilePos(g_iso, &x, &y, &tx, &ty);
    int bx = P(int16_t, iso_unit((int)(b & 0xffff)), U_X);
    int by = P(int16_t, iso_unit((int)(b & 0xffff)), U_Y);
    IsoVTilePos(g_iso, &bx, &by, &tx, &ty);
    int16_t dir = unit_direction(a, b);
    if (dir == -1)
        return -1;
    int16_t empty = 0;
    x += g_step_dirs[dir * 4 + (y & 1) * 2];
    y += g_step_dirs[dir * 4 + (y & 1) * 2 + 1];
    for (;;) {
        if (bx == x && by == y)
            return 1;
        int16_t r = (int16_t)cell_passable(x, (uint32_t)y, (uint16_t)dir,
                                           P(uint8_t, iso_unit((int)(a & 0xffff)), U_PLAYER));
        if (r < 0 || empty >= 0x14)
            return 0;
        if (r > 0)
            empty = 0;
        else
            empty++;
        x += g_step_dirs[dir * 4 + (y & 1) * 2];
        y += g_step_dirs[dir * 4 + (y & 1) * 2 + 1];
    }
}

/* ---- the message lines ---- */

/* a message: 0xc0 bytes from 0xa22, in a list linked through +0xbe */
#define MSG(i, off) (0xa22 + (i) * 0xc0 + (off))
#define MSG_PLAYER 0x00 /* short */
#define MSG_UNTIL 0x06  /* game time it is shown until */
#define MSG_TEXT 0x0a
#define MSG_NEXT 0xbe   /* char */
#define ISO_MSG_FIRST 0x1abe /* char */
#define ISO_MSG_LAST 0x1abf
#define ISO_MSG_COUNT 0x1ac0
#define LINE_SPRITE(n) (0xd952 + (n) * 0x10)
#define LINE_SHOWN(n) (0xd956 + (n) * 0x10)
#define LINE_DIB(n) (0xd95e + (n) * 0x10)

/* 0x1000605e: draws the newest messages (up to five) in the player's colours, drops the
 * expired ones */
void __cdecl messages_draw(void)
{
    if (I(int, 0x28))
        return;
    G(uint32_t, 0x101572c4) = 0;
    G(uint32_t, 0x101572c8) = 0;
    G(uint32_t, 0x101572cc) = 0;
    int8_t m = I(int8_t, ISO_MSG_FIRST);
    int n = 0;
    while (m >= 0 && n < 5 && n < I(int8_t, ISO_MSG_COUNT)) {
        if (I(uint32_t, 0x17dd) < I(uint32_t, MSG(m, MSG_UNTIL))) {
            /* original: the text goes into the engine's buffer of a string resource */
            char *text = (char *)RtGetString(APP, I(uint32_t, 0xc34a));
            if (text) {
                lstrcpyA(text, (const char *)(g_iso + MSG(m, MSG_TEXT)));
                uint8_t *dib = *(uint8_t **)RtGetResource(APP, I(uint32_t, LINE_DIB(n)));
                if (!dib)
                    return;
                uint32_t w = P(uint32_t, dib, 8), h = P(uint32_t, dib, 0xc);
                G(uint32_t, 0x10157280) = ((h >> 1) - (uint32_t)(I(int32_t, 0xd722) >> 1)) + (h + 2) * (uint32_t)n;
                G(uint32_t, 0x1015727c) = ((w >> 1) - (uint32_t)(I(int32_t, 0xd70e) >> 1)) + 2;
                void *sprite = I(void *, LINE_SPRITE(n));
                SpriteSendMessage(HSL, sprite, 4, ORIG_PTR(0x10157278));
                int colours = I(int32_t, I(uint16_t, MSG(m, MSG_PLAYER)) * ISO_PLAYER_SIZE + 0x1ba2);
                void *map = G(void *, 0x101450d8 + colours * 4);
                memset(P(void *, dib, 0x20), 0, h * w);
                BF_BlastText(I(void *, 0x1812), dib, I(uint32_t, 0xd9a6), map, 0, 0, text, 0);
                G(uint32_t, 0x101572c4) = 0;
                G(uint32_t, 0x101572c8) = 0;
                G(uint32_t, 0x101572cc) = 0;
                SpriteSendMessage(HSL, sprite, 4, ORIG_PTR(0x101572c0));
                I(int32_t, LINE_SHOWN(n)) = 1;
                n++;
            }
            m = I(int8_t, MSG(m, MSG_NEXT));
        } else {
            I(uint16_t, MSG(m, MSG_PLAYER)) = 0;
            I(int8_t, ISO_MSG_FIRST) = I(int8_t, MSG(m, MSG_NEXT));
            I(uint16_t, MSG(m, MSG_NEXT)) = 0xffff; /* original: also the next message's byte */
            m = I(int8_t, ISO_MSG_FIRST);
            I(int8_t, ISO_MSG_COUNT)--;
        }
    }
    if (I(int8_t, ISO_MSG_FIRST) < 0)
        I(uint8_t, ISO_MSG_LAST) = 0xff;
    for (; n < 5; n++)
        if (I(int32_t, LINE_SHOWN(n))) {
            SpriteSendMessage(HSL, I(void *, LINE_SPRITE(n)), 1, g_msg_hide);
            I(int32_t, LINE_SHOWN(n)) = 0;
        }
}

/* 0x10006459: an entry of a 2D table {data, count, ..., x stride, y stride}, or -1
 * outside it */
int32_t __cdecl table_get(uint32_t *t, int x, int y)
{
    int32_t *p = (int32_t *)(uintptr_t)(t[0] + t[5] * (uint32_t)x * 4 + t[6] * (uint32_t)y * 4);
    if ((uintptr_t)p < t[0] || (uintptr_t)p >= t[1] * 4 + t[0])
        return -1;
    return *p;
}

/* ---- the display process ---- */

#define ISO_FRAME_DELAY 0xd84f

/* 0x100064c8 */
void __stdcall IsoSetDisplayDelay(uint8_t *iso, uint32_t delay)
{
    g_iso = iso;
    I(uint32_t, ISO_FRAME_DELAY) = I(int, 0xd79f) ? 0x10 : delay;
}

/* 0x1000652a */
int __stdcall IsoInitDisplayProcess(uint8_t *iso)
{
    g_iso = iso;
    I(uint32_t, ISO_FRAME_DELAY) = 0x28;
    I(uint32_t, 0xd883) = timeGetTime();
    I(uint32_t, 0xd853) = I(uint32_t, 0xd883);
    G(int32_t, 0x1017baf8) = 1;
    return 1;
}

/* 0x1000659a */
void __stdcall IsoStopDisplayProcess(uint8_t *iso)
{
    g_iso = iso;
    I(uint32_t, ISO_FRAME_DELAY) = 0;
}
