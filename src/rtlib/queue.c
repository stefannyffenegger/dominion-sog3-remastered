/* The sprite engine's script queues: each sprite can run a list of commands (animate
 * cels, move along a line or curve, wait, loop, flip, show, hide, send a message to the
 * game, ...). Rewritten from the original (0x10053c50-0x10054da0).
 *
 * A command is a 16-bit type followed by its data, packed. Most values come with a flag
 * byte: when set, the value is an index into the game's variable table (sprite_delay).
 * Commands still original: 2, 5 (curve), 9 (line), 10. */
#include "orig.h"
#include "spritelib.h"
#include "queue.h"
#include "sprite.h"
#include "view.h"
#include "utils.h"
#include "blit.h"
#include "display.h"

#define g_hsl G(uint8_t *, 0x10089e24)
#define H(type, off) (*(type *)(g_hsl + (off)))
#define S(type, s, off) (*(type *)((uint8_t *)(s) + (off)))
#define C(type, c, off) (*(type *)((uint8_t *)(c) + (off)))

/* HSL fields */
#define HSL_CURRENT 0x008
#define HSL_MSG_PROC 0x01c  /* proc(message, sprite id) */
#define HSL_IN_MSG 0x9f4    /* a message is being handled */
#define HSL_NO_SEND_MSG 0x944
#define HSL_MSG 0xa20       /* copy of the message being sent */

/* sprite fields */
#define SP_QUEUE 0x28       /* start, current command, end */
#define SP_QUEUE_CUR 0x2c
#define SP_QUEUE_END 0x30
#define SP_INFO 0x34
#define SP_X 0x3c
#define SP_Y 0x40
#define SP_NEXT_STEP 0x64
#define SP_NEXT_MOVE 0x68
#define SP_SPEED 0x6c
#define SP_MOTION 0x70
#define SP_FLIP 0x7c
#define SP_QUEUE_SIZE 0x84
#define SP_NPARTS 0x90
#define SP_ID 0x94
#define SP_BREAK_LOOPS 0xa4
#define SP_MSG_PENDING 0xa8 /* cleared if the game changes the sprite during the message */
#define SP_CALLBACK 0xac
#define SP_MOVED 0xb4       /* the command moved or changed the sprite */
#define SP_ENABLED 0xb8
#define SP_RESTART 0xc8     /* start the next command after the message */
#define SP_CC 0xcc
#define SP_PRIORITY 0xd4
#define SP_POSITIONED 0xdc
#define INFO_FLIP 0x40

/* command sizes by type (filled by queue_init_sizes) */
#define g_cmd_size ((uint32_t *)ORIG_PTR(0x10088dc0))

typedef void(__stdcall *MsgProc)(void *, uint32_t);

/* original functions not rewritten yet */

/* value at off with its flag byte at flag */
#define VAL(c, off, flag) (uint32_t) sprite_delay(C(int32_t, c, off), C(char, c, flag))

/* After a message: if the game didn't change the sprite meanwhile, mark it enabled */
static int msg_done(uint8_t *s, uint8_t *hsl)
{
    if ((S(uint8_t *, hsl, HSL_CURRENT) == s || sprite_check(s) != -1) && S(uint32_t, s, SP_MSG_PENDING)) {
        S(uint32_t, s, SP_ENABLED) = 1;
        S(uint32_t, s, SP_MSG_PENDING) = 0;
        return 1;
    }
    return 0;
}

/* Tells the game that s finished its animation (message 0x13) */
void __stdcall sprite_callback(uint8_t *s)
{
    if (!H(MsgProc, HSL_MSG_PROC))
        return;
#pragma pack(push, 1)
    struct {
        uint16_t type;
        uint32_t id;
    } msg = {0x13, S(uint32_t, s, SP_ID)};
#pragma pack(pop)
    S(uint32_t, s, SP_ENABLED) = 0;
    S(uint32_t, s, SP_CALLBACK) = 0;
    S(uint32_t, s, SP_MSG_PENDING) = 1;
    uint8_t *hsl = g_hsl;
    H(MsgProc, HSL_MSG_PROC)(&msg, S(uint32_t, s, SP_ID));
    g_hsl = hsl;
    msg_done(s, hsl);
}

uint32_t __stdcall cmd_size(const uint16_t *c)
{
    if (*c == 2)
        return 0x2e - (8 - C(int32_t, c, 6)) * 4; /* up to 8 cels */
    if (*c == 0x15)
        return C(uint32_t, c, 2); /* message with its size */
    return g_cmd_size[*c];
}

/* Moves out of a line command so it takes `steps` steps at most */
void __stdcall line_steps(uint8_t *c)
{
    int32_t x0 = C(int32_t, c, 0x14), y0 = C(int32_t, c, 0x18), x1 = C(int32_t, c, 0x1c), y1 = C(int32_t, c, 0x20);
    uint32_t dx = x0 < x1 ? x1 - x0 : x0 - x1;
    uint32_t dy = y0 < y1 ? y1 - y0 : y0 - y1;
    uint32_t step = C(uint32_t, c, 0x28);
    if (!step)
        step = 1;
    if (dy <= dx)
        dy = dx;
    C(uint32_t, c, 0x28) = dy / step + (dy % step != 0);
}

void __cdecl queue_init_sizes(void)
{
    static const uint8_t sizes[25] = {0,   0x16, 0x2e, 2, 2,   0x3c, 0xe,  0x16, 2,   0x2c, 0xe, 8,   0xe,
                                      6,   6,    8,    0x12, 0xc, 6,  6,    2,    6,   0x22, 0x10, 0xc};
    for (int i = 1; i < 25; i++)
        g_cmd_size[i] = sizes[i];
}

/* Moves to the next command and prepares it; at the end, empties the queue */
int __stdcall cmd_start(uint8_t *s)
{
    uint8_t *c = cmd_next(s);
    S(uint8_t *, s, SP_QUEUE_CUR) = c;
    if (!c) {
        SpriteEmptyQueue(g_hsl, s);
        return 0;
    }
    switch (C(uint16_t, c, 0)) {
    case 1: { /* animate cels */
        uint32_t last = VAL(c, 6, 0x13), first = VAL(c, 2, 0x12), cel = VAL(c, 2, 0x12);
        C(int32_t, c, 0xa) = sprite_set_cels(s, cel, first, last);
        S(uint32_t, s, SP_MOVED) = 0;
        break;
    }
    case 2:
        S(uint32_t, s, SP_MOVED) = 0;
        C(uint8_t, c, 0xa) = 0;
        break;
    case 5: /* curve: from the current position through two points to a third */
        C(int32_t, c, 0x1c) = S(int32_t, s, SP_X);
        C(int32_t, c, 0x20) = S(int32_t, s, SP_Y);
        C(int32_t, c, 0x24) = VAL(c, 0xc, 7);
        C(int32_t, c, 0x28) = VAL(c, 0x10, 8);
        C(int32_t, c, 0x2c) = VAL(c, 0x14, 9);
        C(int32_t, c, 0x30) = VAL(c, 0x18, 0xa);
        if (C(char, c, 6))
            for (int i = 0; i < 4; i++)
                C(int32_t, c, 0x24 + i * 4) += S(int32_t, s, i & 1 ? SP_Y : SP_X);
        C(int32_t, c, 0x34) = 0;
        C(int32_t, c, 0x38) = VAL(c, 2, 0xb);
        break;
    case 9: /* line to a point */
        C(int32_t, c, 0x14) = S(int32_t, s, SP_X);
        C(int32_t, c, 0x18) = S(int32_t, s, SP_Y);
        C(int32_t, c, 0x1c) = VAL(c, 0xc, 8);
        C(int32_t, c, 0x20) = VAL(c, 0x10, 9);
        if (C(char, c, 6)) {
            C(int32_t, c, 0x1c) += S(int32_t, s, SP_X);
            C(int32_t, c, 0x20) += S(int32_t, s, SP_Y);
        }
        C(int32_t, c, 0x24) = 0;
        C(int32_t, c, 0x28) = VAL(c, 2, 0xa);
        if (C(char, c, 7))
            line_steps(c);
        break;
    case 0xb:
        C(uint8_t, c, 6) = 0;
        break;
    case 0x17:
        C(uint32_t, c, 6) = 0;
        break;
    }
    return 1;
}

/* The command after the current one, running loops (command 6) on the way */
uint8_t *__stdcall cmd_next(uint8_t *s)
{
    uint8_t *start = S(uint8_t *, s, SP_QUEUE), *c = S(uint8_t *, s, SP_QUEUE_CUR);
    c = c ? c + cmd_size((uint16_t *)c) : start;
    while (c < S(uint8_t *, s, SP_QUEUE_END)) {
        if (C(uint16_t, c, 0) != 6)
            return c;
        /* loop: count at +2 (reset from +6 when done), jump target at +10 */
        if (S(uint32_t, s, SP_BREAK_LOOPS) || C(int32_t, c, 2) == 1) {
            C(int32_t, c, 2) = C(int32_t, c, 6);
            c += cmd_size((uint16_t *)c);
        } else {
            if (C(int32_t, c, 2))
                C(int32_t, c, 2)--;
            c = start + C(int32_t, c, 10);
        }
    }
    return NULL;
}

/* Runs the current command one step; returns 0 when the sprite should wait */
uint32_t __stdcall sprite_step(uint8_t *s)
{
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    uint32_t r = 1;
    switch (C(uint16_t, c, 0)) {
    case 1:
        sprite_set_positioned(s, 1, 0);
        r = cmd_cels(s);
        break;
    case 2:
        if (!S(uint32_t, s, SP_POSITIONED))
            S(uint32_t, s, SP_CC) = 0;
        sprite_set_positioned(s, 1, 0);
        r = cmd_parts(s);
        break;
    case 4: /* draw into the background */
        comp_to_background(s, 0);
        break;
    case 5:
        sprite_set_positioned(s, 1, 0);
        r = cmd_curve(s);
        break;
    case 7: /* flip, then go on at once */
    case 0xc: /* jump, then go on at once */
        if (C(uint16_t, c, 0) == 7)
            cmd_flip(s);
        else
            cmd_jump(s, c);
        cmd_start(s);
        r = 0;
        if (S(uint32_t, s, SP_MOVED)) {
            sprite_reposition(s);
        } else if (S(uint8_t *, s, SP_QUEUE_CUR)) {
            S(uint32_t, s, SP_NEXT_MOVE) = 0;
            r = sprite_step(s);
        }
        break;
    case 8:
        SpriteHide(g_hsl, s, 1);
        break;
    case 9:
        sprite_set_positioned(s, 1, 0);
        r = cmd_line(s);
        break;
    case 10:
        sprite_set_positioned(s, 1, 0);
        r = cmd_10(s);
        break;
    case 0xb:
        r = sprite_wait_done(s);
        break;
    case 0xd: /* priority */
        S(uint32_t, s, SP_PRIORITY) = C(uint32_t, c, 2);
        S(uint32_t, s, SP_NEXT_STEP) = 0;
        S(uint32_t, s, SP_NEXT_MOVE) = 0;
        if (cmd_start(s))
            r = sprite_step(s);
        break;
    case 0xe:
        sprite_set_positioned(s, C(uint32_t, c, 2), 1);
        break;
    case 0xf:
        SpriteSetMotion(s, VAL(c, 2, 6));
        if (cmd_start(s))
            r = sprite_step(s);
        break;
    case 0x10:
        sprite_set_cels(s, VAL(c, 2, 0xe), VAL(c, 6, 0xf), VAL(c, 0xa, 0x10));
        if (cmd_start(s))
            r = sprite_step(s);
        break;
    case 0x11:
        SpriteSetSpeed(s, VAL(c, 2, 10));
        if (C(int32_t, c, 6))
            SpriteSetMotion(s, VAL(c, 6, 0xb));
        if (cmd_start(s))
            r = sprite_step(s);
        break;
    case 0x12:
        SpriteShow(g_hsl, s, C(uint32_t, c, 2), 1);
        break;
    case 0x14:
    case 0x16:
        if (!H(uint32_t, HSL_IN_MSG))
            cmd_send_msg(s, g_cmd_size[C(uint16_t, c, 0)]);
        r = 0;
        break;
    case 0x15:
        r = cmd_send_msg(s, C(uint32_t, c, 2));
        break;
    case 0x17:
        sprite_set_positioned(s, 1, 0);
        r = cmd_movie(s);
        break;
    case 0x18:
        cmd_level(s, c);
        if (cmd_start(s))
            r = sprite_step(s);
        break;
    }
    return r;
}

/* Command 7: set the flip, optionally moving by an offset */
void __stdcall cmd_flip(uint8_t *s)
{
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    uint32_t *flip = S(uint32_t, s, SP_POSITIONED) ? &S(uint32_t, s, SP_FLIP)
                                                    : &S(uint32_t, S(uint8_t *, s, SP_INFO), INFO_FLIP);
    uint32_t old = *flip;
    uint32_t fx = VAL(c, 2, 0xc), fy = VAL(c, 6, 0xd);
    *flip = (fy != 0) << 8 | (fx != 0);
    int dx = VAL(c, 0xe, 10), dy = VAL(c, 0x12, 0xb);
    if (dx == 0 && dy == 0) {
        if (*flip == old)
            return;
    } else {
        sprite_move(s, dx, dy, 1, 0);
    }
    if (S(uint32_t, s, SP_NPARTS))
        parts_update_bounds(s);
    S(uint32_t, s, SP_CC) = 0;
    S(uint32_t, s, SP_MOVED) = 1;
}

/* Command 0x18: level relative to another sprite (never implemented by the engine) */
void __stdcall cmd_level(uint8_t *s, uint8_t *c)
{
    uint32_t id = VAL(c, 2, 10);
    uint8_t *other = id ? sprite_find_id(id) : NULL;
    uint32_t v = VAL(c, 6, 0xb);
    sprite_set_level_relative(g_hsl, (uint32_t)(uintptr_t)s, (uint32_t)(uintptr_t)other, v);
}

/* Command 0xc: jump to a position */
void __stdcall cmd_jump(uint8_t *s, uint8_t *c)
{
    uint32_t redraw = C(uint8_t, c, 10);
    int y = VAL(c, 6, 0xc), x = VAL(c, 2, 0xb);
    sprite_move(s, x, y, redraw, 0);
    S(uint32_t, s, SP_MOVED) = 1;
}

/* Commands 0x14-0x16: send the command itself to the game as a message */
uint32_t __stdcall cmd_send_msg(uint8_t *s, uint32_t size)
{
    if (!H(MsgProc, HSL_MSG_PROC))
        return 1;
    if (H(uint32_t, HSL_IN_MSG) || H(uint32_t, HSL_NO_SEND_MSG))
        return 0;
    uint8_t was = H(uint8_t, HSL_IN_MSG);
    H(uint32_t, HSL_IN_MSG) = 1;
    memcpy(&H(uint8_t, HSL_MSG), S(void *, s, SP_QUEUE_CUR), size);
    S(uint32_t, s, SP_ENABLED) = 0;
    S(uint32_t, s, SP_MSG_PENDING) = 1;
    S(uint32_t, s, SP_RESTART) = 1;
    uint8_t *hsl = g_hsl;
    H(MsgProc, HSL_MSG_PROC)(g_hsl + HSL_MSG, S(uint32_t, s, SP_ID));
    g_hsl = hsl;
    S(uint32_t, hsl, HSL_IN_MSG) = was;
    if (msg_done(s, g_hsl)) {
        if (S(uint32_t, s, SP_RESTART))
            cmd_start(s);
        S(uint32_t, s, SP_RESTART) = 0;
    }
    return 0;
}

void __stdcall SpriteEmptyQueue(uint8_t *hsl, uint8_t *s)
{
    g_hsl = hsl;
    if (S(void *, s, SP_QUEUE))
        winfree(S(void *, s, SP_QUEUE));
    S(uint32_t, s, SP_POSITIONED) = 1;
    S(uint32_t, s, SP_NEXT_STEP) = S(uint32_t, s, SP_NEXT_MOVE) = 0;
    S(uint32_t, s, SP_SPEED) = S(uint32_t, s, SP_MOTION) = 0;
    S(uint32_t, s, SP_QUEUE) = S(uint32_t, s, SP_QUEUE_CUR) = S(uint32_t, s, SP_QUEUE_END) = 0;
    S(uint32_t, s, SP_PRIORITY) = S(uint32_t, s, SP_RESTART) = S(uint32_t, s, SP_BREAK_LOOPS) = 0;
}

void __stdcall SpriteSetMessageProc(uint8_t *hsl, uint32_t proc)
{
    g_hsl = hsl;
    H(uint32_t, HSL_MSG_PROC) = proc;
}

/* Gives s a copy of the command list and starts it */
int __stdcall SpriteSetQueue(uint8_t *hsl, uint8_t *s, const void *cmds, SIZE_T size)
{
    g_hsl = hsl;
    if (!cmds || !size)
        return 0;
    uint8_t *q = winmalloc(size, 1);
    if (!q) {
        DebugStr("SpriteSetQueue: failed");
        return 0;
    }
    SpriteEmptyQueue(hsl, s);
    memcpy(q, cmds, size);
    S(uint8_t *, s, SP_QUEUE) = q;
    S(uint32_t, s, SP_QUEUE_SIZE) = size;
    S(uint8_t *, s, SP_QUEUE_END) = q + size;
    cmd_start(s);
    return 1;
}

/* ---- messages: building queue commands from C (0x10058400-0x10059a70) ----
 * SpritePostMessage appends the command to the sprite's queue (and starts the queue if
 * it isn't running); SpriteSendMessage runs it at once, replacing the queue. A message is
 * a type followed by its values. */

#define SP_DIRTY_BOTTOM 0x50  /* bounds bottom */
#define SP_SCROLLS 0xe4
#define INFO_LOOP_START 0x3c   /* queue offset where the next loop jumps to */
#define HSL_DIRECTDRAW 0x954
#define HSL_B5C 0xb5c
#define HSL_IFACE_ON 0xb68
#define HSL_VIEW2_BOTTOM 0x920
#define INFO(type, s, off) S(type, S(uint8_t *, s, SP_INFO), off)

static void q_start(uint8_t *s)
{
    if (!S(uint8_t *, s, SP_QUEUE_CUR))
        cmd_start(s);
}

/* Empties the queue without freeing it */
void __stdcall queue_rewind(uint8_t *s)
{
    S(uint32_t, s, SP_QUEUE_END) = S(uint32_t, s, SP_QUEUE);
    S(uint32_t, s, SP_QUEUE_CUR) = S(uint32_t, s, SP_QUEUE_END);
    S(uint32_t, s, SP_RESTART) = 0;
    S(uint32_t, s, SP_BREAK_LOOPS) = 0;
}

/* Appends a command of `type` (size from the table unless given), growing the queue in
 * 128-byte steps. original: a queue with no current command gets its current pointer
 * moved by the reallocation anyway. */
uint8_t *__stdcall cmd_append(uint8_t *s, uint32_t type, uint32_t size)
{
    uint32_t cap = S(uint32_t, s, SP_QUEUE_SIZE);
    uint32_t used = S(uint32_t, s, SP_QUEUE_END) - S(uint32_t, s, SP_QUEUE);
    uint32_t need = used + (size ? size : g_cmd_size[type]);
    if (cap < need) {
        uint32_t ncap = (need + 0x7f) & ~0x7fu;
        uint8_t *q = winrealloc(S(void *, s, SP_QUEUE), ncap, 0);
        if (!q)
            return NULL;
        S(uint32_t, s, SP_QUEUE_SIZE) = ncap;
        if (cap < ncap)
            memset(q + cap, 0, ncap - cap);
        S(uint32_t, s, SP_QUEUE_CUR) = S(uint32_t, s, SP_QUEUE_CUR) - S(uint32_t, s, SP_QUEUE) + (uint32_t)(uintptr_t)q;
        S(uint8_t *, s, SP_QUEUE) = q;
        S(uint8_t *, s, SP_QUEUE_END) = q + used;
    }
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_END);
    *(uint16_t *)c = (uint16_t)type;
    S(uint8_t *, s, SP_QUEUE_END) = S(uint8_t *, s, SP_QUEUE) + need;
    return c;
}

uint8_t *__stdcall cmd_alloc(uint8_t *s, uint32_t type, uint32_t size)
{
    if (S(void *, s, SP_QUEUE))
        return cmd_append(s, type, size);
    uint32_t n = size ? size : g_cmd_size[type], cap = (n + 0x7f) & ~0x7fu;
    uint8_t *q = winmalloc(cap, 1);
    if (!q)
        return NULL;
    S(uint8_t *, s, SP_QUEUE) = q;
    S(uint32_t, s, SP_QUEUE_SIZE) = cap;
    S(uint32_t, s, SP_QUEUE_CUR) = 0;
    S(uint8_t *, s, SP_QUEUE_END) = q + n;
    *(uint16_t *)q = (uint16_t)type;
    return q;
}

/* Runs command c now; empties the queue when it finished */
void __stdcall msg_run(uint8_t *s, uint8_t *c)
{
    S(uint8_t *, s, SP_QUEUE_CUR) = c;
    if (sprite_step(s))
        SpriteEmptyQueue(g_hsl, s);
}

#define ALLOC(type, size)                                                                          \
    uint8_t *c = cmd_alloc(s, type, size);                                                         \
    if (!c)                                                                                        \
        return 0

int __stdcall msg_cels(uint8_t *s, const uint32_t *d, int now)
{
    if (now)
        queue_rewind(s);
    ALLOC(1, 0);
    C(uint32_t, c, 2) = d[0];
    C(uint32_t, c, 6) = d[1];
    C(uint32_t, c, 0xe) = d[2];
    if (!now) {
        q_start(s);
    } else {
        C(int32_t, c, 0xa) = sprite_set_cels(s, C(uint32_t, c, 2), C(uint32_t, c, 2), C(uint32_t, c, 6));
        msg_run(s, c);
    }
    return 1;
}

int __stdcall msg_type2(uint8_t *s, const uint32_t *d, uint32_t count, int now)
{
    if (count - 2 < 3 || 9 < count - 2)
        return 0;
    if (now)
        queue_rewind(s);
    ALLOC(2, 0x2e - (8 - (count - 3)) * 4);
    for (uint32_t k = 0; k < count - 3; k++)
        C(uint32_t, c, 0xe + k * 4) = *d++;
    C(uint32_t, c, 2) = *d;
    C(uint32_t, c, 6) = (count - 3) & 0xff;
    C(uint8_t, c, 0xa) = 0;
    if (!now)
        q_start(s);
    else
        msg_run(s, c);
    return 1;
}

/* marks where the next loop jumps back to */
int __stdcall msg_loop_start(uint8_t *s, int now)
{
    if (!now)
        INFO(uint32_t, s, INFO_LOOP_START) = S(uint32_t, s, SP_QUEUE_END) - S(uint32_t, s, SP_QUEUE);
    return 1;
}

int __stdcall msg_to_background(uint8_t *s, int now)
{
    if (!now) {
        ALLOC(4, 0);
        (void)c;
        q_start(s);
    } else {
        comp_to_background(s, 0);
    }
    return 1;
}

int __stdcall msg_curve(uint8_t *s, const int32_t *d, int now)
{
    if (now)
        queue_rewind(s);
    ALLOC(5, 0);
    if (!now) {
        C(int32_t, c, 0xc) = d[0];
        C(int32_t, c, 0x10) = d[1];
        C(int32_t, c, 0x14) = d[2];
        C(int32_t, c, 0x18) = d[3];
        C(int32_t, c, 2) = d[4];
        C(uint8_t, c, 6) = d[5] != 0;
        q_start(s);
    } else {
        C(int32_t, c, 0x1c) = S(int32_t, s, SP_X);
        C(int32_t, c, 0x20) = S(int32_t, s, SP_Y);
        for (int i = 0; i < 4; i++)
            C(int32_t, c, 0x24 + i * 4) = d[i];
        C(int32_t, c, 0x38) = d[4];
        if (d[5])
            for (int i = 0; i < 4; i++)
                C(int32_t, c, 0x24 + i * 4) += S(int32_t, s, i & 1 ? SP_Y : SP_X);
        C(int32_t, c, 0x34) = 0;
        msg_run(s, c);
    }
    return 1;
}

int __stdcall msg_loop(uint8_t *s, const uint32_t *d, uint32_t count, int now)
{
    if (now)
        return 1;
    ALLOC(6, 0);
    C(uint32_t, c, 2) = count ? d[0] : 0;
    C(uint32_t, c, 6) = C(uint32_t, c, 2);
    C(uint32_t, c, 10) = INFO(uint32_t, s, INFO_LOOP_START);
    q_start(s);
    return 1;
}

int __stdcall msg_flip(uint8_t *s, const int32_t *d, int now)
{
    if (!now) {
        ALLOC(7, 0);
        C(uint32_t, c, 2) = d[0] != 0;
        C(uint32_t, c, 6) = d[1] != 0;
        C(uint32_t, c, 0x12) = 0;
        C(uint32_t, c, 0xe) = 0;
        q_start(s);
        return 1;
    }
    uint32_t old = S(uint32_t, s, SP_FLIP);
    uint32_t *flip = S(uint32_t, s, SP_POSITIONED) ? &S(uint32_t, s, SP_FLIP) : &INFO(uint32_t, s, INFO_FLIP);
    *flip = (d[1] != 0) << 8 | (d[0] != 0);
    if (*flip == old)
        return 1;
    if (S(uint32_t, s, SP_NPARTS))
        parts_update_bounds(s);
    sprite_reposition(s);
    return 1;
}

int __stdcall msg_hide(uint8_t *s, int now)
{
    if (!now) {
        ALLOC(8, 0);
        (void)c;
        q_start(s);
    } else {
        SpriteHide(g_hsl, s, 1);
    }
    return 1;
}

int __stdcall msg_level(uint8_t *s, const uint32_t *d, int now)
{
    uint32_t id = d[0], v = d[1];
    if (!now) {
        ALLOC(0x18, 0);
        C(uint32_t, c, 2) = id;
        C(uint32_t, c, 6) = v;
        q_start(s);
    } else {
        uint8_t *other = id ? sprite_find_id(id) : NULL;
        sprite_set_level_relative(g_hsl, (uint32_t)(uintptr_t)s, (uint32_t)(uintptr_t)other, v);
    }
    return 1;
}

int __stdcall msg_line(uint8_t *s, const int32_t *d, int now)
{
    if (now)
        queue_rewind(s);
    ALLOC(9, 0);
    if (!now) {
        C(int32_t, c, 0xc) = d[0];
        C(int32_t, c, 0x10) = d[1];
        C(int32_t, c, 2) = d[2];
        C(uint8_t, c, 7) = d[3] != 0;
        C(uint8_t, c, 6) = d[4] != 0;
        q_start(s);
    } else {
        C(int32_t, c, 0x14) = S(int32_t, s, SP_X);
        C(int32_t, c, 0x18) = S(int32_t, s, SP_Y);
        C(int32_t, c, 0x1c) = d[0];
        C(int32_t, c, 0x20) = d[1];
        C(int32_t, c, 0x28) = d[2];
        if (d[4]) {
            C(int32_t, c, 0x1c) += S(int32_t, s, SP_X);
            C(int32_t, c, 0x20) += S(int32_t, s, SP_Y);
        }
        if (d[3])
            line_steps(c);
        C(int32_t, c, 0x24) = 0;
        msg_run(s, c);
    }
    return 1;
}

int __stdcall msg_type10(uint8_t *s, const uint32_t *d, uint32_t count, int now)
{
    if (count != 5)
        return 0;
    if (now)
        queue_rewind(s);
    ALLOC(10, 0);
    C(uint32_t, c, 6) = d[0];
    C(uint32_t, c, 10) = d[1];
    C(uint8_t, c, 2) = d[2] != 0;
    if (!now)
        q_start(s);
    else
        msg_run(s, c);
    return 1;
}

/* type 0x16: a message for the game, posted even "now" */
int __stdcall msg_game(uint8_t *s, const uint32_t *d, uint32_t count, int now)
{
    if (count < 3 || 9 < count)
        return 0;
    if (now)
        queue_rewind(s);
    ALLOC(0x16, 0);
    C(uint32_t, c, 2) = d[0];
    for (uint32_t k = 0; k < count - 3; k++)
        C(uint32_t, c, 0xa + k * 4) = d[1 + k];
    C(uint32_t, c, 6) = count - 3;
    q_start(s);
    return 1;
}

int __stdcall msg_wait(uint8_t *s, const uint32_t *d, int now)
{
    if (!now) {
        ALLOC(0xb, 0);
        C(uint32_t, c, 2) = d[0];
        C(uint8_t, c, 6) = 0;
        q_start(s);
    }
    return 1;
}

int __stdcall msg_jump(uint8_t *s, const int32_t *d, uint32_t count, int now)
{
    if (count - 2 < 2)
        return 0;
    int32_t x = d[0], y = d[1], rel = count - 2 < 3 ? 0 : d[2];
    if (!now) {
        ALLOC(0xc, 0);
        C(int32_t, c, 2) = x;
        C(int32_t, c, 6) = y;
        C(uint8_t, c, 10) = rel != 0;
        q_start(s);
    } else {
        sprite_move(s, x, y, rel, 1);
    }
    return 1;
}

int __stdcall msg_priority(uint8_t *s, const int32_t *d, int now)
{
    uint32_t v = d[0] != 0;
    if (!now) {
        ALLOC(0xd, 0);
        C(uint32_t, c, 2) = v;
        q_start(s);
    } else {
        S(uint32_t, s, SP_PRIORITY) = v;
        S(uint32_t, s, SP_NEXT_STEP) = 0;
        S(uint32_t, s, SP_NEXT_MOVE) = 0;
    }
    return 1;
}

int __stdcall msg_cel_range(uint8_t *s, const uint32_t *d, int now)
{
    if (!now) {
        ALLOC(0x10, 0);
        C(uint32_t, c, 2) = d[0];
        C(uint32_t, c, 6) = d[1];
        C(uint32_t, c, 10) = d[2];
        q_start(s);
    } else {
        sprite_set_cels(s, d[0], d[1], d[2]);
    }
    return 1;
}

int __stdcall msg_redraw(uint8_t *s, const int32_t *d, int now)
{
    if (!now) {
        ALLOC(0xe, 0);
        C(uint32_t, c, 2) = d[0] != 0;
        q_start(s);
    } else {
        sprite_set_positioned(s, d[0], 1);
    }
    return 1;
}

int __stdcall msg_motion(uint8_t *s, const int32_t *d, int now)
{
    if (!now) {
        ALLOC(0xf, 0);
        C(int32_t, c, 2) = d[0];
        q_start(s);
    } else if (!S(void *, s, SP_QUEUE)) {
        SpriteSetMotion(s, d[0]);
    } else {
        S(int32_t, s, SP_NEXT_MOVE) -= S(int32_t, s, SP_MOTION);
        S(int32_t, s, SP_MOTION) = d[0];
        S(int32_t, s, SP_NEXT_MOVE) += S(int32_t, s, SP_MOTION);
    }
    return 1;
}

int __stdcall msg_speed(uint8_t *s, const int32_t *d, uint32_t count, int now)
{
    if (!now) {
        ALLOC(0x11, 0);
        C(int32_t, c, 2) = d[0];
        if (count > 1)
            C(int32_t, c, 6) = d[1];
        q_start(s);
        return 1;
    }
    if (!S(void *, s, SP_QUEUE)) {
        SpriteSetSpeed(s, d[0]);
    } else {
        S(int32_t, s, SP_NEXT_STEP) -= S(int32_t, s, SP_SPEED);
        S(int32_t, s, SP_SPEED) = d[0];
        S(int32_t, s, SP_NEXT_STEP) += S(int32_t, s, SP_SPEED);
    }
    if (count > 1)
        msg_motion(s, d + 1, now);
    return 1;
}

int __stdcall msg_show(uint8_t *s, const uint32_t *d, uint32_t count, int now)
{
    uint32_t cel = count ? d[0] : 0xffffffff;
    if (!now) {
        ALLOC(0x12, 0);
        C(uint32_t, c, 2) = cel;
        q_start(s);
    } else {
        SpriteShow(g_hsl, s, cel, 1);
    }
    return 1;
}

/* Runs or queues message msg (type, values...) with `count` values */
int __stdcall msg_dispatch(uint8_t *s, const uint32_t *msg, uint32_t count, int now)
{
    /* sprites on the interface panel mark its dirty rectangles before and after
     * (original: compared with >= here, > in sprite_move) */
    int panel = !S(uint32_t, s, SP_SCROLLS) && H(uint32_t, HSL_DIRECTDRAW) && !H(uint32_t, HSL_B5C) &&
                H(uint32_t, HSL_IFACE_ON) && H(int32_t, HSL_VIEW2_BOTTOM) <= S(int32_t, s, SP_DIRTY_BOTTOM);
    if (panel)
        dirty_add_if_shown(s);
    const uint32_t *d = msg + 1;
    int r = 1;
    switch (msg[0]) {
    case 1: r = msg_cels(s, d, now); break;
    case 2: r = msg_type2(s, d, count, now); break;
    case 3: r = msg_loop_start(s, now); break;
    case 4: r = msg_to_background(s, now); break;
    case 5: r = msg_curve(s, (const int32_t *)d, now); break;
    case 6: r = msg_loop(s, d, count, now); break;
    case 7: r = msg_flip(s, (const int32_t *)d, now); break;
    case 8: msg_hide(s, now); break;
    case 9: r = msg_line(s, (const int32_t *)d, now); break;
    case 10: r = msg_type10(s, d, count, now); break;
    case 0xb: r = msg_wait(s, d, now); break;
    case 0xc: r = msg_jump(s, (const int32_t *)d, count, now); break;
    case 0xd: r = msg_priority(s, (const int32_t *)d, now); break;
    case 0xe: r = msg_redraw(s, (const int32_t *)d, now); break;
    case 0xf: r = msg_motion(s, (const int32_t *)d, now); break;
    case 0x10: r = msg_cel_range(s, d, now); break;
    case 0x11: r = msg_speed(s, (const int32_t *)d, count, now); break;
    case 0x12: r = msg_show(s, d, count, now); break;
    case 0x16: r = msg_game(s, d, count, now); break;
    case 0x18: r = msg_level(s, d, now); break;
    default: r = 0; break;
    }
    if (panel)
        dirty_add_if_shown(s);
    return r;
}

void __stdcall SpritePostMessage(uint8_t *hsl, uint8_t *s, uint32_t count, const uint32_t *msg)
{
    g_hsl = hsl;
    msg_dispatch(s, msg, count, 0);
}

void __stdcall SpriteSendMessage(uint8_t *hsl, uint8_t *s, uint32_t count, const uint32_t *msg)
{
    g_hsl = hsl;
    msg_dispatch(s, msg, count, 1);
}

/* ---- movies and blit effects (0x1005bbf7-0x1005c010) ---- */

#define HSL_MOVIE_PROC 0x020   /* proc(sprite, current command): 0 = show the command's frame */
#define HSL_FX_TABLE1 0xb6c
#define HSL_FX_TABLE2 0xb70
#define SP_BITMAP 0x18
#define SP_RECT 0x44
#define SP_OVERLAPS 0xc4
#define SP_UPDATED 0xe8
#define SP_VISIBLE 0xec
#define INFO_FX 0x2c
#define INFO_FX_ARG 0x30
#define HSL_SCROLL_X 0x8d4
#define HSL_SCROLL_Y 0x8d8

typedef uint32_t(__stdcall *MovieProc)(uint8_t *, uint8_t *);

void __stdcall set_fx_table(uint8_t *hsl, int which, uint32_t table)
{
    g_hsl = hsl;
    if (which == 1)
        H(uint32_t, HSL_FX_TABLE1) = table;
    else if (which == 2)
        H(uint32_t, HSL_FX_TABLE2) = table;
}

void __stdcall SpriteSetBlitFX(uint8_t *s, uint32_t fx, uint32_t arg)
{
    INFO(uint32_t, s, INFO_FX) = fx;
    INFO(uint32_t, s, INFO_FX_ARG) = arg;
}

void __stdcall SpriteSetMovieProc(uint8_t *hsl, uint32_t proc)
{
    g_hsl = hsl;
    H(uint32_t, HSL_MOVIE_PROC) = proc;
}

/* Asks the game's movie procedure about s; 1 when there is none or it handled the frame */
int __stdcall movie_call(uint8_t *s)
{
    if (!H(MovieProc, HSL_MOVIE_PROC))
        return 1;
    S(uint32_t, s, SP_ENABLED) = 0;
    S(uint32_t, s, SP_MSG_PENDING) = 1;
    int r = H(MovieProc, HSL_MOVIE_PROC)(s, S(uint8_t *, s, SP_QUEUE_CUR)) == 0;
    /* (the original checks the state that is current after the call) */
    if ((H(uint8_t *, HSL_CURRENT) == s || sprite_check(s) != -1) && S(uint32_t, s, SP_MSG_PENDING)) {
        S(uint32_t, s, SP_ENABLED) = 1;
        S(uint32_t, s, SP_MSG_PENDING) = 0;
        return r;
    }
    return 0;
}

/* Command 0x17: show the movie frame (bitmap at +0xc, flag at +10) unless the movie
 * procedure handled it; returns 1 when the command is done */
uint32_t __stdcall cmd_movie(uint8_t *s)
{
    if (movie_call(s))
        return 1;
    uint8_t *c = S(uint8_t *, s, SP_QUEUE_CUR);
    if (!c || !C(char, c, 10) || !C(uint32_t, c, 0xc))
        return 0;
    uint8_t *bm = C(uint8_t *, c, 0xc);
    S(uint8_t *, s, SP_BITMAP) = bm;
    int32_t hw = (S(int32_t, bm, 4) - 1) / 2, hh = (S(int32_t, bm, 0xc) - 1) / 2;
    int32_t old[4];
    memcpy(old, s + SP_RECT, 16);
    int32_t *r = (int32_t *)(s + SP_RECT);
    r[0] = S(int32_t, s, SP_X) - hw + S(int32_t, bm, 0x14);
    r[2] = r[0] + S(int32_t, bm, 4);
    r[1] = S(int32_t, s, SP_Y) - hh + S(int32_t, bm, 0x18);
    r[3] = r[1] + S(int32_t, bm, 0xc);
    if (S(uint32_t, s, SP_SCROLLS))
        OffsetRect((RECT *)r, H(int32_t, HSL_SCROLL_X), H(int32_t, HSL_SCROLL_Y));
    S(uint32_t, s, SP_OVERLAPS) = rect_intersect(old, old, r);
    S(uint32_t, s, SP_UPDATED) = 0;
    S(uint32_t, s, SP_VISIBLE) = 1;
    if (S(uint32_t, s, SP_CALLBACK))
        sprite_callback(s);
    sprite_reposition(s);
    return 0;
}
