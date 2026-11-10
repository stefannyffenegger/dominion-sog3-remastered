/* The sprite engine's script queues: each sprite can run a list of commands (animate
 * cels, move along a line or curve, wait, loop, flip, show, hide, send a message to the
 * game, ...). Rewritten from the original (0x10053c50-0x10054da0).
 *
 * A command is a 16-bit type followed by its data, packed. Most values come with a flag
 * byte: when set, the value is an index into the game's variable table (sprite_delay).
 * Commands still original: 1 (cels), 2, 5 (curve), 9 (line), 10, 0x17. */
#include "orig.h"
#include "queue.h"
#include "sprite.h"
#include "view.h"
#include "utils.h"

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
#define cmd_cels ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10055fc0)
#define cmd_2 ORIG(uint32_t(__stdcall *)(uint8_t *), 0x1006254a)
#define cmd_curve ORIG(uint32_t(__stdcall *)(uint8_t *), 0x100634b0)
#define cmd_line ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10063180)
#define cmd_10 ORIG(uint32_t(__stdcall *)(uint8_t *), 0x10063368)
#define cmd_17 ORIG(uint32_t(__stdcall *)(uint8_t *), 0x1005bd70)
#define sprite_reposition ORIG(void(__stdcall *)(uint8_t *), 0x100560d0)
#define sprite_move ORIG(void(__stdcall *)(uint8_t *, int, int, int, int), 0x10057753)
#define parts_update_bounds ORIG(void(__stdcall *)(uint8_t *), 0x10062320)
#define SpriteHide ORIG(void(__stdcall *)(uint8_t *, uint8_t *, int), 0x100579e3)
#define SpriteShow ORIG(void(__stdcall *)(uint8_t *, uint8_t *, uint32_t, int), 0x10057c57)

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
        r = cmd_2(s);
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
        r = cmd_17(s);
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
