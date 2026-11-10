/* Engine core: script expressions (0x100032b0-0x10003f6d, 0x1000b16f-0x1000b3e0).
 *
 * Expressions in the menu scripts are compiled to a small stack machine: one opcode
 * byte, some followed by 32-bit operands; 0 ends the expression. Values are 32 bits;
 * addresses of variables are real pointers. Rewritten from the original.
 *
 * Variables below number 0x13fe are globals in the state block; higher numbers are
 * locals, addressed downwards from the frame pointer at RT_FRAME. */
#include "rtcore.h"
#include "rt.h"
#include "rt_script.h"

#define RT_EXTERN_TABLE 0x324f /* 8 bytes per extern function: ?, address */
#define RT_OBJECTS 0x325b
#define RT_EXTERN_OBJECTS 0x32ab /* object number of each extern function's plugin */
#define RT_FRAME 0x36cf          /* frame pointer of the script's local variables */
#define RT_CALL_HOOK 0x3c3b      /* proc(function, arg): called before every call */
#define RT_CALL_HOOK_ARG 0x3c3f
#define RT_SP 0x3e4f             /* evaluation stack pointer */
#define RT_RESULT 0x3f3b         /* a script function's return value */
#define RT_BUILTINS 0x3f4b       /* function numbers above this are extern functions */
#define RT_GLOBALS 0x3f4f
#define RT_SCRIPT_DEBUG 0x95d8

#define g_stack ((int32_t *)ORIG_PTR(0x1007c408)) /* evaluation stack */
#define SP R(int32_t, RT_SP)
#define TOP g_stack[SP - 1]

#define rt_script_error ORIG(int32_t(__stdcall *)(uint32_t, uint32_t), 0x100260f6)
#define rt_fatal ORIG(void(__stdcall *)(uint32_t, uint32_t), 0x10025b0f)
#define script_run ORIG(int(__stdcall *)(uint32_t), 0x10022ce9)

/* 0x1000b390: where variable n lives */
int32_t *__cdecl script_var(uint32_t n)
{
    if (n < 0x13fe)
        return (int32_t *)(g_rt + RT_GLOBALS + n * 4);
    return (int32_t *)(uintptr_t)(R(uint32_t, RT_FRAME) - (n * 4 - 0x4ff8));
}

#define PTR(v) ((int32_t *)(uintptr_t)(uint32_t)(v))

/* 0x100032b0: evaluates the expression at code. The stack pointer is put back at the
 * end; the result is the first value the expression pushed. */
int32_t __stdcall script_eval(const uint8_t *code)
{
    int32_t base = SP;
    for (;;) {
        uint8_t op = *code++;
        int32_t b;
        switch (op) {
        case 0:
            SP = base;
            return g_stack[SP];
        case 1: /* constant */
            g_stack[SP++] = *(const int32_t *)code;
            code += 4;
            break;
        case 2: /* variable's value */
            g_stack[SP++] = *script_var(*(const uint32_t *)code);
            code += 4;
            break;
        case 3: /* array[index] */
            SP--;
            TOP = PTR(TOP)[g_stack[SP]];
            break;
        case 4: /* variable's address */
            g_stack[SP++] = (int32_t)(uintptr_t)script_var(*(const uint32_t *)code);
            code += 4;
            break;
        case 5: /* *address */
            TOP = *PTR(TOP);
            break;
        case 6: /* &array[index] */
            SP--;
            TOP = TOP + g_stack[SP] * 4;
            break;
        case 8: /* *address = value */
            SP--;
            *PTR(TOP) = g_stack[SP];
            TOP = g_stack[SP];
            break;
        case 0xd: /* built-in or extern function */
            script_call((const int32_t *)code);
            code += 8;
            break;
        case 0xe: /* script function */
            script_user_call((const int32_t *)code);
            code += 8;
            break;
        case 0xf:
            TOP = -TOP;
            break;
        case 0x10:
            TOP = TOP == 0;
            break;
        case 0x11:
            TOP = ~TOP;
            break;
        case 0x12:
            SP--;
            TOP += g_stack[SP];
            break;
        case 0x13:
            SP--;
            TOP -= g_stack[SP];
            break;
        case 0x14:
            SP--;
            TOP *= g_stack[SP];
            break;
        case 0x15:
        case 0x16: /* divide, remainder; the stack pointer is not put back on an error */
            SP--;
            if (g_stack[SP] == 0) {
                if (R(uint8_t, RT_SCRIPT_DEBUG))
                    return rt_script_error(0x6f, 0);
                rt_fatal(1000, 0);
                return 0;
            }
            TOP = op == 0x15 ? TOP / g_stack[SP] : TOP % g_stack[SP];
            break;
        case 0x17:
            SP--;
            TOP = TOP == g_stack[SP];
            break;
        case 0x18:
            SP--;
            TOP = TOP <= g_stack[SP];
            break;
        case 0x19:
            SP--;
            TOP = TOP < g_stack[SP];
            break;
        case 0x1a:
            SP--;
            TOP = TOP >= g_stack[SP];
            break;
        case 0x1b:
            SP--;
            TOP = TOP > g_stack[SP];
            break;
        case 0x1c:
            SP--;
            TOP = TOP != g_stack[SP];
            break;
        case 0x1d:
            SP--;
            TOP = TOP != 0 && g_stack[SP] != 0;
            break;
        case 0x1e:
            SP--;
            TOP = TOP != 0 || g_stack[SP] != 0;
            break;
        case 0x1f:
            SP--;
            TOP &= g_stack[SP];
            break;
        case 0x20:
            SP--;
            TOP |= g_stack[SP];
            break;
        case 0x21:
            SP--;
            TOP ^= g_stack[SP];
            break;
        case 0x22: /* ++*address */
            ++*PTR(TOP);
            TOP = *PTR(TOP);
            break;
        case 0x23: { /* (*address)++ */
            int32_t *p = PTR(TOP);
            TOP = *p;
            ++*p;
            break;
        }
        case 0x24:
            --*PTR(TOP);
            TOP = *PTR(TOP);
            break;
        case 0x25: {
            int32_t *p = PTR(TOP);
            TOP = *p;
            --*p;
            break;
        }
        case 0x26: /* ||: when the left side is true, push 1 and jump over the right side */
            if (TOP == 0)
                code += 4;
            else {
                b = *(const int32_t *)code;
                g_stack[SP++] = 1;
                code += b;
            }
            break;
        case 0x27: /* &&: when the left side is false, push 0 and jump */
            if (TOP == 0) {
                b = *(const int32_t *)code;
                g_stack[SP++] = 0;
                code += b;
            } else
                code += 4;
            break;
        case 0x28: /* push *address, keeping the address */
            g_stack[SP] = *PTR(TOP);
            SP++;
            break;
        case 0x29:
            SP--;
            TOP = (int32_t)((uint32_t)TOP << (g_stack[SP] & 0x1f));
            break;
        case 0x2a:
            SP--;
            TOP = TOP >> (g_stack[SP] & 0x1f);
            break;
        }
    }
}

/* 0x10003ea5: calls function number stack[top - argc]. args: the caller's frame size in
 * variables, then the argument count. Built-in functions are in the original's big
 * switch (0x10003f6d). */
void __stdcall script_call(const int32_t *args)
{
    int32_t frame = args[0], argc = args[1];
    R(uint32_t, RT_FRAME) -= frame * 4;
    uint32_t fn = (uint32_t)g_stack[SP - argc - 1];
    if (R(uint32_t, RT_CALL_HOOK)) {
        uint32_t a[2] = {fn, R(uint32_t, RT_CALL_HOOK_ARG)};
        orig_stdcall_unsafe(R(void *, RT_CALL_HOOK), 2, a);
    }
    if (R(uint32_t, RT_BUILTINS) < fn)
        script_extern_call(fn, argc);
    else
        ORIG(void(__stdcall *)(uint32_t, int32_t), 0x10003f6d)(fn, argc);
    R(uint32_t, RT_FRAME) += frame * 4;
}

/* ExternalCall (0x1003d861): fn(ctx, args...), C calling convention */
uint32_t __stdcall ExternalCall(void *fn, uint32_t ctx, const uint32_t *args, uint32_t argc, uint32_t unused)
{
    (void)unused;
    uint32_t a[65];
    if (argc > 64) {
        uint32_t o[5] = {(uint32_t)(uintptr_t)fn, ctx, (uint32_t)(uintptr_t)args, argc, 0};
        return orig_stdcall_unsafe(ORIG_PTR(0x1003d861), 5, o);
    }
    a[0] = ctx;
    for (uint32_t i = 0; i < argc; i++)
        a[i + 1] = args[i];
    return orig_stdcall_unsafe(fn, argc + 1, a);
}

/* 0x1000b16f: a function in a plugin DLL. Its arguments are taken off the stack and
 * the result replaces the function number. */
void __stdcall script_extern_call(uint32_t fn, int argc)
{
    uint32_t i = fn - R(uint32_t, RT_BUILTINS) - 1;
    SP -= argc;
    uint8_t *plugin = R(uint8_t **, RT_OBJECTS)[R(uint32_t *, RT_EXTERN_OBJECTS)[i]];
    P(int32_t, plugin, 0x28) = argc + 1;
    TOP = (int32_t)ExternalCall(P(void *, R(uint8_t *, RT_EXTERN_TABLE), 4 + i * 8), P(uint32_t, plugin, 4),
                                (const uint32_t *)&g_stack[SP], (uint32_t)argc, 0);
}

/* 0x1000b250: a function written in the script. The arguments move from the stack into
 * the new frame's variables; the function's result is pushed. */
int __stdcall script_user_call(const int32_t *args)
{
    int32_t frame = args[0], argc = args[1];
    int32_t *v = PTR(R(uint32_t, RT_FRAME) - frame * 4 - argc * 4);
    for (int32_t k = 1; k <= argc; k++)
        v[k] = g_stack[--SP];
    R(uint32_t, RT_FRAME) -= frame * 4;
    uint32_t saved = R(uint32_t, RT_RESULT);
    R(uint32_t, RT_RESULT) = 0;
    SP--;
    int r = script_run((uint32_t)g_stack[SP]);
    g_stack[SP] = R(int32_t, RT_RESULT);
    SP++;
    R(uint32_t, RT_RESULT) = saved;
    R(uint32_t, RT_FRAME) += frame * 4;
    return r;
}
