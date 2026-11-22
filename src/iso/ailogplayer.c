/* The game logic, 0x100e3f0d-0x100e42a6: the AI log's report on one computer player. */
#include "iso_core.h"
#include "aigrouplog.h"
#include "ailists.h"
#include "ailogplayer.h"
#include "ailogstate.h"
#include "unitinfo.h"

#define AI G(uint8_t *, 0x1018b604)
#define TASK_KIND(t) P(int32_t, G(uint8_t *, 0x1018b600) + (t) * 0x74, 0x4c)
#define STR(a) ((const char *)ORIG_PTR(a))
#define LOG(...) debug_print(0x10, __VA_ARGS__)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define attitude_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009e26a)
#define count_1b9b ORIG(int32_t(__cdecl *)(int32_t), 0x100a0285)
#define count_1b97 ORIG(int32_t(__cdecl *)(int32_t), 0x100a02b6)
#define squad_next ORIG(int32_t(__cdecl *)(int32_t), 0x100c038c)

/* the unit type of the task in field `off`, or the text at `none` */
static const char *slot_name(const uint8_t *p, uint32_t off, char *buf, uint32_t none)
{
    int32_t t = P(int32_t, p, off);
    return t != -1 ? unit_type_name(TASK_KIND(t), buf) : STR(none);
}

/* 0x100e3f0d: writes player `k`'s state to the AI log: attitude, counters, build slots,
 * records and its squads of kinds 2-5. The log is switched off in the shipped game. */
void __cdecl ai_log_player(int32_t k)
{
    uint8_t *p = AI + k * 0x143 + 0xc;
    char a[0x1c], b[0x1c], c[0x1c], d[0x1c];
    LOG(STR(0x1015b10c), k, I(int32_t, 0x17dd));
    LOG(STR(0x1015b128), attitude_name(P(int32_t, p, 0x5e), b));
    LOG(STR(0x1015b138), (uint32_t)P(uint16_t, p, 0x66), (uint32_t)P(uint8_t, p, 0x10a),
        (int32_t)P(int16_t, p, 0x117), (int32_t)P(int16_t, p, 0x115));
    LOG(STR(0x1015b170), (uint32_t)P(uint16_t, p, 0x94), (uint32_t)P(uint16_t, p, 0x96),
        (uint32_t)P(uint16_t, p, 0x9c), (uint32_t)P(uint16_t, p, 0x9a), (uint32_t)P(uint16_t, p, 0x9e),
        P(int32_t, p, 0xfa));
    int32_t c97 = count_1b97(k);
    int32_t c9b = count_1b9b(k);
    LOG(STR(0x1015b1d8), P(int32_t, p, 0xfe), c9b, P(int32_t, p, 0x102), c97, (uint32_t)P(uint16_t, p, 0x6a),
        (uint32_t)P(uint16_t, p, 0x6c));
    const char *n1 = slot_name(p, 0x133, a, 0x1015b24c);
    const char *n2 = slot_name(p, 0x12f, b, 0x1015b250);
    LOG(STR(0x1015b254), n2, n1);
    const char *n3 = slot_name(p, 0x13b, c, 0x1015b274);
    const char *n4 = slot_name(p, 0x137, d, 0x1015b278);
    LOG(STR(0x1015b27c), n4, n3);
    ai_log_state(k);
    ai_log_enemies(k);
    for (uint32_t kind = 2; kind <= 5; kind++)
        for (int32_t s = ai_list_first(k, (int32_t)kind); s != -1; s = squad_next(s))
            ai_log_group(k, s);
    LOG(STR(0x1015b28c), P(int32_t, AI, 0xa40), P(int32_t, AI, 0xa48), P(int32_t, AI, 0xa44));
    LOG(STR(0x1015b2c4), G(int32_t, 0x10152b58));
    LOG(STR(0x1015b2ec), k);
}
