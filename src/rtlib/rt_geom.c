/* Engine core, 0x10014840-0x10015150: the script's geometry built-ins (angles, sine
 * and cosine, line intersections, a point moved along an angle to a rectangle's edge)
 * and the controller layer's mouse steering. Angles are in tenths of a degree.
 * Rewritten from the original. */
#include "rtcore.h"
#include "rt.h"
#include "rt_geom.h"

#define RT_PAUSED 0x95f1
#define RT_MOUSE_DIR 0x188a  /* the last steering direction (word) */
#define RT_DEAD_ZONE 0x186a  /* RECT around the centre where the mouse does not steer */
#define RT_MOUSE_AREA 0x187a /* RECT the mouse is held in */
#define RT_MOUSE_X 0x8d83
#define RT_MOUSE_Y 0x8d87

/* x87 floating point: these stay the original's */
#define GetAngle ORIG(int(__stdcall *)(int, int), 0x1003dad4)
#define GetIntersect ORIG(int(__stdcall *)(int *, int, int, int, int, int, int, int, int), 0x1003e660)
#define controller_event ORIG(void(__stdcall *)(U, U), 0x1000e208)

#define g_dir_codes ((const uint8_t *)ORIG_PTR(0x10075340)) /* per quadrant, 4 bytes apart */

#define C(type, c, off) P(type, c, off)

/* 0x10014a8b: v / 10000, rounded half away from zero */
int __stdcall div10000(int v) { return (v < 1 ? v - 5000 : v + 5000) / 10000; }

/* 0x10014840: where the ray from (x, y) at `angle` leaves the rectangle */
void __stdcall ray_to_rect(int angle, int x, int y, int left, int top, int right, int bottom, int *out)
{
    if (angle == 0) {
        out[0] = x;
        out[1] = bottom;
        return;
    }
    if (angle == 900) {
        out[0] = right;
        out[1] = y;
        return;
    }
    if (angle == 1800) {
        out[0] = x;
        out[1] = top;
        return;
    }
    if (angle == 2700) {
        out[0] = left;
        out[1] = y;
        return;
    }
    int q = GetQuadrant(angle);
    int cx = q < 2 ? right : left;          /* the corner the ray heads for */
    int cy = q == 1 || q == 2 ? top : bottom;
    int dx = cx - x, dy = cy - y;
    if (!dx && !dy) {
        out[0] = x;
        out[1] = y;
        return;
    }
    int corner = GetAngle(dx, dy);
    if (corner == angle) {
        out[0] = cx;
        out[1] = cy;
        return;
    }
    int steep = corner < angle; /* the ray meets the vertical side */
    int a = angle % 900;
    if (q == 1 || q == 3) {
        steep = !steep;
        if (!steep)
            a = 900 - a;
        dy = -dy;
        dx = -dx;
    } else if (steep) {
        a = 900 - a;
    }
    int t = Tangent(a);
    if (steep)
        cy = y + div10000(dx * t);
    else
        cx = x + div10000(dy * t);
    out[0] = cx;
    out[1] = cy;
}

/* 0x10014acc: var = the angle from (x1, y1) to (x2, y2) */
void __stdcall builtin_angle(uint8_t *c)
{
    int x1 = C(int, c, 6), y1 = C(int, c, 0xa), x2 = C(int, c, 0xe), y2 = C(int, c, 0x12);
    int32_t *v = script_var(C(U, c, 2));
    *v = GetAngle(x2 - x1, y2 - y1);
}

/* 0x10014b23: (var x, var y) = where two lines meet, if they do */
void __stdcall builtin_intersect(uint8_t *c)
{
    int pt[2];
    if (GetIntersect(pt, C(int, c, 0xa), C(int, c, 0xe), C(int, c, 0x12), C(int, c, 0x16), C(int, c, 0x1a),
                     C(int, c, 0x1e), C(int, c, 0x22), C(int, c, 0x26))) {
        *script_var(C(U, c, 2)) = pt[0];
        *script_var(C(U, c, 6)) = pt[1];
    }
}

/* 0x10014bba: var = sine (command 0xca) or cosine of an angle */
void __stdcall builtin_sin_cos(uint8_t *c)
{
    int a = C(int, c, 6);
    int32_t *v = script_var(C(U, c, 2));
    *v = C(int16_t, c, 0) == 0xca ? Sine(a) : Cosine(a);
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : hi < v ? hi : v; }

/* 0x10014c1f: (var x, var y) = where the ray from a point (moved into the rectangle)
 * at an angle leaves the rectangle */
void __stdcall builtin_ray_to_rect(uint8_t *c)
{
    int l = C(int, c, 0x16), r = C(int, c, 0x1e), t = C(int, c, 0x1a), b = C(int, c, 0x22);
    if (r < l) {
        int s = l;
        l = r;
        r = s;
    }
    if (b < t) {
        int s = t;
        t = b;
        b = s;
    }
    int x = clamp(C(int, c, 0xa), l, r), y = clamp(C(int, c, 0xe), t, b);
    int out[2];
    ray_to_rect(NormalizeAngle(C(int, c, 0x12)), x, y, l, t, r, b, out);
    x = clamp(out[0], l, r);
    y = clamp(out[1], t, b);
    *script_var(C(U, c, 2)) = x;
    *script_var(C(U, c, 6)) = y;
}

/* 0x10014eb6: the steering direction for a mouse at `pos` (client coordinates); out of
 * the outer area the mouse is pulled back in */
uint8_t __cdecl mouse_steer(U pos)
{
    POINT p = {(int16_t)pos, (int16_t)(pos >> 16)};
    mouse_to_view(&p);
    R(int, RT_MOUSE_X) = p.x;
    R(int, RT_MOUSE_Y) = p.y;
    const RECT *dead = (const RECT *)(g_rt + RT_DEAD_ZONE), *area = (const RECT *)(g_rt + RT_MOUSE_AREA);
    if (p.x >= dead->left && p.x < dead->right && p.y >= dead->top && p.y < dead->bottom)
        return 0;
    int angle = GetAngle(p.x, p.y);
    if (p.x < area->left || p.x >= area->right || p.y < area->top || p.y >= area->bottom) {
        int out[2];
        ray_to_rect(angle, 0, 0, area->left + 2, area->top + 2, area->right - 2, area->bottom - 2, out);
        R(int, RT_MOUSE_X) = out[0];
        R(int, RT_MOUSE_Y) = out[1];
        SetCursorPos(out[0] + R(int, 0x36c7), R(int, 0x36cb) + out[1]);
    }
    return g_dir_codes[GetQuadrant((angle + 450) % 3600) * 4];
}

/* 0x10014dc0: a mouse message for the controller layer: moves steer, buttons press
 * and release (button codes; releases are complemented) */
U __stdcall mouse_controller(U msg, U pos)
{
    if (R(uint8_t, RT_PAUSED))
        return 1;
    uint8_t code = 0;
    switch (msg) {
    case WM_MOUSEMOVE:
        code = mouse_steer(pos);
        if (R(uint16_t, RT_MOUSE_DIR) == code)
            return 1;
        R(uint16_t, RT_MOUSE_DIR) = code;
        break;
    case WM_LBUTTONDOWN:
        code = 1;
        break;
    case WM_LBUTTONUP:
        code = 0xfe;
        break;
    case WM_RBUTTONDOWN:
        code = 2;
        break;
    case WM_RBUTTONUP:
        code = 0xfd;
        break;
    }
    controller_event(5, code);
    return 1;
}

/* 0x10015085: with `reset`, the mouse goes to the centre and the areas back to their
 * defaults (40 and 60 pixels around it) */
void __stdcall mouse_controller_reset(U unused, U reset)
{
    (void)unused;
    if (!(reset & 0xff))
        return;
    RECT *dead = (RECT *)(g_rt + RT_DEAD_ZONE), *area = (RECT *)(g_rt + RT_MOUSE_AREA);
    memset(dead, 0, sizeof *dead);
    memset(area, 0, sizeof *area);
    InflateRect(dead, 40, 40);
    InflateRect(area, 60, 60);
    R(uint16_t, RT_MOUSE_DIR) = 0;
    R(int, RT_MOUSE_X) = 0;
    R(int, RT_MOUSE_Y) = 0;
    SetCursorPos(R(int, 0x36c7), R(int, 0x36cb));
}
