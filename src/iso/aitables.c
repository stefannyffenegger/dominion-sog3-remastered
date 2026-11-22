/* The game logic, 0x100e2095-0x100e24f7: the computer player's three record tables. */
#include "iso_core.h"
#include "aitables.h"

#define AI G(uint8_t *, 0x1018b604)
#define TABLE_A G(uint8_t *, 0x1018b610)
#define TABLE_B G(uint8_t *, 0x1018b608)
#define TABLE_C G(uint8_t *, 0x1018b600)
#define COUNT 2000
#define orig_malloc ORIG(uint8_t *(__stdcall *)(uint32_t, int32_t), 0x100f4976)
#define ai_set_head ORIG(void(__cdecl *)(int32_t), 0x100721c7)
#define squad_set_first ORIG(void(__cdecl *)(int32_t), 0x100c0368)
#define task_set_free_head ORIG(void(__cdecl *)(int32_t), 0x1004bb08)
#define ai_records_init ORIG(int32_t(__cdecl *)(void), 0x1009c5c0)
#define records_alloc ORIG(int32_t(__cdecl *)(uint32_t), 0x10094d90)

/* 0x100e2095: allocates the AI's unit (0x120 bytes), squad (0x130) and task (0x74) records,
 * 2000 of each, as free lists, then its other records. 0 on failure.
 * original: after allocating table B it tests table A again, so a failed B is not caught. */
int32_t __cdecl ai_tables_init(void)
{
    TABLE_A = orig_malloc(0x8ca00, 1);
    if (!TABLE_A)
        return 0;
    for (int32_t i = 0; i < COUNT; i++) {
        uint8_t *r = TABLE_A + i * 0x120;
        P(int32_t, r, 0) = 0;
        P(int32_t, r, 8) = i - 1;
        P(int32_t, r, 4) = i + 1;
        P(int32_t, r, 0x18) = -1;
        P(int32_t, r, 0x110) = -1;
        P(int32_t, r, 0x114) = -1;
        P(int32_t, r, 0x118) = -1;
        P(int32_t, r, 0x11c) = -1;
        P(int32_t, r, 0x2c) = -1;
        P(int32_t, r, 0x28) = -1;
        P(int32_t, r, 0x20) = 0;
    }
    P(int32_t, TABLE_A + (COUNT - 1) * 0x120, 4) = -1;
    ai_set_head(0);

    TABLE_B = orig_malloc(0x94700, 1);
    if (!TABLE_A)
        return 0;
    for (int32_t i = 0; i < COUNT; i++) {
        uint8_t *r = TABLE_B + i * 0x130;
        P(int32_t, r, 0) = 0;
        P(int32_t, r, 8) = i - 1;
        P(int32_t, r, 4) = i + 1;
        P(int32_t, r, 0x24) = -1;
        P(int32_t, r, 0x28) = -1;
        P(int16_t, r, 0x34) = 0;
        P(int32_t, r, 0x114) = 0;
        P(int32_t, r, 0xe0) = 0;
        P(int32_t, r, 0xe4) = 0;
        P(int32_t, r, 0xe8) = 0;
        P(int32_t, r, 0xec) = 0;
    }
    P(int32_t, TABLE_B + (COUNT - 1) * 0x130, 4) = -1;
    squad_set_first(0);

    TABLE_C = orig_malloc(0x38a40, 1);
    if (!TABLE_C)
        return 0;
    for (int32_t i = 0; i < COUNT; i++) {
        uint8_t *r = TABLE_C + i * 0x74;
        P(int32_t, r, 0) = 0;
        P(int32_t, r, 0x18) = i - 1;
        P(int32_t, r, 0x14) = i + 1;
    }
    P(int32_t, TABLE_C + (COUNT - 1) * 0x74, 0x14) = -1;
    task_set_free_head(0);
    if (!ai_records_init())
        return 0;
    return records_alloc(P(uint32_t, AI, 0xa3c)) != 0;
}
