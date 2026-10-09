// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_ring_buffer.h"

#include <stdio.h>
#include <string.h>


// Only pif_ring_buffer.c is linked, so the globals normally defined in pif.c live here.
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
static int s_lock_depth;
static int s_lock_count;


static void _actLock(PifRingBuffer *p_owner, BOOL lock)
{
    (void)p_owner;
    if (lock) {
        s_lock_depth++;
        s_lock_count++;
    }
    else {
        s_lock_depth--;
    }
}

static BOOL _putBytes(PifRingBuffer *p_rb, uint8_t first, uint16_t count)
{
    uint16_t i;

    for (i = 0; i < count; i++) {
        if (!pifRingBuffer_PutByte(p_rb, (uint8_t)(first + i))) return FALSE;
    }
    return TRUE;
}

static void _testWrapAround(void)
{
    PifRingBuffer rb;
    uint8_t data[8];
    uint16_t i, round;

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    CHECK(pifRingBuffer_GetRemainSize(&rb) == 7);
    for (round = 0; round < 5; round++) {
        CHECK(_putBytes(&rb, (uint8_t)(round * 10), 5));
        CHECK(pifRingBuffer_GetFillSize(&rb) == 5);
        CHECK(pifRingBuffer_GetRemainSize(&rb) == 2);
        CHECK(pifRingBuffer_GetBytes(&rb, data, 5) == 5);
        for (i = 0; i < 5; i++) CHECK(data[i] == round * 10 + i);
        CHECK(pifRingBuffer_IsEmpty(&rb));
    }
    CHECK(_putBytes(&rb, 0, 7));
    CHECK(!pifRingBuffer_PutByte(&rb, 7));
    pifRingBuffer_Clear(&rb);
}

static void _testPartialGetBytes(void)
{
    PifRingBuffer rb;
    uint8_t data[8];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    CHECK(_putBytes(&rb, 1, 3));
    // Fewer bytes than asked for are still taken out of the buffer.
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 3);
    CHECK(data[0] == 1 && data[2] == 3);
    CHECK(pifRingBuffer_IsEmpty(&rb));
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 0);
    pifRingBuffer_Clear(&rb);
}

static void _testSessionHidden(void)
{
    PifRingBuffer rb;
    uint8_t data[8];
    uint8_t body[4] = { 0, 0xB1, 0xB2, 0xB3 };

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 16));
    CHECK(_putBytes(&rb, 0xA0, 2));

    pifRingBuffer_BeginPutting(&rb);
    CHECK(pifRingBuffer_PutData(&rb, body, 4));
    // The reader sees only what was there before the session.
    CHECK(pifRingBuffer_GetFillSize(&rb) == 2);
    CHECK(pifRingBuffer_GetRemainSize(&rb) == 15 - 6);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 2);
    CHECK(pifRingBuffer_IsEmpty(&rb));

    *pifRingBuffer_GetPointerPutting(&rb, 0) = 0xB0;
    pifRingBuffer_CommitPutting(&rb);
    CHECK(pifRingBuffer_GetFillSize(&rb) == 4);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 4);
    CHECK(data[0] == 0xB0 && data[3] == 0xB3);

    // Puts after the commit are visible at once again.
    CHECK(pifRingBuffer_PutByte(&rb, 0xC0));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 1);
    pifRingBuffer_Clear(&rb);
}

static void _testSessionRollback(void)
{
    PifRingBuffer rb;
    uint8_t data[8];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));

    // Outside a session a rollback does nothing.
    pifRingBuffer_RollbackPutting(&rb);
    CHECK(pifRingBuffer_IsEmpty(&rb));
    CHECK(pifRingBuffer_GetRemainSize(&rb) == 7);

    CHECK(_putBytes(&rb, 1, 2));
    pifRingBuffer_BeginPutting(&rb);
    CHECK(_putBytes(&rb, 0x10, 5));
    // Full counting the session, so the next byte has no room.
    CHECK(!pifRingBuffer_PutByte(&rb, 0x15));
    pifRingBuffer_RollbackPutting(&rb);
    CHECK(pifRingBuffer_GetFillSize(&rb) == 2);
    CHECK(pifRingBuffer_GetRemainSize(&rb) == 5);

    // A second begin drops the unfinished first session.
    pifRingBuffer_BeginPutting(&rb);
    CHECK(_putBytes(&rb, 0x20, 3));
    pifRingBuffer_BeginPutting(&rb);
    CHECK(_putBytes(&rb, 0x30, 1));
    pifRingBuffer_CommitPutting(&rb);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 3);
    CHECK(data[0] == 1 && data[1] == 2 && data[2] == 0x30);
    pifRingBuffer_Clear(&rb);
}

static void _testChopOffLength(void)
{
    PifRingBuffer rb;
    uint8_t data[16];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 10));
    pifRingBuffer_ChopsOffLength(&rb, 3);
    pifRingBuffer_AttachActLock(&rb, _actLock);
    CHECK(_putBytes(&rb, 0, 9));
    s_lock_count = 0;
    CHECK(_putBytes(&rb, 9, 2));
    CHECK(s_lock_count > 0 && s_lock_depth == 0);
    // Three oldest bytes went to make room for the two new ones.
    CHECK(pifRingBuffer_GetBytes(&rb, data, 16) == 8);
    CHECK(data[0] == 3 && data[7] == 10);

    // A zero length cannot drop anything and must not hang.
    pifRingBuffer_ChopsOffLength(&rb, 0);
    CHECK(_putBytes(&rb, 0, 9));
    CHECK(!pifRingBuffer_PutByte(&rb, 9));
    CHECK(s_lock_depth == 0);
    pifRingBuffer_Clear(&rb);
}

static void _testChopOffLengthSession(void)
{
    PifRingBuffer rb;
    uint8_t data[16];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 10));
    pifRingBuffer_ChopsOffLength(&rb, 2);
    CHECK(_putBytes(&rb, 0, 4));
    pifRingBuffer_BeginPutting(&rb);
    CHECK(_putBytes(&rb, 0x10, 5));
    // Only committed bytes can be dropped for the session.
    CHECK(_putBytes(&rb, 0x15, 2));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 2);
    CHECK(_putBytes(&rb, 0x17, 2));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 0);
    CHECK(!pifRingBuffer_PutByte(&rb, 0x19));
    pifRingBuffer_CommitPutting(&rb);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 16) == 9);
    CHECK(data[0] == 0x10 && data[8] == 0x18);
    pifRingBuffer_Clear(&rb);
}

static void _testChopOffChar(void)
{
    PifRingBuffer rb;
    char text[16];
    uint16_t length;

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 12));
    pifRingBuffer_ChopsOffChar(&rb, '$');
    CHECK(pifRingBuffer_PutString(&rb, "$ab$cd$ef"));
    CHECK(pifRingBuffer_PutString(&rb, "gh"));
    CHECK(pifRingBuffer_PutString(&rb, "ij"));
    length = pifRingBuffer_GetBytes(&rb, (uint8_t *)text, sizeof(text) - 1);
    text[length] = 0;
    CHECK(!strcmp(text, "$cd$efghij"));

    // Without a delimiter far enough in, nothing can be dropped.
    pifRingBuffer_Empty(&rb);
    CHECK(pifRingBuffer_PutString(&rb, "abcdefghijk"));
    CHECK(!pifRingBuffer_PutString(&rb, "x"));
    pifRingBuffer_Clear(&rb);
}

static void _testLockBalance(void)
{
    PifRingBuffer rb;
    uint8_t data[4];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    pifRingBuffer_AttachActLock(&rb, _actLock);
    s_lock_count = 0;
    CHECK(_putBytes(&rb, 0, 3));
    // Plain puts never touch the tail.
    CHECK(s_lock_count == 0);
    CHECK(pifRingBuffer_GetByte(&rb, data));
    CHECK(pifRingBuffer_GetBytes(&rb, data, 4) == 2);
    CHECK(!pifRingBuffer_GetByte(&rb, data));
    pifRingBuffer_Remove(&rb, 1);
    pifRingBuffer_Empty(&rb);
    CHECK(s_lock_count == 5 && s_lock_depth == 0);
    pifRingBuffer_Clear(&rb);
}

static void _testMoveHead(void)
{
    PifRingBuffer rb;
    uint8_t data[8];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    CHECK(_putBytes(&rb, 0, 6));
    CHECK(pifRingBuffer_GetBytes(&rb, data, 6) == 6);
    // Only two bytes are left before the end, so the empty buffer starts over.
    CHECK(pifRingBuffer_MoveHeadForLinear(&rb, 4));
    CHECK(pifRingBuffer_IsEmpty(&rb));
    memcpy(pifRingBuffer_GetTailPointer(&rb, 0), "\x01\x02\x03\x04", 4);
    CHECK(pifRingBuffer_SetHead(&rb, 4));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 4 && *pifRingBuffer_GetTailPointer(&rb, 3) == 4);
    CHECK(!pifRingBuffer_MoveHeadForLinear(&rb, 2));
    pifRingBuffer_Clear(&rb);
}

static void _testSetHead(void)
{
    PifRingBuffer rb;
    uint8_t data[8];

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    CHECK(!pifRingBuffer_SetHead(&rb, 8));
    CHECK(pifRingBuffer_SetHead(&rb, 0) && pifRingBuffer_IsEmpty(&rb));

    // A DMA position that wrapped past the end of memory.
    memcpy(pifRingBuffer_GetBuffer(&rb), "\x10\x11\x12\x13\x14\x15", 6);
    CHECK(pifRingBuffer_SetHead(&rb, 6));
    CHECK(*pifRingBuffer_GetTailPointer(&rb, 5) == 0x15);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 4) == 4);
    CHECK(pifRingBuffer_SetHead(&rb, 1));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 5);

    // Filling up to one byte short of the tail is the most that fits.
    CHECK(pifRingBuffer_SetHead(&rb, 3));
    CHECK(pifRingBuffer_GetRemainSize(&rb) == 0);

    // Running over the tail keeps the newest bytes.
    CHECK(!pifRingBuffer_SetHead(&rb, 5));
    CHECK(pifRingBuffer_GetFillSize(&rb) == 7);
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 7);
    CHECK(pifRingBuffer_IsEmpty(&rb));
    pifRingBuffer_Clear(&rb);
}

static void _testReadPointer(void)
{
    PifRingBuffer rb;
    uint8_t *p_data;
    uint16_t length;

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    p_data = pifRingBuffer_GetReadPointer(&rb, 0, &length);
    CHECK(p_data && length == 0);

    // Tail at 5 with six bytes: three before the end of memory, three after the wrap.
    CHECK(_putBytes(&rb, 0, 5));
    pifRingBuffer_Remove(&rb, 5);
    CHECK(_putBytes(&rb, 10, 6));
    p_data = pifRingBuffer_GetReadPointer(&rb, 0, &length);
    CHECK(length == 3 && p_data[0] == 10 && p_data[2] == 12);
    p_data = pifRingBuffer_GetReadPointer(&rb, 3, &length);
    CHECK(length == 3 && p_data[0] == 13 && p_data[2] == 15);
    p_data = pifRingBuffer_GetReadPointer(&rb, 1, &length);
    CHECK(length == 2 && p_data[0] == 11);
    pifRingBuffer_GetReadPointer(&rb, 6, &length);
    CHECK(length == 0);
    pifRingBuffer_Clear(&rb);
}

static void _testReadPointerClaim(void)
{
    PifRingBuffer rb;
    uint8_t *p_data;
    uint8_t data[8];
    uint16_t length;

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 8));
    pifRingBuffer_ChopsOffLength(&rb, 1);
    CHECK(_putBytes(&rb, 0, 7));

    // While bytes are out, a chop-off would hand them to the writer, so the put fails.
    p_data = pifRingBuffer_GetReadPointer(&rb, 0, &length);
    CHECK(length == 7);
    CHECK(!pifRingBuffer_PutByte(&rb, 7));
    CHECK(p_data[0] == 0 && pifRingBuffer_GetFillSize(&rb) == 7);

    // Removing part of them ends the claim, and dropping works again.
    pifRingBuffer_Remove(&rb, 2);
    CHECK(_putBytes(&rb, 7, 3));
    CHECK(pifRingBuffer_PutByte(&rb, 10));
    CHECK(pifRingBuffer_GetBytes(&rb, data, 8) == 7);
    CHECK(data[0] == 4 && data[6] == 10);

    // Taking the claimed bytes with GetBytes also gives them back.
    CHECK(_putBytes(&rb, 0, 7));
    pifRingBuffer_GetReadPointer(&rb, 0, &length);
    CHECK(pifRingBuffer_GetBytes(&rb, data, length) == length);
    CHECK(_putBytes(&rb, 0, 7));
    CHECK(pifRingBuffer_PutByte(&rb, 7));
    pifRingBuffer_Clear(&rb);
}

static void _testEdgeCases(void)
{
    PifRingBuffer rb;
    PifRingBuffer *p_rb;
    uint8_t data[4];

    CHECK(pifRingBuffer_CreateHeap(PIF_ID_AUTO, 0) == NULL);
    p_rb = pifRingBuffer_CreateHeap(PIF_ID_AUTO, 4);
    CHECK(p_rb);
    pifRingBuffer_Destroy(&p_rb);
    CHECK(!p_rb);

    CHECK(pifRingBuffer_InitHeap(&rb, PIF_ID_AUTO, 4));
    CHECK(_putBytes(&rb, 1, 2));
    CHECK(!pifRingBuffer_ResizeHeap(&rb, 0));
    CHECK(pifRingBuffer_IsBuffer(&rb) && rb._size == 4);
    CHECK(pifRingBuffer_CopyToArray(data, 4, &rb, 2) == 0);
    CHECK(pifRingBuffer_CopyToArray(data, 4, &rb, 1) == 1 && data[0] == 2);
    CHECK(pifRingBuffer_ResizeHeap(&rb, 6));
    CHECK(pifRingBuffer_IsEmpty(&rb) && pifRingBuffer_GetRemainSize(&rb) == 5);
    CHECK(!pifRingBuffer_PutString(&rb, "123456"));
    CHECK(pifRingBuffer_PutString(&rb, "12345"));
    pifRingBuffer_Clear(&rb);
}

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "wrap around", _testWrapAround },
    { "partial get bytes", _testPartialGetBytes },
    { "session hidden", _testSessionHidden },
    { "session rollback", _testSessionRollback },
    { "chop off length", _testChopOffLength },
    { "chop off length in session", _testChopOffLengthSession },
    { "chop off char", _testChopOffChar },
    { "lock balance", _testLockBalance },
    { "move head for linear", _testMoveHead },
    { "set head", _testSetHead },
    { "read pointer", _testReadPointer },
    { "read pointer claim", _testReadPointerClaim },
    { "edge cases", _testEdgeCases },
};

int main(void)
{
    int failed = 0;
    size_t i;

    printf("pif_ring_buffer\n");
    for (i = 0; i < sizeof(s_tests) / sizeof(s_tests[0]); i++) {
        s_failed = FALSE;
        (*s_tests[i].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
