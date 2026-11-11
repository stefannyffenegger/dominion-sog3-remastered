/* Engine core, 0x1000d7c0-0x1000dfb0: bringing the scene up and taking it down. On a
 * palette display the palette fades in or out in 75 steps; otherwise the background is
 * wiped in or out (the sprite engine's wipes). Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_fade.h"

#define HSL R(uint8_t *, RT_HSL)
#define RT_FADE_STEP 0x3ddf
#define RT_3DEF 0x3def
#define RT_FADE_STEPS 0x3e1f
#define RT_DD_BLACK 0x95d9  /* bytes: clear the DirectDraw surfaces around a fade */
#define RT_FADING 0x95da
#define RT_SHOWN 0x95db
#define RT_NO_FADE 0x95ea

/* 0x1000d7c0: one step of the palette fade (up when showing, down when hiding) */
void __cdecl fade_step(void)
{
    SpriteAnimate(HSL);
    uint32_t step = R(uint8_t, RT_SHOWN) ? R(uint32_t, RT_FADE_STEP) : R(uint32_t, RT_FADE_STEPS) - R(uint32_t, RT_FADE_STEP);
    SpriteFadePalette(HSL, step, R(uint32_t, RT_FADE_STEPS), 10, R(uint32_t, RT_3DEF));
    R(uint32_t, RT_FADE_STEP)++;
    if (R(uint32_t, RT_FADE_STEPS) < R(uint32_t, RT_FADE_STEP))
        R(uint8_t, RT_FADING) = 0;
}

static void fade_run(uint32_t first, uint32_t steps)
{
    R(uint32_t, RT_FADE_STEP) = first;
    R(uint32_t, RT_FADE_STEPS) = steps;
    R(uint8_t, RT_FADING) = 1;
    if (R(uint8_t, RT_NO_FADE))
        R(uint8_t, RT_FADING) = 0;
    else
        while (R(uint8_t, RT_FADING))
            fade_step();
}

/* 0x1000d889: shows the hidden scene. The argument is overwritten with 75 steps. */
void __stdcall scene_show(uint32_t unused)
{
    (void)unused;
    uint32_t steps = 0x4b;
    if (!R(uint8_t, RT_COMPOSE))
        return;
    int msgs = SpriteSetSendMsg(HSL, 0);
    R(uint8_t, RT_SHOWN) = 1;
    if (!R(uint8_t, RT_PALETTE_DEVICE)) {
        SpriteSetBkgTransparent(HSL, !R(uint8_t, RT_BKG_IS_COLOR) || R(uint32_t, RT_BKG_COLOR));
        if (!R(uint32_t, RT_SCROLL_DIB))
            R(uint8_t *, RT_SCROLL_DIB) = SpriteDibCreate(R(int, RT_BKG_W), R(int, RT_BKG_H), 1, 8);
        if (R(uint32_t, RT_SCROLL_DIB)) {
            R(uint8_t, RT_FADING) = 1;
            wipe_in(HSL, R(uint8_t *, RT_BACKGROUND), (steps * 0x1e + 0x32) / 100, 1);
            R(uint8_t, RT_COMPOSE) = 0;
            R(void *, RT_SCROLL_DIB) = winfree(R(void *, RT_SCROLL_DIB));
        }
    } else {
        palette_changed(HSL);
        palette_set_painting(HSL, 1, 0);
        display_flip_now(HSL);
        fade_run(1, steps);
        R(uint8_t, RT_COMPOSE) = 0;
    }
    palette_set_painting(HSL, 1, 1);
    SpriteSetSendMsg(HSL, msgs);
}

/* 0x1000dafc: hides the scene */
void __stdcall scene_hide(uint32_t unused)
{
    (void)unused;
    uint32_t steps = 0x4b;
    if (R(uint8_t, RT_COMPOSE))
        return;
    int msgs = SpriteSetSendMsg(HSL, 0);
    R(uint8_t, RT_SHOWN) = 0;
    palette_set_painting(HSL, 0, 0);
    if (!R(uint8_t, RT_PALETTE_DEVICE)) {
        uint8_t *bg = R(uint8_t *, RT_BACKGROUND);
        if (bg) {
            uint8_t *copy = SpriteDibCreate(R(int, RT_BKG_W), R(int, RT_BKG_H), 0, 8);
            R(uint8_t *, RT_SCROLL_DIB) = copy;
            if (copy) {
                CopyHugeBytes(P(void *, copy, 0x20), P(void *, bg, 0x20), P(uint32_t, bg, 8) * P(uint32_t, bg, 0xc));
                R(uint8_t, RT_FADING) = 1;
                wipe_out(HSL, copy, (steps * 0x1e + 0x32) / 100, 1);
            }
        }
    } else {
        fade_run(2, steps);
        R(uint8_t, RT_COMPOSE) = 1;
        if (R(uint8_t, RT_DD_BLACK))
            SpritePaintDirectDrawSurfacesBlack1(HSL, 0, 0);
    }
    R(uint8_t, RT_COMPOSE) = 1;
    SpriteSetSendMsg(HSL, msgs);
}

static void show_wrapped(void)
{
    if (!R(uint8_t, RT_PALETTE_DEVICE))
        SpriteSetBkgTransparent(HSL, 1);
}

static void show_done(void)
{
    if (!R(uint8_t, RT_PALETTE_DEVICE) || R(uint8_t, RT_NO_FADE))
        SpriteSetBkgTransparent(HSL, 0);
}

static void hide_or_skip(void)
{
    if (!R(uint8_t, RT_95F9))
        scene_hide(0);
    else {
        R(uint8_t, RT_SHOWN) = 0;
        palette_set_painting(HSL, 0, 0);
    }
}

/* 0x1000dd2c: script commands 0x3c (show) and the one after it (hide) */
void __stdcall scene_cmd(const int16_t *c)
{
    show_wrapped();
    if (*c == 0x3c) {
        if (R(uint8_t, RT_DD_BLACK))
            SpritePaintDirectDrawSurfacesBlack(HSL, 0, 0);
        scene_show(0);
        show_done();
    } else
        hide_or_skip();
}

/* 0x1000de5a, 0x1000def5: the same as built-in script functions */
void __stdcall scene_show_fn(uint32_t unused)
{
    (void)unused;
    show_wrapped();
    scene_show(0);
    show_done();
}

void __stdcall scene_hide_fn(uint32_t unused)
{
    (void)unused;
    if (!R(uint8_t, RT_PALETTE_DEVICE) || R(uint8_t, RT_NO_FADE))
        SpriteSetBkgTransparent(HSL, 1);
    if (!R(uint8_t, RT_95F9))
        scene_hide(0);
    else {
        palette_set_painting(HSL, 0, 0);
        R(uint8_t, RT_SHOWN) = 0;
    }
}
