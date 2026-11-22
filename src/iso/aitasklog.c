/* The game logic, 0x100e3174-0x100e38bf: the AI log's list of a player's tasks. */
#include "iso_core.h"
#include "ailists.h"
#include "aitasklog.h"
#include "unitinfo.h"

#define T(t, off) P(int32_t, G(uint8_t *, 0x1018b600) + (t) * 0x74, off)
#define TARGET_TYPE(k) P(int32_t, G(uint8_t *, 0x1018b610) + (k) * 0x120, 0x24)
#define STR(a) ((const char *)ORIG_PTR(a))
#define LOG(...) debug_print(0x10, __VA_ARGS__)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define task_type_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009de5b)
#define task_state_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009e0e3)
#define player_unseen ORIG(int32_t(__cdecl *)(int32_t), 0x100a04d3)
#define task_next ORIG(int32_t(__cdecl *)(int32_t), 0x1004bb2c)

/* the name of the target's type, or `none` without a target */
static const char *target_name(int has, int32_t t, char *buf, uint32_t none)
{
    return has ? unit_type_name(TARGET_TYPE(T(t, 0x44)), buf) : STR(none);
}

/* the name of the task's unit type (+0x4c), or `none` */
static const char *kind_name(int32_t t, char *buf, uint32_t none)
{
    return T(t, 0x4c) > 0 ? unit_type_name(T(t, 0x4c), buf) : STR(none);
}

/* 0x100e3174: writes player `k`'s tasks (list 7) to the AI log, with the details of their type */
void __cdecl ai_log_tasks(int32_t k)
{
    char a[0x40], b[0x40];
    int32_t t = ai_list_first(k, 7);
    LOG(STR(0x1015ac70));
    for (; t != -1; t = task_next(t)) {
        const char *state = task_state_name(T(t, 0x2c), a);
        const char *type = task_type_name(T(t, 0x30), b);
        LOG(STR(0x1015ac7c), t, type, state);
        if (T(t, 0x30) == 8) {
            const char *tn = target_name(T(t, 0x44) != -1, t, a, 0x1015ac90);
            const char *sn = kind_name(t, b, 0x1015ac94);
            LOG(STR(0x1015aca4), sn, T(t, 0x38), T(t, 0x40), tn, T(t, 0x44));
        }
        if (T(t, 0x30) == 0x14) {
            const char *tn = target_name(T(t, 0x44) != -1, t, a, 0x1015acf8);
            LOG(STR(0x1015acfc), T(t, 0x60), T(t, 0x38), tn, T(t, 0x44), T(t, 0x40));
        }
        if (T(t, 0x30) == 0x15) {
            const char *tn = target_name(T(t, 0x44) != -1, t, a, 0x1015ad50);
            LOG(STR(0x1015ad54), T(t, 0x38), tn, T(t, 0x44), T(t, 0x40));
        }
        if (T(t, 0x30) == 7) {
            int32_t v = T(t, 0x38) == -1 ? -2 : P(int32_t, G(uint8_t *, 0x1018b608) + T(t, 0x38) * 0x130, 0xdc);
            LOG(STR(0x1015ad9c), T(t, 0x38), v);
        }
        if (T(t, 0x30) == 0xf || T(t, 0x30) == 0x10) {
            const char *tn = target_name(T(t, 0x44) > -1, t, a, 0x1015adc0);
            const char *sn = kind_name(t, b, 0x1015adc8);
            LOG(STR(0x1015add8), sn, tn, T(t, 0x44), T(t, 0x40), T(t, 0x38));
        }
        if (T(t, 0x30) == 1)
            LOG(STR(0x1015ae1c), T(t, 0x50), T(t, 0x4c), player_unseen(k));
        if (T(t, 0x30) == 0x13) {
            const char *tn = target_name(T(t, 0x44) > -1, t, a, 0x1015ae60);
            LOG(STR(0x1015ae68), tn, T(t, 0x44), T(t, 0x40), T(t, 0x38));
        }
    }
    LOG(STR(0x1015aea4));
}
