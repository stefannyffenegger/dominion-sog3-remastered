/* The game logic, 0x100120c0-0x10012852: the in-game mouse cursor (a sprite with one
 * animation per cursor state) and the in-game font. */
#include "iso_core.h"
#include "cursor.h"

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define CUR_ON 0xd759       /* shown */
#define CUR_ENABLED 0xd75d
#define CUR_SPRITE 0xd761
#define CUR_TABLE 0xd765    /* the states' animation table */
#define CUR_SPRITE_RES 0xd769
#define CUR_X 0xd76d
#define CUR_Y 0xd771
#define CUR_TABLE_RES 0xd775
#define CUR_STATE 0xd779
#define CUR_OP 0xd77d

/* 0x100120c0: the cursor's animation table and sprite (resources); the cursor resource
 * `sprite_ptr` (a pointer to the current cursor's id) and an event to run */
int __stdcall IsoLoadMouseCursor(uint8_t *iso, uint32_t table, uint32_t sprite, uint32_t sprite_ptr, uint32_t event)
{
    g_iso = iso;
    if (P(int, iso, CUR_TABLE) || P(int, iso, CUR_TABLE_RES)) {
        RtUnload(P(void *, iso, ISO_APP), P(uint32_t, iso, CUR_TABLE_RES));
        I(uint32_t, CUR_TABLE) = 0;
        I(uint32_t, CUR_TABLE_RES) = 0;
        I(uint32_t, CUR_ENABLED) = 0;
    }
    I(uint32_t, 0xe127) = event ? event : 0;
    I(uint32_t, 0x182a) = sprite_ptr;
    if (!table)
        return 1;
    RtLoad(APP, table);
    I(void *, CUR_TABLE) = *RtGetResource(APP, table);
    if (!I(void *, CUR_TABLE))
        return 0;
    I(uint32_t, CUR_TABLE_RES) = table;
    I(uint32_t, CUR_SPRITE_RES) = sprite;
    if (!RtLoad(APP, sprite))
        return 0;
    I(void *, CUR_SPRITE) = RtGetResource(APP, sprite)[1];
    if (!I(void *, CUR_SPRITE))
        return 0;
    RtRegisterCursorSprite(APP, I(void *, CUR_SPRITE));
    SpriteSetLevel(HSL, I(void *, CUR_SPRITE), 29999);
    return 1;
}

/* 0x100122db */
void __stdcall IsoUnloadMouseCursor(uint8_t *iso)
{
    g_iso = iso;
    if (!P(int, iso, CUR_TABLE) && !P(int, iso, CUR_TABLE_RES))
        return;
    SpriteEmptyQueue(P(void *, iso, ISO_HSL), P(void *, iso, CUR_SPRITE));
    RtRegisterCursorSprite(APP, NULL);
    SpriteHide(HSL, I(void *, CUR_SPRITE), 1);
    RtUnload(APP, I(uint32_t, CUR_SPRITE_RES));
    RtUnload(APP, I(uint32_t, CUR_TABLE_RES));
    I(uint32_t, CUR_TABLE) = 0;
    I(uint32_t, CUR_TABLE_RES) = 0;
    I(uint32_t, CUR_SPRITE) = 0;
    I(uint32_t, CUR_ENABLED) = 0;
}

/* 0x100123e7: our cursor on (the system's hidden) or off */
void __stdcall IsoEnableMouseCursor(uint8_t *iso, int on)
{
    g_iso = iso;
    if (!P(int, iso, CUR_SPRITE_RES) || !P(int, iso, CUR_SPRITE)) {
        P(uint32_t, iso, CUR_ENABLED) = 0;
        return;
    }
    if (on)
        RtCursor(P(void *, iso, ISO_APP), 9);
    I(int32_t, CUR_ENABLED) = on;
    I(uint32_t, CUR_X) = 0;
    I(uint32_t, CUR_Y) = 0;
}

/* 0x10012486: cursor state `state` (its animation: first and last cell, speed), shown or
 * not (state 99: hidden); returns the previous state */
uint32_t __stdcall IsoSetMouseCursorState(uint8_t *iso, uint32_t state, int shown)
{
    g_iso = iso;
    uint32_t was = P(uint32_t, iso, CUR_STATE);
    if (!P(int, iso, CUR_SPRITE_RES) || !P(int, iso, CUR_SPRITE)) {
        P(uint32_t, iso, CUR_ENABLED) = 0;
        return was;
    }
    void *s = P(void *, iso, CUR_SPRITE);
    if (!P(uint8_t, iso, 0x17d) && state < 0x30) {
        int32_t *t = P(int32_t *, iso, CUR_TABLE);
        int32_t *e = (int32_t *)(uintptr_t)(t[0] + t[0xb] * state * 4);
        int32_t first = e[0], last = e[1], speed = e[2];
        SpriteEmptyQueue(P(void *, iso, ISO_HSL), s);
        G(int32_t, 0x101572c4) = first;
        G(int32_t, 0x101572c8) = last;
        G(int32_t, 0x101572cc) = speed;
        if (last == first) {
            SpriteSendMessage(HSL, I(void *, CUR_SPRITE), 4, ORIG_PTR(0x101572c0));
        } else {
            SpriteSendMessage(HSL, I(void *, CUR_SPRITE), 1, ORIG_PTR(0x10157334));
            SpritePostMessage(HSL, I(void *, CUR_SPRITE), 4, ORIG_PTR(0x101572c0));
            SpritePostMessage(HSL, I(void *, CUR_SPRITE), 4, ORIG_PTR(0x10157338));
        }
    }
    I(int32_t, CUR_ON) = shown;
    I(uint32_t, CUR_STATE) = state;
    if (!shown || I(uint32_t, CUR_STATE) == 99) {
        SpriteEmptyQueue(HSL, I(void *, CUR_SPRITE));
        RtRegisterCursorSprite(APP, NULL);
        SpriteHide(HSL, I(void *, CUR_SPRITE), 1);
    } else {
        SpriteSetPosition(HSL, I(void *, CUR_SPRITE), I(int32_t, CUR_X), I(int32_t, CUR_Y), 0);
        RtRegisterCursorSprite(APP, I(void *, CUR_SPRITE));
    }
    return was;
}

/* 0x1001275f */
uint32_t __stdcall IsoGetMouseCursorState(uint8_t *iso)
{
    g_iso = iso;
    return P(uint32_t, iso, CUR_STATE);
}

/* 0x10012784: the cursor state for the current operation */
int __stdcall IsoSetMouseCursorOp(uint8_t *iso, uint32_t op)
{
    g_iso = iso;
    P(uint32_t, iso, CUR_OP) = op;
    IsoSetMouseCursorState(iso, op, 1);
    return 1;
}

/* 0x100127c1 */
uint32_t __stdcall IsoGetMouseCursorOp(uint8_t *iso)
{
    g_iso = iso;
    return P(uint32_t, iso, CUR_OP);
}

/* 0x100127e6: the fonts for the game's text and the unit name popup */
void __cdecl IsoInitInGameFont(uint8_t *iso, uint32_t font, uint32_t font2, uint32_t popup, uint32_t popup_dib)
{
    g_iso = iso;
    P(uint32_t, iso, 0x17c1) = font;
    I(uint32_t, 0x17c5) = font2;
    I(uint32_t, 0xd686) = popup;
    I(uint32_t, 0xd682) = popup_dib;
    RtLoad(APP, popup);
}
