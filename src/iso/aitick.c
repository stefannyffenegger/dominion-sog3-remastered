/* The game logic, 0x100e2f68-0x100e315a: the computer players' main tick. */
#include "iso_core.h"
#include "aislots.h"
#include "ailists.h"
#include "aitasks.h"
#include "aitick.h"
#include "debugtime.h"

#define AI G(uint8_t *, 0x1018b604)
#define PLAYER(k) (AI + (k) * 0x143 + 0xc)
#define STR(a) ((const char *)ORIG_PTR(a))
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define rule_run ORIG(void(__cdecl *)(uint8_t *), 0x100f4599)
#define task_initial ORIG(void(__cdecl *)(int32_t, int32_t), 0x10061a7e)
#define player_first_state ORIG(void(__cdecl *)(int32_t), 0x100e3f0d)

/* 0x100e2f68: counts player `k`'s ticks (+0xe); every +0x12th it evaluates its active rules
 * (records with +0 and +0xc set, of this player at +4) */
void __cdecl ai_player_rules(int32_t k)
{
    P(uint32_t, PLAYER(k), 0xe)++;
    if (P(uint32_t, PLAYER(k), 0xe) < P(uint32_t, PLAYER(k), 0x12))
        return;
    DWORD start = timeGetTime();
    for (int32_t i = 0; i < P(uint16_t, AI, 0xa26); i++) {
        uint8_t *r = ai_record(i);
        if (P(int32_t, r, 0) && P(int32_t, r, 0xc) && P(int32_t, r, 4) == k)
            rule_run(r);
    }
    P(uint32_t, PLAYER(k), 0xe) = 0;
    ai_timing_log(0x10, STR(0x1015abc8), start);
}

/* 0x100e3077: one AI tick for every computer player: on tick 0 its first state, then its
 * rules and tasks. Returns 1 */
int32_t __cdecl ai_tick(void)
{
    DWORD start = timeGetTime();
    debug_print(0x10, STR(0x1015abdc), I(int32_t, 0x17dd));
    for (int32_t k = 0; k < 8; k++) {
        if (!player_is_ai(k))
            continue;
        if (!I(int32_t, 0x17dd)) {
            task_initial(k, 6);
            debug_print(0x10, STR(0x1015abf0));
            player_first_state(k);
            debug_print(0x10, STR(0x1015ac28));
        }
        ai_player_rules(k);
        ai_run_tasks(k);
    }
    ai_timing_log(0x10, STR(0x1015ac60), start);
    return 1;
}
