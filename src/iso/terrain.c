/* The game logic, from the AI's terrain analysis (0x10048270-0x1004b181): the parts
 * every game runs. The analysis finds corridors, key terrain ("KT"), areas of approach
 * ("AOA") and base locations on the sector grid. The shipped missions describe their
 * AOAs and key terrain in the scenario instead, so the analysis itself (still original)
 * is not reached by the tests: when the scenario loader starts it, the sector grid
 * is not set up yet, and it stops at once. */
#include "iso_core.h"
#include "terrain.h"

/* the list library (0x100c9cf0-0x100ca30a), not rewritten yet */
#define list_add ORIG(void *(__cdecl *)(void *, int, int, void *, int), 0x100c9cf0)
#define list_remove ORIG(void(__cdecl *)(void *, void *), 0x100c9efd)
#define list_first ORIG(void *(__cdecl *)(void *), 0x100ca0a9)
#define list_next ORIG(void *(__cdecl *)(void *, void *), 0x100ca1c8)
#define list_new ORIG(void *(__cdecl *)(void), 0x100ca2b9)
#define list_free ORIG(void(__cdecl *)(void *), 0x100ca30a)
#define orig_memset ORIG(void *(__cdecl *)(void *, int, uint32_t), 0x100f4c10)

/* the analysis' state, in the original's data */
#define ta_cols G(uint32_t, 0x10139e60)   /* the sector grid's width; 0: not set up */
#define ta_cells G(uint32_t, 0x10139e64)  /* sectors in all */
#define ta_given G(int32_t, 0x10139e68)   /* AOAs the scenario gave */
#define ta_near G(int32_t, 0x10139e6c)    /* how close two key terrains count as one */
#define ta_area G(int32_t, 0x10139e70)
#define ta_corridor G(uint32_t, 0x10139e74)
#define ta_min G(int32_t, 0x10139e78)
#define ta_done G(int32_t, 0x10139e7c)
#define ta_aoas G(void *, 0x10139e80)     /* list of AOAs (0x24 bytes) */
#define ta_kts G(void *, 0x10139e84)      /* list of key terrains (0xc bytes) */

#define SECTOR_ROW(s) ((uint32_t)(s) >> (I(uint8_t, 0xc2f5) & 0x1f))
#define SECTOR_COL(s) ((I(uint16_t, 0xc2fd) - 1u) & (uint32_t)(s))

/* 0x10049720 (Ghidra's library match "AfxResetMsgCache" is wrong): clears a 0x800-byte
 * table */
void __cdecl ta_clear_table(void)
{
    orig_memset(ORIG_PTR(0x1017bcf8), 0, 0x800);
}

/* 0x10049f66 */
int32_t __cdecl aoa_free(void **aoa)
{
    if (*aoa)
        list_free(*aoa);
    list_remove(ta_aoas, aoa);
    return 1;
}

/* 0x10048bf2 */
void __cdecl ta_free(void)
{
    if (ta_aoas) {
        void **a = list_first(ta_aoas);
        while (a) {
            void **next = list_next(ta_aoas, a);
            aoa_free(a);
            a = next;
        }
        list_free(ta_aoas);
        ta_aoas = NULL;
    }
    if (ta_kts) {
        list_free(ta_kts);
        ta_kts = NULL;
    }
    ta_done = 0;
    ta_given = 0;
    ta_cols = 0;
    ta_cells = 0;
}

/* 0x1004844c: the sizes the analysis works with, from the sector grid */
void __cdecl ta_init(void)
{
    ta_free();
    if (!I(uint16_t, 0xc2fd)) {
        DebugStr((const char *)ORIG_PTR(0x1013a000));
        ta_cols = 0;
        return;
    }
    ta_cols = I(uint16_t, 0xc2fd);
    ta_cells = I(uint32_t, 0xc2f1);
    uint32_t small = ta_cells / ta_cols, big = ta_cols;
    if (ta_cells / ta_cols > ta_cols) {
        small = ta_cols;
        big = ta_cells / ta_cols;
    }
    uint32_t c = big / 3;
    if (c <= 5)
        c = 5;
    if (c >= small)
        c = small;
    ta_corridor = c;
    ta_min = (int32_t)(ta_corridor * 0x50) / 100;
    if (ta_min <= 4)
        ta_min = 4;
    ta_area = (int32_t)(ta_corridor * 0x700) / 10;
    ta_aoas = list_new();
    ta_kts = list_new();
    ta_clear_table();
    ta_given = 0;
    ta_near = (ta_cols >> 4) > 1 ? (int32_t)(ta_cols >> 4) : 1;
    if (!ta_aoas || !ta_kts)
        DebugStr((const char *)ORIG_PTR(0x1013a03c));
}

/* 0x10049744: a new, empty AOA */
int32_t *__cdecl aoa_new(void)
{
    int32_t *a = NULL;
    if (ta_aoas)
        a = list_add(ta_aoas, 1, 0x24, NULL, 0);
    if (!a)
        return NULL;
    a[4] = 0;
    a[3] = 0;
    a[1] = 0;
    a[2] = 0;
    a[5] = 0;
    a[6] = 0;
    a[7] = 1;
    a[8] = 1;
    a[0] = (int32_t)(uintptr_t)list_new();
    if (!a[0])
        return NULL;
    return a;
}

/* 0x10049ee4: a sector added to an AOA's list (sector, row, column) */
uint32_t *__cdecl aoa_add_sector(int32_t *aoa, uint32_t sector)
{
    uint32_t *e = list_add((void *)(uintptr_t)aoa[0], 1, 0xc, NULL, 0);
    e[0] = sector;
    e[1] = SECTOR_ROW(sector);
    e[2] = SECTOR_COL(sector);
    aoa[3] = *(int32_t *)(uintptr_t)aoa[0];
    aoa[2] = (int32_t)sector;
    return e;
}

/* 0x1004a2e7: a key terrain of the type within ta_near sectors of `sector` */
int32_t *__cdecl kt_find(uint32_t sector, int32_t type)
{
    if (!ta_kts)
        return NULL;
    int32_t *k = list_first(ta_kts);
    int32_t row = (int32_t)SECTOR_ROW(sector), col = (int32_t)SECTOR_COL(sector);
    for (; k; k = list_next(ta_kts, k)) {
        if (k[0] != type)
            continue;
        int32_t dr = row - (int32_t)SECTOR_ROW(k[1]), dc = col - (int32_t)SECTOR_COL(k[1]);
        if ((dr < 0 ? -dr : dr) <= ta_near && (dc < 0 ? -dc : dc) <= ta_near)
            return k;
    }
    return NULL;
}

/* 0x1004a229: a key terrain, unless one of the type is already near */
int32_t *__cdecl kt_add(int32_t type, uint32_t sector, int32_t value)
{
    int32_t *k = kt_find(sector, type);
    if (k)
        return k;
    if (ta_kts)
        k = list_add(ta_kts, 1, 0xc, NULL, 0);
    if (!k)
        return NULL;
    k[0] = type;
    k[1] = (int32_t)sector;
    k[2] = value;
    DebugStr((const char *)ORIG_PTR(0x1013a224), type, sector, SECTOR_ROW(sector), SECTOR_COL(sector));
    return k;
}

/* 0x1004b03a: an AOA from a scenario record (its values at +10: four fields, the
 * sectors from the sixth on, up to 15, ending at 0) */
int32_t __cdecl sdl_aoa(uint8_t *rec)
{
    if (!ta_given && !ta_cols)
        ta_init();
    int32_t *a = aoa_new();
    if (!a)
        return 0;
    a[4] = P(int32_t, rec, 10);
    a[5] = P(int32_t, rec, 0xe);
    a[6] = P(int32_t, rec, 0x12);
    a[7] = P(int32_t, rec, 0x16);
    a[8] = P(int32_t, rec, 0x1a);
    a[1] = P(int32_t, rec, 0x1e);
    for (int i = 5; i < 0x14 && P(int32_t, rec, 10 + i * 4); i++)
        aoa_add_sector(a, P(uint32_t, rec, 10 + i * 4));
    ta_given++;
    return 1;
}

/* 0x1004b119: a key terrain from a scenario record (type, sector, value) */
int32_t __cdecl sdl_kt(uint8_t *rec)
{
    if (!ta_given && !ta_cols)
        ta_init();
    return kt_add(P(int32_t, rec, 0xe), P(uint32_t, rec, 10), P(int32_t, rec, 0x12)) != NULL;
}
