/* The game logic, 0x10003fd4-0x10005bb1: the power bar, scrolling the map view (keys,
 * screen edges, gliding to a stop, screen shake), the radar's view box, and the
 * players' selections. */
#include "iso_core.h"
#include "power.h"
#include "view.h"

/* not rewritten yet (names ours, provisional) */
#define status_text ORIG(int(__cdecl *)(char *, int), 0x10053ef5)
#define cursor_scroll ORIG(void(__cdecl *)(int, int, int), 0x100145d0)
#define IsoHidePopupMenu ORIG(void(__stdcall *)(uint8_t *), 0x100d4263)
#define IsoSetMouseCursorOp ORIG(void(__stdcall *)(uint8_t *, uint32_t), 0x10012784)
#define RubberSetActiveBox ORIG(void(__stdcall *)(uint8_t *, int, int, int, int), 0x10060110)
#define IsoTilePos ORIG(void(__stdcall *)(uint8_t *, int *, int *, int *, int *), 0x100af006)

#define ISO_POWER_BAR 0x1826   /* the power bar sprite */
#define ISO_POWER_LEFT 0xbecd  /* byte: percent of power left */
#define ISO_SCROLL_X 0xcd82    /* the view's position (minus the engine's scroll position) */
#define ISO_SCROLL_Y 0xcd86
#define ISO_GLIDE 0xd6ab       /* steps left of gliding after a scroll, 0-5 */
#define ISO_GLIDE_DX 0xd6af
#define ISO_GLIDE_DY 0xd6b3
#define ISO_STEP_X 0xd712      /* pixels per scroll step */
#define ISO_STEP_Y 0xd726
#define ISO_SHAKE 0x0a02       /* screen shake on */
#define ISO_SHAKE_TIMES 0xc31f
#define ISO_SHAKE_AMOUNT 0xc323
#define ISO_SHAKE_PHASE 0xc327

#define HSL I(void *, ISO_HSL)
#define APP I(void *, ISO_APP)
#define SPRITE(off) I(void *, off)

/* the current cursor's sprite, or -1 */
static int32_t cursor_res(void) { return *I(int32_t *, 0x182a); }

/* 0x10004067: the power bar shows how much of the local player's power is used */
void __cdecl power_bar_set(int uses, uint32_t gives)
{
    uint32_t pct;
    if (!gives) {
        pct = 99;
    } else {
        pct = ((uint32_t)(uses << 10) / gives) * 100 >> 10;
        I(uint8_t, ISO_POWER_LEFT) = (uint8_t)(100 - (uint8_t)pct);
        if (pct > 99)
            pct = 99;
    }
    if (I(int, ISO_POWER_BAR)) {
        G(uint32_t, 0x101572c4) = P(uint32_t, SPRITE(ISO_POWER_BAR), 0x74);
        G(uint32_t, 0x101572cc) = 100;
        G(uint32_t, 0x101572c8) = pct / 5;
        SpriteSendMessage(HSL, SPRITE(ISO_POWER_BAR), 4, ORIG_PTR(0x101572c0));
    }
}

/* 0x10003fd4 */
void __cdecl IsoInitIntfPowerBar(uint8_t *iso, uint32_t res, uint16_t player)
{
    g_iso = iso;
    if (!res)
        return;
    RtLoad(I(void *, ISO_APP), res);
    I(void *, ISO_POWER_BAR) = RtGetResource(APP, res)[1];
    SpriteShow(HSL, SPRITE(ISO_POWER_BAR), 0, 1);
    power_grid_sum(player);
}

/* 0x1000413d: "<n>% total power" in the status line */
void __cdecl IsoPowerBarBrowse(uint8_t *iso)
{
    char text[32];
    g_iso = iso;
    wsprintfA(text, (const char *)ORIG_PTR(0x10118a48), I(uint8_t, ISO_POWER_LEFT));
    status_text(text, 0);
}

/* 0x10004190: the scroll box (the view area, from the middle of the screen) for the
 * screen size; the mouse scrolls the map outside it */
int __cdecl view_set_scroll_box(void)
{
    int w = I(int, 0xd70e), h = I(int, 0xd722);
    I(int32_t, 0xd6a7) = -0xec;
    I(int32_t, 0xd69b) = 0xec;
    I(int32_t, 0xd6a3) = 0x13c;
    I(int32_t, 0xd69f) = -0x13c;
    I(int32_t, 0xd6a7) -= (h - 480) / 2;
    I(int32_t, 0xd69b) += (h - 480) / 2;
    I(int32_t, 0xd6a3) += (w - 640) / 2;
    I(int32_t, 0xd69f) -= (w - 640) / 2;
    /* original: unset for other widths; we take 640's */
    int32_t box[4] = {-0x13f, -0xef, 0x141, 0x83};
    if (w == 800) {
        box[0] = -0x18f, box[1] = -299, box[2] = 0x191, box[3] = 0xa3;
    } else if (w == 0x400) {
        box[0] = -0x1ff, box[1] = -0x17f, box[2] = 0x201, box[3] = 0xd1;
    } else if (w == 0x500) {
        box[0] = -0x27f, box[1] = -0x1ff, box[2] = 0x281, box[3] = 0x125;
    }
    if (!I(int, ISO_HAVE_SPRITES))
        box[3] = h + box[1] - 0x51;
    RtSetScrollBox(APP, box);
    RubberSetActiveBox(g_iso, box[0], box[1], box[2], box[3]);
    return 1;
}

/* the view's position converted the way MSVC's __ftol does (low 32 bits of a 64-bit
 * truncation), with the x87 doing the arithmetic as the original's did */
static int32_t ftol(long double v) { return (int32_t)(int64_t)v; }

/* 0x100043c3: moves the radar's view box with the view; 1 if it moved */
int __cdecl IsoCheckRadarScreenAreaUpdate(uint8_t *iso)
{
    g_iso = iso;
    uint8_t *radar = I(uint8_t *, 0xbefe);
    int32_t vx = I(int32_t, ISO_SCROLL_X) + I(int32_t, 0xd71a) / 2;
    int32_t vy = I(int32_t, ISO_SCROLL_Y) + I(int32_t, 0xd72e) / 2;
    int32_t x = ftol((long double)vx / (long double)(I(int32_t, 0xd6f1) - 0x80) *
                     (long double)P(uint32_t, radar, 8));
    int32_t y = ftol((long double)vy / (long double)(I(int32_t, 0xd6f5) - 0x40) *
                     (long double)P(uint32_t, radar, 0xc));
    float scale = I(float, 0xd733);
    if (scale > G(float, 0x10113054))
        y = ftol((long double)y / scale);
    else
        x = -ftol((long double)x / scale);
    /* original: compares y before negating it, so it nearly always reports a move */
    if (I(int32_t, 0xbf16) == x && I(int32_t, 0xbf1a) == y)
        return 0;
    I(int32_t, 0xbf16) = x;
    I(int32_t, 0xbf1a) = -y;
    P(int32_t, I(uint8_t *, 0xbf0a), 0x14) = x;
    P(int32_t, I(uint8_t *, 0xbf0a), 0x18) = -y;
    return 1;
}

/* 0x10004573: shakes the screen `times` times by `amount` pixels */
void __cdecl view_shake(int32_t amount, int32_t times)
{
    if (I(uint8_t, 0x17e) || I(uint8_t, 0x181) || I(uint8_t, 0xd90c))
        return;
    I(int32_t, ISO_SHAKE) = 1;
    I(int32_t, ISO_SHAKE_AMOUNT) = amount;
    I(int32_t, ISO_SHAKE_TIMES) = times;
    I(int32_t, ISO_SHAKE_PHASE) = 2;
}

static int key_down(int vk) { return GetAsyncKeyState(vk) & 0x8000; }

/* 0x100045fe: scrolling with the arrow keys and the four keys above them (diagonals);
 * not with Alt or Ctrl held */
int __cdecl view_scroll_keys(int *dx, int *dy)
{
    int fast = I(int, 0xd7af), x = 0, y = 0;
    int sx = I(int32_t, ISO_STEP_X), sy = I(int32_t, ISO_STEP_Y);
    if (key_down(VK_MENU) || key_down(VK_CONTROL))
        return 0;
    if (key_down(VK_RIGHT))
        x = -sx;
    else if (key_down(VK_LEFT))
        x = sx;
    else if (key_down(VK_DOWN))
        y = -sy;
    else if (key_down(VK_UP))
        y = sy;
    else if (key_down(VK_NEXT))
        x = -sx, y = -sy;
    else if (key_down(VK_END))
        x = sx, y = -sy;
    else if (key_down(VK_HOME))
        x = sx, y = sy;
    else if (key_down(VK_PRIOR))
        x = -sx, y = sy;
    if (fast) {
        x *= 3;
        y *= 3;
    }
    if (!x && !y)
        return 0;
    *dx = x;
    *dy = y;
    return 1;
}

/* the area around the view in which things are kept up to date */
static void view_set_bounds(void)
{
    I(int32_t, 0xd6bb) = I(int32_t, 0xd78b) + I(int32_t, ISO_SCROLL_Y) - 0x80;
    I(int32_t, 0xd6c3) = I(int32_t, 0xd793) + I(int32_t, ISO_SCROLL_Y) + 0x80;
    I(int32_t, 0xd6bf) = I(int32_t, 0xd78f) + I(int32_t, ISO_SCROLL_X) + 0x80;
    I(int32_t, 0xd6b7) = I(int32_t, 0xd787) + I(int32_t, ISO_SCROLL_X) - 0x80;
}

static void scroll_by(int dx, int dy, int32_t *pos)
{
    if (!I(int, 0xd79f) && dx && dy) {
        SpriteScroll(HSL, dx, 0, 1);
        SpriteScroll(HSL, 0, dy, 1);
    } else {
        SpriteScroll(HSL, dx, dy, 1);
    }
    SpriteGetScrollPos(HSL, pos);
}

static void scrolled_to(const int32_t *pos)
{
    I(int32_t, ISO_SCROLL_X) = -pos[0];
    I(int32_t, ISO_SCROLL_Y) = -pos[1];
    I(int32_t, 0xbfc1) = pos[0];
    I(int32_t, 0xbfc5) = pos[1];
    if (I(int, ISO_HAVE_SPRITES))
        IsoCheckRadarScreenAreaUpdate(g_iso);
    view_set_bounds();
}

static int cursor_shown(void) { return I(int, 0xd759) && I(int, 0xd761); }

/* 0x10004824: one frame of scrolling: by (dx, dy) when `forced` (or by the keys), else
 * when the mouse is outside the scroll box; then gliding to a stop and the shake. */
int __cdecl view_scroll(int forced, int dx, int dy)
{
    int shift = 0, moved = 0, settle = 0;
    if (I(int, 0xd7ab))
        return 0;
    if (I(uint8_t, 0x17e) || I(uint8_t, 0x181)) {
        I(int32_t, ISO_GLIDE) = 0;
        I(int32_t, ISO_SHAKE) = 0;
        return 0;
    }
    if (I(int, 0xe12b) && view_scroll_keys(&dx, &dy))
        forced = 1;
    if (!forced) {
        int mx = I(int32_t, 0xd76d), my = I(int32_t, 0xd771), k = I(int, 0xd7a7) ? 3 : 1;
        if (mx < I(int32_t, 0xd69f))
            dx += I(int32_t, ISO_STEP_X) * k;
        else if (I(int32_t, 0xd6a3) < mx)
            dx -= I(int32_t, ISO_STEP_X) * k;
        if (my < I(int32_t, 0xd6a7))
            dy += I(int32_t, ISO_STEP_Y) * k;
        else if (I(int32_t, 0xd69b) < my)
            dy -= I(int32_t, ISO_STEP_Y) * k;
    } else {
        I(uint8_t, ISO_FLAGS) |= 0x80;
    }
    if (!dx && !dy) {
        if (I(int32_t, ISO_GLIDE)) {
            switch (I(int32_t, ISO_GLIDE)) {
            case 1: shift = 4; break;
            case 2: shift = 3; break;
            case 3:
            case 4: shift = 2; break;
            case 5: shift = 1; break;
            }
            dx = I(int32_t, ISO_GLIDE_DX) >> shift;
            dy = I(int32_t, ISO_GLIDE_DY) >> shift;
            if (I(int32_t, ISO_GLIDE) == 1 && !forced)
                settle = 1;
            if (I(int32_t, ISO_GLIDE) > 0)
                I(int32_t, ISO_GLIDE)--;
        }
    } else {
        moved = 1;
        if (I(int, 0xdaf0))
            IsoHidePopupMenu(g_iso);
    }
    if (I(int, ISO_SHAKE)) {
        int phase = I(int32_t, ISO_SHAKE_PHASE);
        if (phase == 0)
            dy += I(int32_t, ISO_SHAKE_AMOUNT);
        else if (phase == 1)
            dy += I(int32_t, ISO_SHAKE_AMOUNT) * -2;
        else if (phase == 2)
            dy += I(int32_t, ISO_SHAKE_AMOUNT);
        if (I(int32_t, ISO_SHAKE_PHASE) < 1) {
            I(int32_t, ISO_SHAKE_TIMES)--;
            if (I(int32_t, ISO_SHAKE_TIMES) < 1)
                I(int32_t, ISO_SHAKE) = 0;
            else
                I(int32_t, ISO_SHAKE_PHASE) = 2;
        } else {
            I(int32_t, ISO_SHAKE_PHASE)--;
        }
    }
    if (dx || dy) {
        I(int32_t, 0x144) = 0;
        if (I(uint32_t, 0x107b) < I(uint32_t, 0x1077))
            I(uint32_t, 0x1077) -= I(uint32_t, 0x107b);
        else
            I(uint32_t, 0x1077) = 0;
        if (!I(int, 0xd79f)) {
            if (cursor_shown()) {
                RtRegisterCursorSprite(APP, 0);
                SpriteHide(HSL, SPRITE(0xd761), 1);
            }
            if (I(int, 0x1af1)) {
                SpriteHide(HSL, SPRITE(0x1afd), 1);
                SpriteHide(HSL, SPRITE(0x1b01), 1);
            }
            if (I(int, 0xbe71))
                SpriteHide(HSL, SPRITE(0xbe79), 1);
            if (I(int, 0xcd8a))
                SpriteHide(HSL, SPRITE(0xcd96), 1);
            if (I(int, 0x175)) /* original: an int over the flag bytes at 0x175 */
                for (int i = 0; i < 5; i++)
                    if (I(int, 0xd956 + i * 0x10))
                        SpriteHide(HSL, SPRITE(0xd952 + i * 0x10), 1);
            if (I(int, 0xd9aa))
                SpriteHide(HSL, SPRITE(0xd9b6), 1);
            if (I(int, 0xd9ae))
                SpriteHide(HSL, SPRITE(0xd9c2), 1);
        }
        if (I(int, ISO_HAVE_SPRITES) && cursor_res() != -1 && I(int8_t, ISO_FLAGS) >= 0)
            SpriteHide(HSL, RtGetResource(APP, (unsigned)cursor_res())[1], 1);
        if (dx & 1)
            dx += dx > 0 ? -1 : 1;
        if (dy & 1)
            dy += dy > 0 ? -1 : 1;
        int32_t pos[2];
        scroll_by(dx, dy, pos);
        int cur = (-I(int32_t, ISO_SCROLL_X) == pos[0] && -I(int32_t, ISO_SCROLL_Y) == pos[1]) ? 0x24 : 0x23;
        if (moved && cursor_shown() && !forced)
            cursor_scroll(cur, dx, dy);
        if (!I(int, 0xd79f)) {
            if (I(int, 0x175))
                for (int i = 0; i < 5; i++)
                    if (I(int, 0xd956 + i * 0x10))
                        SpriteShow(HSL, SPRITE(0xd952 + i * 0x10), 0, 1);
            if (I(int, 0xd9aa))
                SpriteShow(HSL, SPRITE(0xd9b6), 0, 1);
            if (I(int, 0xd9ae))
                SpriteShow(HSL, SPRITE(0xd9c2), 0, 1);
            if (I(int, 0x1af1)) {
                SpriteShow(HSL, SPRITE(0x1afd), 0, 1);
                SpriteShow(HSL, SPRITE(0x1b01), 0, 1);
            }
            if (I(int, 0xbe71))
                SpriteShow(HSL, SPRITE(0xbe79), 0, 1);
            if (I(int, 0xcd8a))
                SpriteShow(HSL, SPRITE(0xcd96), 0, 1);
        }
        scrolled_to(pos);
        if (moved) {
            I(int32_t, ISO_GLIDE) = 5;
            I(int32_t, ISO_GLIDE_DX) = dx;
            I(int32_t, ISO_GLIDE_DY) = dy;
        }
    }
    if (I(int, ISO_HAVE_SPRITES) && cursor_res() != -1 && I(int8_t, ISO_FLAGS) < 0 && I(int, 0xe127)) {
        uint8_t rec[0x22];
        memset(rec, 0, sizeof rec);
        P(uint32_t, rec, 2) = I(uint32_t, 0xe127);
        int x = I(int32_t, 0xd76d), y = I(int32_t, 0xd771), tx, ty;
        IsoTilePos(g_iso, &x, &y, &tx, &ty);
        P(int32_t, rec, 10) = tx;
        P(int32_t, rec, 14) = ty;
        rec[9] = 2;
        RtRunMsg(APP, rec);
    }
    I(uint8_t, 0x17d) = (uint8_t)moved;
    if (I(int, ISO_HAVE_SPRITES) && settle) {
        I(uint8_t, 0x17d) = 0;
        I(uint8_t, ISO_FLAGS) &= 0x7f;
        IsoSetMouseCursorOp(g_iso, I(uint32_t, 0xd77d));
        if (cursor_res() != -1) {
            uint8_t *s = RtGetResource(APP, (unsigned)cursor_res())[1];
            P(uint32_t, s, 0xec) = 0;
            SpriteShow(HSL, s, P(uint32_t, s, 0x74), 1);
        }
        if (I(int, 0xd7a7))
            I(uint32_t, 0xd7ff) = timeGetTime();
    }
    return 1;
}

/* 0x10005579: centres the view on a unit */
void __cdecl view_center_on(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    if (!u)
        return;
    int dx = I(int32_t, ISO_SCROLL_X) - P(int16_t, u, U_X);
    int dy = I(int32_t, ISO_SCROLL_Y) - ((P(uint16_t, u, 0xba) >> 1) + P(int16_t, u, U_Y));
    if (!dx && !dy)
        return;
    if (cursor_shown()) {
        RtRegisterCursorSprite(APP, 0);
        SpriteHide(HSL, SPRITE(0xd761), 1);
    }
    int32_t pos[2];
    scroll_by(dx, dy, pos);
    if (cursor_shown())
        SpriteShow(HSL, SPRITE(0xd761), P(uint32_t, SPRITE(0xd761), 0x74), 1);
    scrolled_to(pos);
}

/* ---- selections ---- */


/* 0x1000585b: the player selects nothing */
int __cdecl selection_clear(int player)
{
    for (int i = 0; i < SELECTED_N(player); i++) {
        uint16_t id = selected((uint32_t)player)[i];
        if (id && id < 2000 && iso_unit(id)) {
            P(uint8_t, iso_unit(id), U_SELECTED) = 0;
            selected((uint32_t)player)[i] = 0;
            if (P(int8_t, iso_unit(id), U_SEL_STATE) == 1)
                P(int8_t, iso_unit(id), U_SEL_STATE) = 2;
        }
    }
    SELECTED_N(player) = 0;
    return 1;
}

/* 0x10005968: takes a unit out of the player's selection (notify: and tells its type) */
void __cdecl selection_remove(uint32_t id, uint32_t player, int notify)
{
    for (int i = 0; i < SELECTED_N(player); i++) {
        uint32_t v = selected(player)[i];
        if (!v || v >= 2000 || !iso_unit((int)v))
            continue;
        selected(player)[i] = 0;
        if (id != v)
            continue;
        P(uint8_t, iso_unit((int)v), U_SELECTED) = 0;
        if (!notify)
            continue;
        if (P(int8_t, iso_unit((int)id), U_SEL_STATE) != 4)
            P(int8_t, iso_unit((int)id), U_SEL_STATE) = 2;
        UnitHandler deselected = unit_type_handler(P(int32_t, iso_unit((int)v), U_ID_TYPE));
        if (deselected)
            deselected(v);
    }
    SELECTED_N(player) = 0;
    for (int i = 0; i < I(int, ISO_UNIT_COUNT); i++) {
        int32_t v = I(int32_t *, ISO_UNIT_LIST)[i];
        if (v > 0 && v < 2000 && iso_unit(v) && P(uint8_t, iso_unit(v), U_SELECTED) == 1 &&
            P(uint8_t, iso_unit(v), U_PLAYER) == player) {
            selected(player)[SELECTED_N(player)] = (uint16_t)v;
            SELECTED_N(player)++;
        }
    }
}
