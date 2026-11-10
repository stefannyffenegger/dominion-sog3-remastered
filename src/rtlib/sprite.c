/* The sprite engine's sprite objects: creating, listing, animating, bounds, getters and
 * setters. Rewritten from the original (0x1004d270-0x1004f2f1 and 0x10050a90).
 *
 * A sprite is a 0xf4-byte block (fields at the original's offsets, S()). Sprites sit in
 * two doubly linked lists between two sentinels created with the first sprite:
 *  - by drawing level (prev at +0, next at +4), head hsl[0] (level -90000), tail hsl[1]
 *    (level 90000);
 *  - the animation list (prev +8, next +0xc) that SpriteAnimate walks, in the same order.
 * The sprite engine's state (HSL) is shared with code not rewritten yet (H()). */
#include "orig.h"
#include "sprite.h"
#include "display.h"
#include "utils.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, s, off) (*(type *)((uint8_t *)(s) + (off)))

/* HSL fields */
#define HSL_HEAD 0x000         /* level list sentinels */
#define HSL_TAIL 0x004
#define HSL_CURRENT 0x008      /* SpriteAnimate's position in the animation list */
#define HSL_TOPMOST 0x00c
#define HSL_DONE_PROC 0x014    /* proc(sprite, arg): animation step finished */
#define HSL_DONE_ARG 0x018
#define HSL_SERVICE 0x024      /* proc(arg), called between sprites */
#define HSL_SERVICE_ARG 0x460
#define HSL_DC 0x468
#define HSL_DELAYS 0x484       /* delay table for queue waits */
#define HSL_ANIMATING 0x89c
#define HSL_COUNT 0x8c4        /* sprites, not counting the sentinels */
#define HSL_ANIMATED 0x8c8     /* sprites visited by the last SpriteAnimate */
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8
#define HSL_VIEW_BOTTOM 0x920
#define HSL_DIRECTDRAW 0x954
#define HSL_DEFAULT_MERGE 0x9f8
#define HSL_B5C 0xb5c
#define HSL_IFACE_ON 0xb68

/* sprite fields */
#define SP_PREV 0x00
#define SP_NEXT 0x04
#define SP_APREV 0x08
#define SP_ANEXT 0x0c
#define SP_BITMAP 0x18     /* current cel */
#define SP_1C 0x1c
#define SP_PARTS 0x20      /* composed sprites: 0x14 bytes each */
#define SP_CELS 0x24       /* cel table (bitmaps or resource ids) */
#define SP_QUEUE 0x28      /* script queue: start, current, end */
#define SP_QUEUE_CUR 0x2c
#define SP_QUEUE_END 0x30
#define SP_INFO 0x34       /* 0x4a-byte block */
#define SP_X 0x3c
#define SP_Y 0x40
#define SP_RECT 0x44       /* bounds */
#define SP_OLD_RECT 0x54
#define SP_NEXT_STEP 0x64
#define SP_NEXT_MOVE 0x68
#define SP_SPEED 0x6c
#define SP_MOTION 0x70
#define SP_SHOWN_CEL 0x74
#define SP_LAST 0x78
#define SP_FLIP_X 0x7c     /* byte */
#define SP_FLIP_Y 0x7d     /* byte */
#define SP_CEL 0x80
#define SP_FIRST 0x88
#define SP_NCELS 0x8c
#define SP_NPARTS 0x90
#define SP_ID 0x94
#define SP_LEVEL 0x98
#define SP_BREAK_LOOPS 0xa4
#define SP_CALLBACK 0xac
#define SP_ENABLED 0xb8
#define SP_FORWARD 0xc0
#define SP_MOVED 0xc4      /* new bounds overlap the old ones */
#define SP_PAUSED 0xd0
#define SP_LOOPS 0xd8      /* more than one cel to animate */
#define SP_POSITIONED 0xdc
#define SP_SCROLLS 0xe4
#define SP_UPDATED 0xe8
#define SP_VISIBLE 0xec
#define SP_MERGE 0xf0
#define SP_SIZE 0xf4
#define INFO_BOUNDS_PROC 0x24
#define INFO_PAUSED_AT 0x18
#define INFO_USER 0x20
#define INFO_EXTRA_ID 0x38
#define INFO_SIZE 0x4a

#define MAX_SPRITES 4000

/* original functions not rewritten yet */
#define sprite_step ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10054373)
#define sprite_redraw ORIG(void(__stdcall *)(uint8_t *), 0x10053f6b)
#define sprite_move ORIG(void(__stdcall *)(uint8_t *, int, int, int, int), 0x10057753)
#define SpriteSetQueue ORIG(int(__stdcall *)(uint8_t *, uint8_t *, void *, uint32_t), 0x10054cde)
#define SpriteHide ORIG(void(__stdcall *)(uint8_t *, uint8_t *, int), 0x100579e3)
#define parts_update_bounds ORIG(void(__stdcall *)(uint8_t *), 0x10062320)
#define sprite_reposition ORIG(void(__stdcall *)(uint8_t *), 0x100560d0)
#define sprite_callback ORIG(void(__stdcall *)(uint8_t *), 0x10053c50)

typedef uint32_t(__stdcall *DoneProc)(uint8_t *, uint32_t);
typedef void(__stdcall *SpriteServiceFn)(uint32_t);
typedef void(__stdcall *BoundsProc)(uint8_t *, uint8_t *, RECT *);

/* assembly that changes EBX: called through orig_stdcall_unsafe */
static uint32_t intersect_rect(RECT *out, const RECT *a, const RECT *b)
{
    uint32_t args[3] = {(uint32_t)(uintptr_t)out, (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b};
    return orig_stdcall_unsafe(ORIG_PTR(0x1005f6ac), 3, args);
}

/* ---- lists ---- */

/* 0 if s is a sprite of the list, -1 if not */
int __stdcall sprite_check(uint8_t *s)
{
    if (s)
        for (uint8_t *p = S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_NEXT); p != H(uint8_t *, HSL_TAIL);
             p = S(uint8_t *, p, SP_NEXT))
            if (p == s)
                return 0;
    return -1;
}

uint8_t *__stdcall sprite_find_id(uint32_t id)
{
    if (!H(uint32_t, HSL_COUNT) || !H(uint8_t *, HSL_HEAD))
        return NULL;
    for (uint8_t *p = S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_NEXT); p != H(uint8_t *, HSL_TAIL);
         p = S(uint8_t *, p, SP_NEXT))
        if (S(uint32_t, p, SP_ID) == id)
            return p;
    return NULL;
}

/* The sprite before s (in level order) that is in the animation list */
uint8_t *__stdcall sprite_anim_prev(uint8_t *s)
{
    uint8_t *head = H(uint8_t *, HSL_HEAD), *p;
    if (S(uint8_t *, head, SP_ANEXT) == H(uint8_t *, HSL_TAIL))
        return head;
    for (p = S(uint8_t *, s, SP_PREV); p != head && !S(uint8_t *, p, SP_APREV); p = S(uint8_t *, p, SP_PREV))
        ;
    return p;
}

uint8_t *__cdecl sprite_alloc(void)
{
    uint8_t *s;
    if (H(uint32_t, HSL_COUNT) >= MAX_SPRITES)
        return NULL;
    s = LocalAlloc(LPTR, SP_SIZE);
    if (!s)
        return NULL;
    S(void *, s, SP_INFO) = winmalloc(INFO_SIZE, 0);
    if (!S(void *, s, SP_INFO)) {
        LocalFree(s);
        return NULL;
    }
    memset(S(void *, s, SP_INFO), 0, INFO_SIZE);
    return s;
}

/* A new sprite, linked in by level (after the others of the same level) */
uint8_t *__stdcall sprite_new(int32_t level)
{
    uint8_t *s = sprite_alloc(), *at, *ap;
    if (!s)
        return NULL;
    S(int32_t, s, SP_LEVEL) = level;
    if (!H(uint32_t, HSL_COUNT) && !H(void *, HSL_HEAD) && !H(void *, HSL_TAIL)) {
        uint8_t *head = sprite_alloc(), *tail = sprite_alloc();
        if (!head || !tail)
            return NULL; /* original: leaks s */
        S(uint8_t *, s, SP_APREV) = head;
        S(uint8_t *, s, SP_PREV) = head;
        H(uint8_t *, HSL_HEAD) = head;
        S(uint8_t *, s, SP_ANEXT) = tail;
        S(uint8_t *, s, SP_NEXT) = tail;
        H(uint8_t *, HSL_TAIL) = tail;
        S(int32_t, head, SP_LEVEL) = -90000;
        S(int32_t, tail, SP_LEVEL) = 90000;
        S(uint8_t *, tail, SP_APREV) = s;
        S(uint8_t *, head, SP_ANEXT) = s;
        S(uint8_t *, tail, SP_PREV) = s;
        S(uint8_t *, head, SP_NEXT) = s;
        S(uint8_t *, tail, SP_ANEXT) = NULL;
        S(uint8_t *, head, SP_APREV) = NULL;
        S(uint8_t *, tail, SP_NEXT) = NULL;
        S(uint8_t *, head, SP_PREV) = NULL;
    } else {
        at = S(uint8_t *, H(uint8_t *, HSL_TAIL), SP_PREV);
        if (H(void *, HSL_TOPMOST))
            at = S(uint8_t *, at, SP_PREV);
        while (at && level < S(int32_t, at, SP_LEVEL))
            at = S(uint8_t *, at, SP_PREV);
        /* original: no sprite can sort before the head (level -90000) */
        S(uint8_t *, s, SP_NEXT) = S(uint8_t *, at, SP_NEXT);
        S(uint8_t *, s, SP_PREV) = at;
        S(uint8_t *, S(uint8_t *, at, SP_NEXT), SP_PREV) = s;
        S(uint8_t *, at, SP_NEXT) = s;
        ap = sprite_anim_prev(s);
        S(uint8_t *, s, SP_ANEXT) = S(uint8_t *, ap, SP_ANEXT);
        S(uint8_t *, s, SP_APREV) = ap;
        S(uint8_t *, S(uint8_t *, ap, SP_ANEXT), SP_APREV) = s;
        S(uint8_t *, ap, SP_ANEXT) = s;
    }
    H(uint32_t, HSL_COUNT)++;
    return s;
}

/* Frees the cels that are allocated bitmaps (not resource ids), each once */
void __stdcall sprite_free_cels(uint8_t *s)
{
    uint32_t *cels = S(uint32_t *, s, SP_CELS);
    for (uint32_t i = 0; i < S(uint32_t, s, SP_NCELS); i++) {
        int first = 1;
        for (uint32_t j = 0; first && j < i; j++)
            first = cels[j] != cels[i];
        if (first && cels[i] >> 16)
            winfree((void *)(uintptr_t)cels[i]);
    }
}

void __stdcall sprite_free(uint8_t *s)
{
    if (H(uint8_t *, HSL_HEAD) == s)
        H(uint8_t *, HSL_HEAD) = NULL;
    else if (H(uint8_t *, HSL_TAIL) == s)
        H(uint8_t *, HSL_TAIL) = NULL;
    if (H(uint8_t *, HSL_TOPMOST) == s)
        H(uint8_t *, HSL_TOPMOST) = NULL;
    if (S(void *, s, SP_PARTS))
        LocalFree(S(void *, s, SP_PARTS));
    if (S(void *, s, SP_QUEUE))
        winfree(S(void *, s, SP_QUEUE));
    if (S(void *, s, SP_CELS))
        LocalFree(S(void *, s, SP_CELS));
    if (S(void *, s, SP_1C))
        LocalFree(S(void *, s, SP_1C));
    if (S(void *, s, SP_INFO))
        winfree(S(void *, s, SP_INFO));
    LocalFree(s);
}

HLOCAL __stdcall local_copy(HLOCAL h)
{
    HLOCAL c;
    SIZE_T n;
    if (!h)
        return NULL;
    n = LocalSize(h);
    c = LocalAlloc(LPTR, n);
    if (c) {
        void *src = LocalLock(h), *dst = LocalLock(c);
        memcpy(dst, src, n);
        LocalUnlock(h);
        LocalUnlock(c);
    }
    return c;
}

/* ---- creating and destroying ---- */

/* The bounds from cel 0: centered on the position, moved by the cel's offset */
int __stdcall sprite_bounds_from_first_cel(uint8_t *s)
{
    uint8_t *bmp = *S(uint8_t **, s, SP_CELS);
    S(uint8_t *, s, SP_BITMAP) = bmp;
    S(int32_t, s, SP_RECT) = S(int32_t, s, SP_X) - (S(int32_t, bmp, 4) - 1) / 2 + S(int32_t, bmp, 0x14);
    S(int32_t, s, SP_RECT + 8) = S(int32_t, s, SP_RECT) + S(int32_t, bmp, 4);
    S(int32_t, s, SP_RECT + 4) = S(int32_t, s, SP_Y) - (S(int32_t, bmp, 0xc) - 1) / 2 + S(int32_t, bmp, 0x18);
    S(int32_t, s, SP_RECT + 12) = S(int32_t, s, SP_RECT + 4) + S(int32_t, bmp, 0xc);
    if (S(uint32_t, s, SP_SCROLLS))
        OffsetRect((RECT *)(s + SP_RECT), H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
    return 1;
}

uint8_t *__stdcall SpriteCreate(uint8_t *hsl, int32_t level, uint32_t bitmap)
{
    uint8_t *s;
    g_hsl = hsl;
    s = sprite_new(level);
    if (!s) {
        DebugStr("SpriteCreate: out of memory");
        return NULL;
    }
    S(uint32_t, s, SP_POSITIONED) = 1;
    S(uint32_t, s, SP_MERGE) = H(uint32_t, HSL_DEFAULT_MERGE);
    if (bitmap) {
        uint32_t *cels = LocalAlloc(LPTR, 4);
        if (!cels) {
            DebugStr("SpriteCreate: out of memory");
            return (uint8_t *)(uintptr_t)SpriteDestroy(hsl, s, 0, 0);
        }
        S(uint32_t *, s, SP_CELS) = cels;
        S(uint32_t, s, SP_FORWARD) = 1;
        S(uint32_t, s, SP_NCELS) = 1;
        cels[0] = bitmap;
        if (!sprite_bounds_from_first_cel(s))
            return (uint8_t *)(uintptr_t)SpriteDestroy(hsl, s, 0, 0);
    }
    return s;
}

/* hide: hide it first if shown; free_bitmaps: free its allocated cels */
int __stdcall SpriteDestroy(uint8_t *hsl, uint8_t *s, int hide, int free_bitmaps)
{
    g_hsl = hsl;
    if (sprite_check(s) < 0) {
        OutputDebugStringA("trying to delete an invalid sprite\n");
        return 0;
    }
    if (S(uint32_t, s, SP_VISIBLE) && hide)
        SpriteHide(hsl, s, 1);
    if (free_bitmaps)
        sprite_free_cels(s);
    if (H(uint8_t *, HSL_CURRENT) == s) {
        H(uint8_t *, HSL_CURRENT) = S(uint8_t *, s, SP_APREV);
        if (!H(uint8_t *, HSL_CURRENT))
            H(uint8_t *, HSL_CURRENT) = NULL;
    }
    S(uint8_t *, S(uint8_t *, s, SP_NEXT), SP_PREV) = S(uint8_t *, s, SP_PREV);
    S(uint8_t *, S(uint8_t *, s, SP_PREV), SP_NEXT) = S(uint8_t *, s, SP_NEXT);
    /* original: a debug assertion when the sprite was the last one */
    if (S(uint8_t *, s, SP_ANEXT))
        S(uint8_t *, S(uint8_t *, s, SP_ANEXT), SP_APREV) = S(uint8_t *, s, SP_APREV);
    if (S(uint8_t *, s, SP_APREV))
        S(uint8_t *, S(uint8_t *, s, SP_APREV), SP_ANEXT) = S(uint8_t *, s, SP_ANEXT);
    S(uint8_t *, s, SP_ANEXT) = S(uint8_t *, s, SP_APREV) = NULL;
    S(uint8_t *, s, SP_NEXT) = S(uint8_t *, s, SP_PREV) = NULL;
    sprite_free(s);
    H(uint32_t, HSL_COUNT)--;
    return 0;
}

void __stdcall SpriteDestroyAll(uint8_t *hsl, int hide)
{
    uint8_t *s;
    g_hsl = hsl;
    for (s = H(uint8_t *, HSL_HEAD); s;) {
        uint8_t *next;
        if (hide && S(uint32_t, s, SP_VISIBLE))
            SpriteHide(hsl, s, 1);
        next = S(uint8_t *, s, SP_NEXT);
        sprite_free(s);
        s = next;
    }
    H(void *, HSL_CURRENT) = NULL;
    H(void *, HSL_TAIL) = NULL;
    H(void *, HSL_HEAD) = NULL;
    H(uint32_t, HSL_COUNT) = 0;
    H(uint32_t, HSL_ANIMATED) = 0;
}

/* original: copies the whole block, list links and the info block pointer included */
uint8_t *__stdcall SpriteCopy(uint8_t *hsl, uint8_t *src)
{
    uint8_t *s;
    HLOCAL cels;
    g_hsl = hsl;
    s = sprite_new(S(int32_t, src, SP_LEVEL));
    if (!s)
        return NULL;
    cels = local_copy(S(HLOCAL, src, SP_CELS));
    if (!cels)
        return (uint8_t *)(uintptr_t)SpriteDestroy(hsl, s, 0, 0);
    memcpy(s, src, SP_SIZE);
    S(HLOCAL, s, SP_CELS) = cels;
    S(void *, s, SP_QUEUE_CUR) = S(void *, s, SP_QUEUE) = S(void *, s, SP_QUEUE_END) = NULL;
    if (S(uint8_t *, src, SP_QUEUE) &&
        !SpriteSetQueue(hsl, s, S(uint8_t *, src, SP_QUEUE),
                        (uint32_t)(S(uint8_t *, src, SP_QUEUE_END) - S(uint8_t *, src, SP_QUEUE))))
        return (uint8_t *)(uintptr_t)SpriteDestroy(hsl, s, 0, 0);
    S(uint32_t, s, SP_VISIBLE) = 0;
    S(uint32_t, s, SP_SCROLLS) = S(uint32_t, src, SP_SCROLLS);
    S(uint32_t, s, SP_MERGE) = H(uint32_t, HSL_DEFAULT_MERGE);
    return s;
}

/* ---- cels ---- */

static void cels_added(uint8_t *s, uint32_t n)
{
    if (S(uint32_t, s, SP_FORWARD) && S(uint32_t, s, SP_NCELS) - 1 == S(uint32_t, s, SP_LAST))
        S(uint32_t, s, SP_LAST) += n;
    else if (!S(uint32_t, s, SP_FORWARD) && S(uint32_t, s, SP_NCELS) - 1 == S(uint32_t, s, SP_FIRST))
        S(uint32_t, s, SP_FIRST) += n;
}

int __stdcall SpriteAppendBitmap(uint8_t *hsl, uint8_t *s, uint32_t bitmap)
{
    uint32_t *cels;
    g_hsl = hsl;
    cels = S(void *, s, SP_CELS)
               ? LocalReAlloc(S(HLOCAL, s, SP_CELS), S(uint32_t, s, SP_NCELS) * 4 + 4, LMEM_MOVEABLE | LMEM_ZEROINIT)
               : LocalAlloc(LPTR, 4);
    if (!cels) {
        DebugStr("SpriteAppendBitmap: out of memory");
        return 0;
    }
    cels[S(uint32_t, s, SP_NCELS)] = bitmap;
    S(uint32_t *, s, SP_CELS) = cels;
    S(uint32_t, s, SP_BITMAP) = cels[0];
    if (!S(uint32_t, s, SP_NCELS)) {
        if (!sprite_bounds_from_first_cel(s))
            return 0;
        S(uint32_t, s, SP_LAST) = 1; /* original: 1, not 0, for the first cel */
        S(uint32_t, s, SP_FORWARD) = 1;
    } else {
        cels_added(s, 1);
    }
    S(uint32_t, s, SP_LOOPS) = S(uint32_t, s, SP_FIRST) != S(uint32_t, s, SP_LAST);
    S(uint32_t, s, SP_NCELS)++;
    return 1;
}

int __stdcall SpriteAppendBitmaps(uint8_t *hsl, uint8_t *s, const uint32_t *bitmaps, uint32_t n)
{
    uint32_t *cels;
    g_hsl = hsl;
    cels = S(void *, s, SP_CELS)
               ? LocalReAlloc(S(HLOCAL, s, SP_CELS), (S(uint32_t, s, SP_NCELS) + n) * 4, LMEM_MOVEABLE | LMEM_ZEROINIT)
               : LocalAlloc(LPTR, n << 2);
    if (!cels) {
        DebugStr("SpriteAppendBitmaps: out of memory");
        return 0;
    }
    S(uint32_t *, s, SP_CELS) = cels;
    S(uint32_t, s, SP_BITMAP) = cels[0];
    for (uint32_t i = 0; i < n; i++)
        cels[S(uint32_t, s, SP_NCELS) + i] = bitmaps[i];
    if (!S(uint32_t, s, SP_NCELS)) {
        if (!sprite_bounds_from_first_cel(s))
            return 0;
        S(uint32_t, s, SP_LAST) = n - 1;
        S(uint32_t, s, SP_FORWARD) = 1;
    } else {
        cels_added(s, n);
    }
    S(uint32_t, s, SP_NCELS) += n;
    S(uint32_t, s, SP_LOOPS) = S(uint32_t, s, SP_FIRST) != S(uint32_t, s, SP_LAST);
    return 1;
}

/* Sets the cel range first..last (backwards if last < first) and the current cel;
 * returns the number of cels in the range */
int __stdcall sprite_set_cels(uint8_t *s, uint32_t cel, uint32_t first, uint32_t last)
{
    if (first >= S(uint32_t, s, SP_NCELS) || last >= S(uint32_t, s, SP_NCELS))
        return 0;
    S(uint32_t, s, SP_FIRST) = first;
    S(uint32_t, s, SP_LAST) = last;
    S(uint32_t, s, SP_LOOPS) = last != first;
    S(uint32_t, s, SP_UPDATED) = 1;
    S(uint32_t, s, SP_FORWARD) = first <= last;
    if (!S(uint32_t, s, SP_FORWARD)) {
        S(uint32_t, s, SP_CEL) = (cel < last || first < cel) ? first : cel;
        return (int)(first - last + 1);
    }
    S(uint32_t, s, SP_CEL) = (cel < first || last < cel) ? first : cel;
    return (int)(last - first + 1);
}

/* New bounds from the current cel and the position, flips and scrolling */
void __stdcall sprite_update_bounds(uint8_t *s)
{
    uint8_t *bmp;
    RECT old;
    int w1, h1;
    if (!S(uint32_t, s, SP_POSITIONED))
        return;
    bmp = S(uint8_t *, s, SP_BITMAP);
    w1 = S(int32_t, bmp, 4) - 1;
    h1 = S(int32_t, bmp, 0xc) - 1;
    memcpy(&old, s + SP_RECT, sizeof old);
    S(int32_t, s, SP_RECT) = S(int32_t, s, SP_X) - w1 / 2 +
                             (S(uint8_t, s, SP_FLIP_X) ? -S(int32_t, bmp, 0x14) : S(int32_t, bmp, 0x14));
    S(int32_t, s, SP_RECT + 8) = S(int32_t, s, SP_RECT) + S(int32_t, bmp, 4);
    S(int32_t, s, SP_RECT + 4) = S(int32_t, s, SP_Y) - h1 / 2 +
                                 (S(uint8_t, s, SP_FLIP_Y) ? -S(int32_t, bmp, 0x18) : S(int32_t, bmp, 0x18));
    S(int32_t, s, SP_RECT + 12) = S(int32_t, s, SP_RECT + 4) + S(int32_t, bmp, 0xc);
    if (S(uint32_t, s, SP_SCROLLS))
        OffsetRect((RECT *)(s + SP_RECT), H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
    if (S(BoundsProc, S(uint8_t *, s, SP_INFO), INFO_BOUNDS_PROC))
        S(BoundsProc, S(uint8_t *, s, SP_INFO), INFO_BOUNDS_PROC)(g_hsl, s, (RECT *)(s + SP_RECT));
    S(uint32_t, s, SP_MOVED) = intersect_rect(&old, &old, (RECT *)(s + SP_RECT));
}

/* Positioning on (1) or off (0): off keeps the position in the info block */
void __stdcall sprite_set_positioned(uint8_t *s, uint32_t on, int update)
{
    uint8_t *info = S(uint8_t *, s, SP_INFO);
    if (S(uint32_t, s, SP_POSITIONED) == on)
        return;
    S(uint32_t, s, SP_POSITIONED) = on;
    if (!on) {
        S(int32_t, info, 0) = S(int32_t, s, SP_X);
        S(int32_t, info, 4) = S(int32_t, s, SP_Y);
        S(uint32_t, info, 0x40) = S(uint32_t, s, SP_FLIP_X);
    } else {
        S(int32_t, s, SP_X) = S(int32_t, info, 0);
        S(int32_t, s, SP_Y) = S(int32_t, info, 4);
        S(uint32_t, s, SP_FLIP_X) = S(uint32_t, info, 0x40);
        if (update) {
            if (!S(uint32_t, s, SP_NPARTS))
                sprite_update_bounds(s);
            else
                parts_update_bounds(s);
            sprite_reposition(s);
        }
    }
}

/* Shows the next cel of the range, wrapping around */
void __stdcall sprite_next_cel(uint8_t *s)
{
    if (S(uint32_t, s, SP_CALLBACK))
        sprite_callback(s);
    S(uint32_t, s, SP_NPARTS) = 0;
    S(uint32_t, s, SP_SHOWN_CEL) = S(uint32_t, s, SP_CEL);
    if (!S(uint32_t, s, SP_FORWARD)) {
        S(int32_t, s, SP_CEL)--;
        if (S(int32_t, s, SP_CEL) < S(int32_t, s, SP_LAST))
            S(uint32_t, s, SP_CEL) = S(uint32_t, s, SP_FIRST);
    } else {
        S(uint32_t, s, SP_CEL)++;
        if (S(uint32_t, s, SP_LAST) < S(uint32_t, s, SP_CEL))
            S(uint32_t, s, SP_CEL) = S(uint32_t, s, SP_FIRST);
    }
    S(uint32_t, s, SP_BITMAP) = S(uint32_t *, s, SP_CELS)[S(uint32_t, s, SP_SHOWN_CEL)];
    sprite_update_bounds(s);
    S(uint32_t, s, SP_UPDATED) = 0;
}

/* A delay, or an index into the delay table */
int __cdecl sprite_delay(int value, char indexed)
{
    return indexed ? H(int32_t *, HSL_DELAYS)[value] : value;
}

/* The queue's current wait command: starts the wait, then says when it is over */
int __stdcall sprite_wait_done(uint8_t *s)
{
    uint8_t *q = S(uint8_t *, s, SP_QUEUE_CUR);
    DWORD now = timeGetTime();
    if (!q[6]) {
        int32_t delay;
        q[6] = 1;
        if (!S(uint32_t, s, 0xd4) || !S(uint32_t, s, SP_NEXT_MOVE))
            S(uint32_t, s, SP_NEXT_MOVE) = now;
        memcpy(&delay, q + 2, 4);
        S(uint32_t, s, SP_NEXT_MOVE) += (uint32_t)sprite_delay(delay, (char)q[7]);
        return 0;
    }
    return S(uint32_t, s, SP_NEXT_MOVE) <= now;
}

/* ---- animation ---- */

void __cdecl sprite_service(void)
{
    uint8_t *hsl = g_hsl;
    H(SpriteServiceFn, HSL_SERVICE)(H(uint32_t, HSL_SERVICE_ARG));
    g_hsl = hsl;
}

uint32_t __stdcall sprite_done(uint8_t *s)
{
    uint8_t *hsl = g_hsl;
    uint32_t r = 0;
    if (H(DoneProc, HSL_DONE_PROC))
        r = H(DoneProc, HSL_DONE_PROC)(s, H(uint32_t, HSL_DONE_ARG));
    g_hsl = hsl;
    return r;
}

/* One animation step for every sprite, in list order. A sprite on the interface panel
 * marks its rectangle dirty before and after it moves. */
void __stdcall SpriteAnimate(uint8_t *hsl)
{
    int stop = 0, n = 0;
    g_hsl = hsl;
    if (H(void *, HSL_SERVICE))
        sprite_service();
    H(uint32_t, HSL_ANIMATING)++;
    if (!H(uint32_t, HSL_COUNT)) {
        H(uint32_t, HSL_ANIMATING)--;
        return;
    }
    while (H(uint8_t *, HSL_CURRENT)) {
        uint8_t *s = H(uint8_t *, HSL_CURRENT);
        n++;
        if (stop) {
            H(uint32_t, HSL_ANIMATING)--;
            return;
        }
        if (S(void *, s, SP_QUEUE_CUR) && S(uint32_t, s, SP_ENABLED) && !S(uint32_t, s, SP_PAUSED)) {
            int panel = H(uint32_t, HSL_DIRECTDRAW) && !H(uint32_t, HSL_B5C) && H(uint32_t, HSL_IFACE_ON) &&
                        !S(uint32_t, s, SP_SCROLLS) && S(int32_t, s, SP_RECT + 12) > H(int32_t, HSL_VIEW_BOTTOM);
            uint32_t moved;
            if (panel)
                dirty_add_if_shown(s);
            moved = sprite_step(s);
            if (panel)
                dirty_add_if_shown(H(uint8_t *, HSL_CURRENT));
            stop = (int)sprite_done(H(uint8_t *, HSL_CURRENT));
            if (moved)
                sprite_redraw(H(uint8_t *, HSL_CURRENT));
            if (H(void *, HSL_SERVICE))
                sprite_service();
            if (!H(uint8_t *, HSL_CURRENT))
                break;
        }
        H(uint8_t *, HSL_CURRENT) = S(uint8_t *, H(uint8_t *, HSL_CURRENT), SP_ANEXT);
    }
    H(uint32_t, HSL_ANIMATED) = (uint32_t)n;
    H(uint8_t *, HSL_CURRENT) = S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_ANEXT);
    H(uint32_t, HSL_ANIMATING)--;
}

void __stdcall sprite_set_paused(uint8_t *s, uint8_t pause)
{
    DWORD now;
    if (pause == S(uint32_t, s, SP_PAUSED))
        return;
    now = timeGetTime();
    if (!pause)
        S(uint32_t, s, SP_NEXT_MOVE) += now - S(uint32_t, S(uint8_t *, s, SP_INFO), INFO_PAUSED_AT);
    else
        S(uint32_t, S(uint8_t *, s, SP_INFO), INFO_PAUSED_AT) = now;
    S(uint32_t, s, SP_PAUSED) = pause;
}

/* s = NULL: all sprites */
void __stdcall SpritePause(uint8_t *hsl, uint8_t *s, int pause)
{
    g_hsl = hsl;
    pause = pause != 0;
    if (s) {
        sprite_set_paused(s, (uint8_t)pause);
        return;
    }
    if (H(uint32_t, HSL_COUNT) && H(void *, HSL_HEAD)) {
        uint8_t *tail = H(uint8_t *, HSL_TAIL);
        for (uint8_t *p = S(uint8_t *, H(uint8_t *, HSL_HEAD), SP_NEXT); p != tail; p = S(uint8_t *, p, SP_NEXT))
            sprite_set_paused(p, (uint8_t)pause);
    }
}

/* ---- position, speed ---- */

int __stdcall SpriteGetBounds(uint8_t *hsl, uint8_t *s, RECT *r)
{
    g_hsl = hsl;
    if (!s)
        return 0;
    memcpy(r, s + SP_RECT, sizeof *r);
    if (S(uint32_t, s, SP_SCROLLS))
        OffsetRect(r, -H(int32_t, HSL_SCROLL_X), -H(int32_t, HSL_SCROLL_Y));
    return 1;
}

int __stdcall SpriteGetPosition(uint8_t *s, int32_t *pos)
{
    if (s) {
        pos[0] = S(int32_t, s, SP_X);
        pos[1] = S(int32_t, s, SP_Y);
    }
    return s != NULL;
}

/* Moves the sprite and its current path command by (dx, dy) */
void __stdcall SpriteOffset(uint8_t *hsl, uint8_t *s, int dx, int dy)
{
    int16_t *q;
    g_hsl = hsl;
    sprite_move(s, dx, dy, 1, 1);
    q = S(int16_t *, s, SP_QUEUE_CUR);
    if (!q)
        return;
    if (q[0] == 5) { /* a line: both ends */
        S(int32_t, q, 0x24) += dx;
        S(int32_t, q, 0x28) += dy;
        S(int32_t, q, 0x2c) += dx;
        S(int32_t, q, 0x30) += dy;
    } else if (q[0] == 9) {
        S(int32_t, q, 0x1c) += dx;
        S(int32_t, q, 0x20) += dy;
    }
}

void __stdcall SpriteSetPosition(uint8_t *hsl, uint8_t *s, int x, int y, int redraw)
{
    g_hsl = hsl;
    sprite_move(s, x, y, redraw, 1);
}

void __stdcall SpriteSetMotion(uint8_t *s, int32_t ms)
{
    DWORD now;
    S(int32_t, s, SP_MOTION) = ms;
    now = timeGetTime();
    S(uint32_t, s, SP_NEXT_STEP) = S(uint32_t, s, SP_SPEED) + now;
    S(uint32_t, s, SP_NEXT_MOVE) = (uint32_t)ms + now;
}

void __stdcall SpriteSetSpeed(uint8_t *s, int32_t ms)
{
    DWORD now;
    S(int32_t, s, SP_SPEED) = ms;
    now = timeGetTime();
    S(uint32_t, s, SP_NEXT_STEP) = now + (uint32_t)ms;
    S(uint32_t, s, SP_NEXT_MOVE) = S(uint32_t, s, SP_MOTION) + now;
}

void __stdcall SpriteSetServiceProc(uint8_t *hsl, SpriteServiceFn proc, uint32_t arg)
{
    g_hsl = hsl;
    H(SpriteServiceFn, HSL_SERVICE) = proc;
    H(uint32_t, HSL_SERVICE_ARG) = arg;
}

/* ---- small getters and setters ---- */

void __stdcall SpriteBreakLoops(uint8_t *s, uint32_t v) { S(uint32_t, s, SP_BREAK_LOOPS) = v; }
void __stdcall SpriteCallBack(uint8_t *s, int on) { S(uint32_t, s, SP_CALLBACK) = on != 0; }
void __stdcall SpriteClearUpdate(uint8_t *s) { S(uint32_t, s, SP_UPDATED) = 0; }
void __stdcall SpriteEnable(uint8_t *s, uint32_t on) { S(uint32_t, s, SP_ENABLED) = on; }
uint32_t __stdcall SpriteGetExtraId(uint8_t *s) { return S(uint32_t, S(uint8_t *, s, SP_INFO), INFO_EXTRA_ID); }
uint32_t __stdcall SpriteGetId(uint8_t *s) { return S(uint32_t, s, SP_ID); }
int32_t __stdcall SpriteGetLevel(uint8_t *s) { return S(int32_t, s, SP_LEVEL); }
uint32_t __stdcall SpriteGetNumCels(uint8_t *s) { return S(uint32_t, s, SP_NCELS); }
uint32_t __stdcall SpriteGetUserData(uint8_t *s) { return S(uint32_t, S(uint8_t *, s, SP_INFO), INFO_USER); }
uint32_t __stdcall SpriteIsScrollable(uint8_t *s) { return S(uint32_t, s, SP_SCROLLS); }
uint32_t __stdcall SpriteIsVisible(uint8_t *s) { return S(uint32_t, s, SP_VISIBLE); }
int __stdcall SpriteIsValid(uint8_t *s) { return sprite_check(s) >= 0; }

HDC __stdcall SpriteGetDC(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(HDC, HSL_DC);
}

uint32_t __stdcall SpriteGetNumSprites(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_COUNT);
}

uint32_t __stdcall SpriteGetNumMsgSprites(uint8_t *hsl)
{
    g_hsl = hsl;
    return H(uint32_t, HSL_ANIMATED);
}

uint32_t __stdcall SpriteGetSprite(uint8_t *hsl, uint32_t unused)
{
    (void)unused;
    g_hsl = hsl;
    OutputDebugStringA("**** SpriteGetSprite UNIMPLEMENTED ****\r\n"); /* original */
    return 0;
}

int __stdcall SpriteGetCell(uint8_t *s, uint32_t i, uint32_t *out)
{
    int ok = i < S(uint32_t, s, SP_NCELS);
    if (ok)
        *out = S(uint32_t *, s, SP_CELS)[i];
    return ok;
}

void __stdcall SpriteGetInfo(uint8_t *s, uint32_t *info)
{
    info[1] = S(uint32_t, s, SP_BITMAP);
    info[0xb] = info[1] != 0;
    if (S(uint32_t, s, SP_NPARTS)) {
        for (uint32_t i = 0; i < S(uint32_t, s, SP_NPARTS); i++)
            info[i + 1] = *(uint32_t *)(S(uint8_t *, s, SP_PARTS) + i * 0x14);
        info[0xb] = S(uint32_t, s, SP_NPARTS);
    }
    info[9] = S(uint32_t, s, SP_ENABLED);
    info[10] = S(uint32_t, s, SP_VISIBLE);
    info[0xd] = S(uint32_t, s, SP_X);
    info[0xe] = S(uint32_t, s, SP_Y);
    memcpy(info + 0xf, s + SP_RECT, 16);
    info[0xc] = S(uint32_t, s, SP_ID);
    info[0] = S(uint32_t, s, SP_QUEUE_CUR);
    info[0x13] = S(uint32_t, s, SP_FIRST);
    info[0x14] = S(uint32_t, s, SP_LAST);
    info[0x15] = S(uint32_t, s, SP_CEL);
    info[0x16] = S(uint32_t, s, SP_FORWARD);
}

/* ---- drawing helpers (assembly in the original, called through the safe wrapper) ---- */

void __stdcall SpriteCopyRect(uint8_t *dst, uint8_t *src, int32_t *r)
{
    if (!r || !src || !dst)
        return;
    if (S(int32_t, src, 4) < r[2])
        r[2] = S(int32_t, src, 4);
    if (S(int32_t, src, 0xc) < r[3])
        r[3] = S(int32_t, src, 0xc);
    if (r[2] - r[0] > 0 && r[3] - r[1] > 0) {
        uint32_t a[9] = {(uint32_t)(uintptr_t)dst, (uint32_t)(uintptr_t)src, (uint32_t)(r[2] - r[0]),
                         (uint32_t)(r[3] - r[1]), (uint32_t)r[0], (uint32_t)r[1], (uint32_t)r[0],
                         (uint32_t)r[1], 0};
        orig_stdcall_unsafe(ORIG_PTR(0x100629f4), 9, a);
    }
}

uint32_t __stdcall SpriteCurvePoint(uint32_t a, uint32_t n, uint32_t c, uint32_t d, uint32_t e,
                                    uint32_t f, uint32_t g, uint32_t h, uint32_t i)
{
    if (n) {
        uint32_t args[9] = {a, n, c, d, e, f, g, h, i};
        orig_stdcall_unsafe(ORIG_PTR(0x10062c34), 9, args);
    }
    return 0;
}

int __stdcall SpriteLinePoint(uint32_t a, uint32_t n, uint32_t c, uint32_t d, uint32_t e,
                              uint32_t f, uint32_t g)
{
    if (n) {
        uint32_t args[7] = {a, n, c, d, e, f, g};
        orig_stdcall_unsafe(ORIG_PTR(0x10062ccc), 7, args);
    }
    return n != 0;
}
