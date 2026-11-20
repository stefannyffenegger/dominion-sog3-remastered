/* The game logic, 0x1009b89f-0x1009c8a9: the path editor's mode, the AI's global conditions
 * (29 of 0x18 bytes at 0x101534f0), and the table of 60 AI records of 400 bytes (0x1017df44,
 * used count 0x1017df40; +0 in use, +4 the player, +8 the record's own number), saved with
 * the game. */
#include "iso_core.h"
#include "aiconds.h"
#include "aislots.h"

#define AI G(uint8_t *, 0x1018b604)
#define CONDS 0x101534f0
#define RECORDS G(uint8_t *, 0x1017df44)
#define USED G(int32_t, 0x1017df40)

/* EDITPATH_GetEditMode */
int32_t __cdecl EDITPATH_GetEditModeC(void) { return G(int32_t, 0x1017c9e8); }

/* 0x1009c5c0: updates the global conditions of type 2 and makes the empty record table.
 * Nonzero if it could */
int32_t __cdecl ai_records_init(void)
{
    for (int k = 0; k < 0x1d; k++)
        if (G(int32_t, CONDS + 4 + k * 0x18) == 2)
            ai_condition_update(0, (uint8_t *)ORIG_PTR(CONDS + k * 0x18));
    USED = 0;
    RECORDS = winmalloc(24000, 1);
    if (!RECORDS)
        return 0;
    for (int k = 0; k < 0x3c; k++) {
        P(int32_t, RECORDS, k * 400) = 0;
        P(int32_t, RECORDS, k * 400 + 8) = k;
        P(int32_t, RECORDS, k * 400 + 4) = 0;
    }
    return 1;
}

/* 0x1009c76a: adds a copy of `r` as the next record (keeping the slot's number), counting it
 * for all and for its player. The record, or 0 when the table is full */
uint8_t *__cdecl ai_record_add(const uint8_t *r)
{
    if (USED >= 0x3c)
        return 0;
    P(int16_t, AI, 0xa26)++;
    P(int16_t, AI, P(int32_t, r, 4) * 0x143 + 0x6e)++;
    uint8_t *s = RECORDS + USED * 400;
    int32_t num = P(int32_t, s, 8);
    memcpy(s, r, 400);
    P(int32_t, s, 0) = 1;
    P(int32_t, s, 8) = num;
    USED++;
    return s;
}

/* 0x1009c86a: the global condition k */
uint8_t *__cdecl ai_global_condition(int32_t k) { return (uint8_t *)ORIG_PTR(CONDS) + k * 0x18; }

/* 0x1009c887: record k */
uint8_t *__cdecl ai_record(int32_t k) { return RECORDS + k * 400; }
