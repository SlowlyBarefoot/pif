// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_ring_data.h"

#include <stdio.h>
#include <string.h>


// Only pif_ring_data.c is linked, so the globals normally defined in pif.c live here.
PifError pif_error = E_SUCCESS;
PifId pif_id = 1;

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;


static BOOL _add(PifRingData *p_rd, uint32_t value)
{
    uint32_t *p_item = pifRingData_Add(p_rd);

    if (!p_item) return FALSE;
    *p_item = value;
    return TRUE;
}

static void _testCapacity(void)
{
    PifRingData rd;
    uint32_t i;

    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 4));
    CHECK(pifRingData_IsEmpty(&rd));
    CHECK(pifRingData_GetRemainSize(&rd) == 4);
    for (i = 0; i < 4; i++) CHECK(_add(&rd, i));
    CHECK(pifRingData_GetFillSize(&rd) == 4);
    CHECK(pifRingData_GetRemainSize(&rd) == 0);
    pif_error = E_SUCCESS;
    CHECK(!pifRingData_Add(&rd));
    CHECK(pif_error == E_OVERFLOW_BUFFER);
    pifRingData_Clear(&rd);

    // A single-item ring must hold one item.
    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 1));
    CHECK(_add(&rd, 7));
    CHECK(!_add(&rd, 8));
    CHECK(*(uint32_t *)pifRingData_Remove(&rd) == 7);
    CHECK(pifRingData_IsEmpty(&rd));
    pifRingData_Clear(&rd);
}

static void _testFifoWrapAround(void)
{
    PifRingData rd;
    uint32_t round, i;

    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 3));
    for (round = 0; round < 5; round++) {
        for (i = 0; i < 3; i++) CHECK(_add(&rd, round * 10 + i));
        for (i = 0; i < 3; i++) {
            uint32_t *p_item = pifRingData_Remove(&rd);
            CHECK(p_item && *p_item == round * 10 + i);
        }
        CHECK(pifRingData_IsEmpty(&rd));
    }
    pif_error = E_SUCCESS;
    CHECK(!pifRingData_Remove(&rd));
    CHECK(pif_error == E_EMPTY_IN_BUFFER);
    pifRingData_Clear(&rd);
}

static void _testGetData(void)
{
    PifRingData rd;
    uint32_t i;

    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 4));
    CHECK(!pifRingData_GetData(&rd, 0));

    // Move tail so that the stored items wrap around the buffer end.
    for (i = 0; i < 3; i++) CHECK(_add(&rd, 100));
    for (i = 0; i < 3; i++) CHECK(pifRingData_Remove(&rd));
    for (i = 0; i < 4; i++) CHECK(_add(&rd, i));

    for (i = 0; i < 4; i++) {
        uint32_t *p_item = pifRingData_GetData(&rd, i);
        CHECK(p_item && *p_item == i);
    }
    CHECK(!pifRingData_GetData(&rd, 4));
    pifRingData_Clear(&rd);
}

static void _testIteration(void)
{
    PifRingData rd;
    uint32_t *p_item;
    uint32_t i, count;

    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 4));
    CHECK(!pifRingData_GetFirstData(&rd));
    CHECK(!pifRingData_GetNextData(&rd));

    for (i = 0; i < 2; i++) CHECK(_add(&rd, 0));
    for (i = 0; i < 2; i++) CHECK(pifRingData_Remove(&rd));
    for (i = 0; i < 4; i++) CHECK(_add(&rd, i));

    count = 0;
    p_item = pifRingData_GetFirstData(&rd);
    while (p_item) {
        CHECK(*p_item == count);
        count++;
        p_item = pifRingData_GetNextData(&rd);
    }
    CHECK(count == 4);

    // Calling again after the end must not return stale items.
    CHECK(!pifRingData_GetNextData(&rd));
    CHECK(!pifRingData_GetNextData(&rd));
    pifRingData_Clear(&rd);
}

static void _testResetAndClear(void)
{
    PifRingData rd;

    CHECK(pifRingData_Init(&rd, PIF_ID_AUTO, sizeof(uint32_t), 3));
    CHECK(_add(&rd, 1));
    CHECK(_add(&rd, 2));
    pifRingData_Reset(&rd);
    CHECK(pifRingData_IsEmpty(&rd));
    CHECK(pifRingData_GetRemainSize(&rd) == 3);
    CHECK(_add(&rd, 3));
    CHECK(*(uint32_t *)pifRingData_Remove(&rd) == 3);

    CHECK(_add(&rd, 4));
    pifRingData_Clear(&rd);
    CHECK(pifRingData_IsEmpty(&rd));
    pif_error = E_SUCCESS;
    CHECK(!pifRingData_Add(&rd));
    CHECK(pif_error == E_INVALID_STATE);
}

static void _testInvalidParam(void)
{
    PifRingData *p_rd;

    CHECK(!pifRingData_Create(PIF_ID_AUTO, 0, 4));
    CHECK(!pifRingData_Create(PIF_ID_AUTO, 4, 0));
    CHECK(!pifRingData_Create(PIF_ID_AUTO, 4, UINT16_MAX));
    CHECK(!pifRingData_Init(NULL, PIF_ID_AUTO, 4, 4));

    p_rd = pifRingData_Create(PIF_ID_AUTO, sizeof(uint32_t), 2);
    CHECK(p_rd);
    CHECK(p_rd->_data_count == 2);
    pifRingData_Destroy(&p_rd);
    CHECK(!p_rd);
}

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "capacity", _testCapacity },
    { "fifo wrap around", _testFifoWrapAround },
    { "get data", _testGetData },
    { "iteration", _testIteration },
    { "reset and clear", _testResetAndClear },
    { "invalid param", _testInvalidParam },
};

int main(void)
{
    int failed = 0;
    size_t i;

    printf("pif_ring_data\n");
    for (i = 0; i < sizeof(s_tests) / sizeof(s_tests[0]); i++) {
        s_failed = FALSE;
        (*s_tests[i].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
