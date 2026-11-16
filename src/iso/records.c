/* The game logic, 0x10016430-0x100167d0 and 0x100167d0-0x10016aa0: a table of 200 records
 * of 0x4c bytes (used, index, then 17 words of the owner's), saved whole in saved games;
 * and a unit's category for the interface. */
#include "iso_core.h"
#include "records.h"

#define RECORDS G(uint8_t *, 0x1017bb08)
#define RECORD(i) (RECORDS + (i) * 0x4c)
#define RECORDS_SIZE 0x3b60
typedef int(__cdecl *RecordTest)(uint8_t *);
#define g_find_test G(RecordTest, 0x1017bb00)
#define g_find_next G(int32_t, 0x1017bb04)

/* 0x10016430 */
int __cdecl records_init(void)
{
    RECORDS = winmalloc(RECORDS_SIZE, 1);
    if (!RECORDS)
        return 0;
    for (int i = 0; i < 200; i++) {
        P(uint32_t, RECORD(i), 0) = 0;
        P(int32_t, RECORD(i), 4) = i;
    }
    return 1;
}

/* 0x100164be */
int __cdecl records_load(HMMIO f) { return mmioRead(f, (HPSTR)RECORDS, RECORDS_SIZE) > 0; }

/* 0x100164f7 */
int __cdecl records_save(HMMIO f) { return mmioWrite(f, (const char *)RECORDS, RECORDS_SIZE) != 0; }

/* 0x10016530 */
void __cdecl records_free(void)
{
    if (RECORDS)
        winfree(RECORDS);
}

/* 0x10016558 */
uint32_t __cdecl records_size(void) { return RECORDS_SIZE; }

/* 0x1001656d: a free record filled with the 0x4c bytes at `from` (then marked used, with
 * its index); NULL when all 200 are used */
uint8_t *__cdecl record_add(const uint32_t *from)
{
    int i = 0;
    while (P(uint32_t, RECORD(i), 0)) {
        if (++i > 199)
            return NULL;
    }
    memcpy(RECORD(i), from, 0x4c);
    P(uint32_t, RECORD(i), 0) = 1;
    P(int32_t, RECORD(i), 4) = i;
    return RECORD(i);
}

/* 0x10016627 */
void __cdecl record_remove(uint32_t *r) { *r = 0; }

/* 0x10016640 */
uint8_t *__cdecl record_get(int i) { return i == -1 ? NULL : RECORD(i); }

static uint8_t *find_from_next(void)
{
    for (; g_find_next <= 199; g_find_next++) {
        if (P(uint32_t, RECORD(g_find_next), 0) && g_find_test(RECORD(g_find_next))) {
            int i = g_find_next; /* read after the test, as the original does */
            g_find_next++;
            return RECORD(i);
        }
    }
    return NULL;
}

/* 0x10016675: the first used record `test` accepts */
uint8_t *__cdecl record_find_first(RecordTest test)
{
    g_find_test = test;
    g_find_next = 0;
    return find_from_next();
}

/* 0x10016725: the next one */
uint8_t *__cdecl record_find_next(void) { return find_from_next(); }

/* 0x100167d0: the unit's category for the interface (its picture and commands): 1
 * infantry, 2-5 special infantry, 6-13 other kinds, 0xe armed structures and relays,
 * 0xf-0x12 other structures; 0 none. Callers use the low byte only (the original leaves
 * other bits in the register for 0xa, 0xb and 0xe). */
uint8_t __cdecl unit_category(uint32_t id)
{
    uint8_t *u = iso_unit((int)(id & 0xffff));
    uint8_t c = P(uint8_t, u, U_CLASS);
    char t = P(char, u, U_ID_TYPE);
    if (c < 8)
        return 1;
    if (c >= 0x40 && c <= 0x9f) {
        if (P(int16_t, u, 0xe6) == -1 && (c < 0x84 || c > 0x87)) {
            if (c == 0x50 || c == 100)
                return 0xf;
            if (c == 0x60)
                return 0x10;
            if (c == 0x5c || c == 0x7c)
                return 0x11;
            return 0x12;
        }
        return 0xe;
    }
    if (c == 0xc)
        return 2;
    if (c == 8)
        return 3;
    if (c == 0x10)
        return 4;
    if (c == 0x3c)
        return 5;
    if (t == '"')
        return 0xc;
    if (c == 0x38)
        return P(uint8_t, u, 0x147) == 'E' ? 0xb : 10;
    if (t == '#')
        return 0xd;
    if (c == 0x1c || c == 0x18 || c == 0x20 || c == 0x28 || c == 0x2c || c == 0x24)
        return 6;
    if (c == 0x30)
        return 7;
    if (c == 0x34)
        return 9;
    return 0;
}
