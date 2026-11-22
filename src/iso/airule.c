/* The game logic, 0x100f4599-0x100f46f4: running one rule of the computer player. */
#include "iso_core.h"
#include "airule.h"
#include "aiconds.h"
#include "aiglue.h"
#include "aislots.h"

#define fn_1009c84d ORIG(Callback *(__cdecl *)(int32_t), 0x1009c84d)

/* 0x100f4599: runs the rule's up to 16 steps in order, for its player (+4). Step kind 0
 * does an action, kind 1 tests a condition and ends the rule when it fails, -1 ends the
 * rule. A rule switched off (+0xc 0) does nothing. 1 if any action was done */
int32_t __cdecl ai_rule_run(uint8_t *rule)
{
    if (!P(int32_t, rule, 0xc))
        return 0;
    int32_t player = P(int32_t, rule, 4), acted = 0;
    for (int k = 0; k < 0x10; k++) {
        uint8_t *e = rule + 0x10 + k * 24;
        int32_t kind = P(int32_t, e, 0);
        if (kind == -1)
            return acted;
        if (kind == 0) {
            ai_callback((uint32_t)player, fn_1009c84d(P(int32_t, e, 4)), P(uint32_t, e, 8), P(uint32_t, e, 0xc),
                        P(uint32_t, e, 0x10), P(uint32_t, e, 0x14));
            acted = 1;
        } else if (kind == 1) {
            if (!ai_condition_value(player, ai_global_condition(P(int32_t, e, 4)), P(uint32_t, e, 8),
                                    P(uint32_t, e, 0xc), P(uint32_t, e, 0x10), P(uint32_t, e, 0x14)))
                return acted;
        }
    }
    return acted;
}
