/* The game logic, 0x100e38bf-0x100e3abf: the AI log's lists of a player's records. */
#include "iso_core.h"
#include "ailists.h"
#include "aislots.h"
#include "ailogstate.h"
#include "aitasklog.h"

#define STR(a) ((const char *)ORIG_PTR(a))
#define LOG(...) debug_print(0x10, __VA_ARGS__)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define task_next ORIG(int32_t(__cdecl *)(int32_t), 0x1004bb2c)
#define own_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x100721e9)
#define other_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x10072212)

/* 0x100e38bf: writes to the AI log player `k`'s active rules, its own targets (list 0), the
 * records of list 8, and its tasks */
void __cdecl ai_log_state(int32_t k)
{
    LOG(STR(0x1015aea8), k);
    for (int32_t i = 0; i < P(uint16_t, G(uint8_t *, 0x1018b604), 0xa26); i++) {
        uint8_t *r = ai_record(i);
        if (P(int32_t, r, 0) && P(int32_t, r, 0xc) && P(int32_t, r, 4) == k)
            LOG(STR(0x1015aebc), P(int32_t, r, 8));
    }
    LOG(STR(0x1015aec0));
    LOG(STR(0x1015aec4), k);
    for (int32_t t = ai_list_first(k, 0); t != -1; t = own_next(k, t))
        LOG(STR(0x1015aed4), t);
    LOG(STR(0x1015aedc));
    LOG(STR(0x1015aee0), k);
    for (int32_t t = ai_list_first(k, 8); t != -1; t = task_next(t))
        LOG(STR(0x1015aef4), t);
    LOG(STR(0x1015aefc));
    ai_log_tasks(k);
}

/* 0x100e3a44: writes the targets of player `k`'s list 1 (the others' units) to the AI log */
void __cdecl ai_log_enemies(int32_t k)
{
    LOG(STR(0x1015af00), k);
    for (int32_t t = ai_list_first(k, 1); t != -1; t = other_next(k, t))
        LOG(STR(0x1015af1c), t);
    LOG(STR(0x1015af24));
}
