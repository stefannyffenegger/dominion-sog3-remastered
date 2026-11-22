/* The game logic, 0x100e3abf-0x100e3f0d: the AI log's entry for one squad. */
#include "iso_core.h"
#include "aigrouplog.h"

#define SQ(s, type, off) P(type, G(uint8_t *, 0x1018b608) + (s) * 0x130, off)
#define STR(a) ((const char *)ORIG_PTR(a))
#define LOG(...) debug_print(0x10, __VA_ARGS__)
#define debug_print ORIG(int32_t(__cdecl *)(int32_t, const char *, ...), 0x10095e98)
#define kind_name ORIG(const char *(__cdecl *)(int32_t, char *), 0x1009e31a)
#define own_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007223b)
#define threat_next ORIG(int32_t(__cdecl *)(int32_t, int32_t), 0x1007225e)
#define health_total ORIG(int32_t(__cdecl *)(int32_t), 0x100c11d6)

/* 0x100e3abf: writes squad `s` of player `k` to the AI log: its kind (2 friendly group, 3
 * friendly base, 4 threat group, 5 threat base), figures and member targets. The AI log is
 * switched off in the shipped game, so only the calls for the figures have any effect. */
void __cdecl ai_log_group(int32_t k, int32_t s)
{
    char name[0x20];
    int32_t kind = SQ(s, int32_t, 0xc);
    switch (kind) {
    case 2: {
        int32_t health = health_total(s);
        const char *task = kind_name(SQ(s, int32_t, 0x10), name);
        LOG(STR(0x1015af48), s, task, STR(SQ(s, int32_t, 0x1c) == -1 ? 0x1015af28 : 0x1015af34),
            SQ(s, int32_t, 0x1c), (uint32_t)SQ(s, uint16_t, 0x34), health, SQ(s, int32_t, 0x114),
            SQ(s, int32_t, 0x120), SQ(s, int32_t, 0x11c));
        break;
    }
    case 4: {
        int32_t health = health_total(s);
        const char *task = kind_name(SQ(s, int32_t, 0x10), name);
        LOG(STR(0x1015afb0), s, task, (uint32_t)SQ(s, uint16_t, 0x34), health, SQ(s, int32_t, 0x114),
            SQ(s, int32_t, 0x120), SQ(s, int32_t, 0x11c));
        break;
    }
    case 3: {
        int32_t health = health_total(s);
        const char *task = kind_name(SQ(s, int32_t, 0x10), name);
        LOG(STR(0x1015b00c), s, task, (uint32_t)SQ(s, uint16_t, 0x34), SQ(s, int32_t, 0xdc), health,
            SQ(s, int32_t, 0x114), SQ(s, int32_t, 0x11c));
        break;
    }
    case 5: {
        int32_t health = health_total(s);
        const char *task = kind_name(SQ(s, int32_t, 0x10), name);
        LOG(STR(0x1015b070), s, task, (uint32_t)SQ(s, uint16_t, 0x34), health, SQ(s, int32_t, 0x114),
            SQ(s, int32_t, 0x120), SQ(s, int32_t, 0x11c));
        break;
    }
    default:
        LOG(STR(0x1015b0d0), kind);
        break;
    }
    LOG(STR(0x1015b0ec));
    for (int32_t t = SQ(s, int32_t, 0x24); t != -1;) {
        LOG(STR(0x1015b100), t);
        t = kind == 4 || kind == 5 ? threat_next(k, t) : own_next(k, t);
    }
    LOG(STR(0x1015b108));
}
