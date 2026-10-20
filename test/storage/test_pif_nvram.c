// SPDX-License-Identifier: BSD-3-Clause
// pif_nvram over media in memory: raw slots, records kept safe from a write cut short, and transfers carried by
// the timer.
#include "core/pif_task_manager.h"
#include "storage/pif_nvram.h"

#include <stdio.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define VOLUME			256
#define PAGE_SIZE		8
#define SLOT_SIZE		32

static BOOL s_failed;
static uint32_t s_now_us;

static PifNvram s_nvram;
static PifTimerManager s_manager;

static uint8_t s_media[VOLUME];
static int s_write_count, s_read_count;
static int s_fail_write;				// The write with this number fails, 0 for none.
static BOOL s_fail_torn;				// and puts half of its bytes in first.
static BOOL s_crossed_page;				// A write crossed a page boundary.
static int s_done_count;
static BOOL s_done_result;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static BOOL _actRead(PifNvram* p_owner, uint8_t* p_dst, uint32_t src, size_t size)
{
    (void)p_owner;

    s_read_count++;
    if (src + size > VOLUME) return FALSE;
    memcpy(p_dst, s_media + src, size);
    return TRUE;
}

static BOOL _actWrite(PifNvram* p_owner, uint32_t dst, uint8_t* p_src, size_t size)
{
    (void)p_owner;

    s_write_count++;
    if (dst + size > VOLUME) return FALSE;
    if (dst / PAGE_SIZE != (dst + size - 1) / PAGE_SIZE) s_crossed_page = TRUE;
    if (s_write_count == s_fail_write) {
        if (s_fail_torn) memcpy(s_media + dst, p_src, size / 2);
        return FALSE;
    }
    memcpy(s_media + dst, p_src, size);
    return TRUE;
}

static void _evtDone(PifNvram* p_owner, BOOL result)
{
    (void)p_owner;

    s_done_count++;
    s_done_result = result;
}

// One tick of the interrupt, then one pass of the main loop.
static void _tick(int count)
{
    while (count--) {
        pifTimerManager_sigTick(&s_manager);
        pifTaskManager_Loop();
    }
}

// An NVRAM of the given mode over media that reads as erased.
static BOOL _setUp(PifNvramMode mode, BOOL timer)
{
    // A failed test returns early, so free what it left behind.
    pifNvram_Clear(&s_nvram);
    pifTimerManager_Clear(&s_manager);

    memset(s_media, 0xFF, sizeof(s_media));
    s_write_count = s_read_count = 0;
    s_fail_write = 0;
    s_fail_torn = FALSE;
    s_crossed_page = FALSE;
    s_done_count = 0;

    if (!pifNvram_Init(&s_nvram, PIF_ID_AUTO)) return FALSE;
    if (!pifNvram_AttachActMedia(&s_nvram, _actRead, _actWrite)) return FALSE;
    if (!pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, SLOT_SIZE, mode)) return FALSE;
    if (timer) {
        if (!pifTimerManager_Init(&s_manager, PIF_ID_AUTO, 1000, 2)) return FALSE;
        if (!pifNvram_AttachTimer(&s_nvram, &s_manager, _evtDone)) return FALSE;
    }
    return TRUE;
}

#ifdef PIF_USE_NVRAM_RECORD

// A new instance over the same media, as after a reset.
static BOOL _restart(void)
{
    pifNvram_Clear(&s_nvram);
    if (!pifNvram_Init(&s_nvram, PIF_ID_AUTO)) return FALSE;
    if (!pifNvram_AttachActMedia(&s_nvram, _actRead, _actWrite)) return FALSE;
    return pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, SLOT_SIZE, NM_RECORD);
}

#endif

static void _fill(uint8_t* p_data, size_t size, uint8_t seed)
{
    size_t i;

    for (i = 0; i < size; i++) p_data[i] = (uint8_t)(seed + i);
}

static void testSetMedia(void)
{
    CHECK(_setUp(NM_RAW, FALSE));
    CHECK(s_nvram._slot_size == SLOT_SIZE && s_nvram._slot_count == VOLUME / SLOT_SIZE);

    CHECK(!pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, 0, NM_RAW) && pif_error == E_INVALID_PARAM);
    CHECK(!pifNvram_SetMedia(&s_nvram, 16, PAGE_SIZE, 32, NM_RAW) && pif_error == E_INVALID_PARAM);
#ifdef PIF_USE_NVRAM_RECORD
    // A record slot has to hold more than the header, and a record needs two slots.
    CHECK(!pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, PIF_NVRAM_RECORD_HEADER, NM_RECORD));
    CHECK(!pifNvram_SetMedia(&s_nvram, 40, PAGE_SIZE, 32, NM_RECORD) && pif_error == E_INVALID_PARAM);
    CHECK(pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, SLOT_SIZE, NM_RECORD));
    CHECK(s_nvram._mode == NM_RECORD);
#else
    {
        static const uint16_t sizes[] = { 4, 20 };

        // The mode left out of the build is refused, and the raw slots set up before are kept.
        pif_error = E_SUCCESS;
        CHECK(!pifNvram_SetMedia(&s_nvram, VOLUME, PAGE_SIZE, SLOT_SIZE, NM_RECORD) && pif_error == E_INVALID_PARAM);
        pif_error = E_SUCCESS;
        CHECK(!pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, sizes, 2, NM_RECORD) && pif_error == E_INVALID_PARAM);
        CHECK(s_nvram._mode == NM_RAW && s_nvram._slot_count == VOLUME / SLOT_SIZE);
    }
#endif
}

static void testRaw(void)
{
    uint8_t data[SLOT_SIZE + 1], read[SLOT_SIZE];

    CHECK(_setUp(NM_RAW, FALSE));

    _fill(data, sizeof(data), 0x10);
    CHECK(pifNvram_Write(&s_nvram, 2, data, SLOT_SIZE) == NS_START_DONE);
    CHECK(memcmp(s_media + 2 * SLOT_SIZE, data, SLOT_SIZE) == 0);
    // A slot of four pages goes as four writes, none of them across a page boundary.
    CHECK(s_write_count == SLOT_SIZE / PAGE_SIZE && !s_crossed_page);

    memset(read, 0, sizeof(read));
    CHECK(pifNvram_Read(&s_nvram, 2, read, SLOT_SIZE) == NS_START_DONE);
    CHECK(memcmp(read, data, SLOT_SIZE) == 0);

    // Nothing runs into the next slot, and nothing past the last one.
    CHECK(pifNvram_Write(&s_nvram, 2, data, SLOT_SIZE + 1) == NS_START_FAILURE && pif_error == E_INVALID_PARAM);
    CHECK(pifNvram_Write(&s_nvram, VOLUME / SLOT_SIZE, data, 1) == NS_START_FAILURE && pif_error == E_INVALID_ID);

#ifdef PIF_USE_NVRAM_RECORD
    // The record functions are for the other mode.
    CHECK(!pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && pif_error == E_CANNOT_USE);
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, 4) == NS_START_FAILURE && pif_error == E_CANNOT_USE);
#endif
}

static void testRawTimed(void)
{
    uint8_t data[SLOT_SIZE], read[SLOT_SIZE];

    CHECK(_setUp(NM_RAW, TRUE));

    _fill(data, sizeof(data), 0x20);
    // The first page goes at once and the rest a page per tick.
    CHECK(pifNvram_Write(&s_nvram, 1, data, SLOT_SIZE) == NS_START_TIMED);
    CHECK(s_nvram._state == NS_TRANSFER && s_write_count == 1);
    CHECK(pifNvram_Write(&s_nvram, 3, data, 4) == NS_START_FAILURE && pif_error == E_INVALID_STATE);
    _tick(1);
    CHECK(s_write_count == 2 && s_done_count == 0);
    _tick(2);
    CHECK(s_write_count == 4 && s_done_count == 1 && s_done_result);
    CHECK(s_nvram._state == NS_IDLE);
    CHECK(memcmp(s_media + SLOT_SIZE, data, SLOT_SIZE) == 0);

    // A transfer of one page is over without the timer.
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_Read(&s_nvram, 1, read, PAGE_SIZE) == NS_START_DONE);
    CHECK(memcmp(read, data, PAGE_SIZE) == 0);

    // A page that fails ends the transfer with the event.
    s_fail_write = s_write_count + 2;
    CHECK(pifNvram_Write(&s_nvram, 1, data, SLOT_SIZE) == NS_START_TIMED);
    _tick(4);
    CHECK(s_done_count == 2 && !s_done_result && s_nvram._state == NS_IDLE);
}

#ifdef PIF_USE_NVRAM_RECORD

static void testRecord(void)
{
    uint8_t data[20], read[20];
    uint16_t size;

    CHECK(_setUp(NM_RECORD, FALSE));

    // Never written, so neither copy is intact.
    CHECK(!pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && pif_error == E_CANNOT_FOUND);

    _fill(data, sizeof(data), 0x30);
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, sizeof(data)) == NS_START_DONE);
    CHECK(!s_crossed_page);
    memset(read, 0, sizeof(read));
    size = 0;
    CHECK(pifNvram_ReadRecord(&s_nvram, 1, read, sizeof(read), &size));
    CHECK(size == sizeof(data) && memcmp(read, data, sizeof(data)) == 0);

    // The copies take turns: the first write went to slot 2, the next goes to slot 3, then 2 again.
    CHECK(s_media[2 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER] == 0x30);
    CHECK(s_media[3 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER] == 0xFF);
    _fill(data, sizeof(data), 0x40);
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, sizeof(data)) == NS_START_DONE);
    CHECK(s_media[2 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER] == 0x30);
    CHECK(s_media[3 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER] == 0x40);
    _fill(data, sizeof(data), 0x50);
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, sizeof(data)) == NS_START_DONE);
    CHECK(s_media[2 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER] == 0x50);

    // After a reset the newest copy is found again.
    CHECK(_restart());
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 1, read, sizeof(read), NULL));
    CHECK(memcmp(read, data, sizeof(data)) == 0);

    // The other records are untouched.
    CHECK(!pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && pif_error == E_CANNOT_FOUND);

    // Too long for a slot, and past the last record.
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, SLOT_SIZE - PIF_NVRAM_RECORD_HEADER + 1) == NS_START_FAILURE);
    CHECK(pifNvram_WriteRecord(&s_nvram, VOLUME / SLOT_SIZE / 2, data, 4) == NS_START_FAILURE && pif_error == E_INVALID_ID);
    // The raw functions are for the other mode.
    CHECK(pifNvram_Read(&s_nvram, 0, read, 4) == NS_START_FAILURE && pif_error == E_CANNOT_USE);
}

static void testRecordCutShort(void)
{
    uint8_t first[20], second[20], read[20];
    int fail;

    // Whichever step of the write is cut short, and however, the record reads as the one before it.
    for (fail = 1; fail <= 4; fail++) {
        CHECK(_setUp(NM_RECORD, FALSE));
        _fill(first, sizeof(first), 0x60);
        _fill(second, sizeof(second), 0x70);
        CHECK(pifNvram_WriteRecord(&s_nvram, 0, first, sizeof(first)) == NS_START_DONE);

        s_fail_write = s_write_count + fail;
        s_fail_torn = fail & 1;
        CHECK(pifNvram_WriteRecord(&s_nvram, 0, second, sizeof(second)) == NS_START_FAILURE);

        // Both from what the instance knows and from the media alone.
        memset(read, 0, sizeof(read));
        CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL));
        CHECK(memcmp(read, first, sizeof(first)) == 0);
        CHECK(_restart());
        memset(read, 0, sizeof(read));
        CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL));
        CHECK(memcmp(read, first, sizeof(first)) == 0);

        // The next write goes over the broken copy, not the intact one.
        s_fail_write = 0;
        CHECK(pifNvram_WriteRecord(&s_nvram, 0, second, sizeof(second)) == NS_START_DONE);
        CHECK(memcmp(s_media + PIF_NVRAM_RECORD_HEADER, first, sizeof(first)) == 0);
        memset(read, 0, sizeof(read));
        CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL));
        CHECK(memcmp(read, second, sizeof(second)) == 0);
    }
}

static void testRecordCorrupted(void)
{
    uint8_t first[10], second[10], read[10];

    CHECK(_setUp(NM_RECORD, FALSE));
    _fill(first, sizeof(first), 0x80);
    _fill(second, sizeof(second), 0x90);
    CHECK(pifNvram_WriteRecord(&s_nvram, 2, first, sizeof(first)) == NS_START_DONE);
    CHECK(pifNvram_WriteRecord(&s_nvram, 2, second, sizeof(second)) == NS_START_DONE);

    // A bit of the latest copy goes bad after it was written, and the copy before it is read instead.
    s_media[5 * SLOT_SIZE + PIF_NVRAM_RECORD_HEADER + 3] ^= 0x04;
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 2, read, sizeof(read), NULL));
    CHECK(memcmp(read, first, sizeof(first)) == 0);

    // Both bad: the buffer is left as it was.
    s_media[4 * SLOT_SIZE + 1] ^= 0x01;
    memset(read, 0xA5, sizeof(read));
    CHECK(!pifNvram_ReadRecord(&s_nvram, 2, read, sizeof(read), NULL) && pif_error == E_CANNOT_FOUND);
    CHECK(read[0] == 0xA5 && read[sizeof(read) - 1] == 0xA5);
}

static void testRecordSequenceWraps(void)
{
    uint8_t data[8], read[8];
    uint16_t seq;

    CHECK(_setUp(NM_RECORD, FALSE));
    _fill(data, sizeof(data), 0xA0);
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, sizeof(data)) == NS_START_DONE);
    // Pretend that copy was the 65535th write, so the next one numbers 0.
    s_nvram.__p_record[0].seq = 0xFFFF;
    _fill(data, sizeof(data), 0xB0);
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, sizeof(data)) == NS_START_DONE);
    seq = s_media[SLOT_SIZE] | (s_media[SLOT_SIZE + 1] << 8);
    CHECK(seq == 0);

    // Then slot 0 is written as the 65535th: slot 0 holds 0xFFFF and slot 1 holds 0.
    _fill(data, sizeof(data), 0xA0);
    CHECK(_restart());
    s_nvram.__p_record[0].slot = 1;
    s_nvram.__p_record[0].seq = 0xFFFE;
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, sizeof(data)) == NS_START_DONE);
    seq = s_media[0] | (s_media[1] << 8);
    CHECK(seq == 0xFFFF);
    // 0 comes after 0xFFFF, so slot 1 is the latest.
    CHECK(_restart());
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL));
    CHECK(read[0] == 0xB0);
}

static void testRecordLength(void)
{
    uint8_t data[12], read[16];
    uint16_t size;

    CHECK(_setUp(NM_RECORD, FALSE));
    _fill(data, sizeof(data), 0xC0);
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, 8) == NS_START_DONE);

    // A record shorter than the buffer leaves the rest of it, such as defaults, as it was.
    memset(read, 0xEE, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), &size));
    CHECK(size == 8 && memcmp(read, data, 8) == 0 && read[8] == 0xEE && read[15] == 0xEE);

    // A longer one fills the buffer and says how long it is.
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, sizeof(data)) == NS_START_DONE);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, 4, &size));
    CHECK(size == sizeof(data) && memcmp(read, data, 4) == 0 && read[4] == 0);

    // A record of no bytes is a record all the same.
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, NULL, 0) == NS_START_DONE);
    CHECK(pifNvram_ReadRecord(&s_nvram, 1, read, sizeof(read), &size) && size == 0);
}

static void testRecordDelete(void)
{
    uint8_t data[8], read[8];

    CHECK(_setUp(NM_RECORD, FALSE));
    _fill(data, sizeof(data), 0xD0);
    CHECK(pifNvram_WriteRecord(&s_nvram, 3, data, sizeof(data)) == NS_START_DONE);
    CHECK(pifNvram_WriteRecord(&s_nvram, 3, data, sizeof(data)) == NS_START_DONE);
    CHECK(pifNvram_DeleteRecord(&s_nvram, 3));
    CHECK(!pifNvram_ReadRecord(&s_nvram, 3, read, sizeof(read), NULL) && pif_error == E_CANNOT_FOUND);
    CHECK(_restart());
    CHECK(!pifNvram_ReadRecord(&s_nvram, 3, read, sizeof(read), NULL) && pif_error == E_CANNOT_FOUND);

    // Written again after that, it starts over.
    CHECK(pifNvram_WriteRecord(&s_nvram, 3, data, sizeof(data)) == NS_START_DONE);
    CHECK(pifNvram_ReadRecord(&s_nvram, 3, read, sizeof(read), NULL) && memcmp(read, data, sizeof(data)) == 0);
}

static void testRecordTimed(void)
{
    uint8_t first[20], second[20], read[20];

    CHECK(_setUp(NM_RECORD, TRUE));
    _fill(first, sizeof(first), 0x11);
    _fill(second, sizeof(second), 0x22);

    // The header goes at once as a step of its own, and the data follows a page per tick.
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, first, sizeof(first)) == NS_START_TIMED);
    CHECK(!pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && pif_error == E_INVALID_STATE);
    _tick(8);
    CHECK(s_done_count == 1 && s_done_result && s_nvram._state == NS_IDLE);
    CHECK(!s_crossed_page);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && memcmp(read, first, sizeof(first)) == 0);

    // Cut short on the way, it leaves the record as it was.
    s_fail_write = s_write_count + 3;
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, second, sizeof(second)) == NS_START_TIMED);
    _tick(8);
    CHECK(s_done_count == 2 && !s_done_result);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && memcmp(read, first, sizeof(first)) == 0);

    // Abandoned with the timer, the same.
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, second, sizeof(second)) == NS_START_TIMED);
    pifNvram_DetachTimer(&s_nvram);
    CHECK(s_nvram._state == NS_IDLE);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), NULL) && memcmp(read, first, sizeof(first)) == 0);
}

#endif

// An NVRAM laid out by sizes over media that reads as erased, or the same layout again over the media as it is.
static BOOL _setUpLayout(const uint16_t* p_size, uint16_t count, PifNvramMode mode, BOOL erase)
{
    pifNvram_Clear(&s_nvram);
    pifTimerManager_Clear(&s_manager);

    if (erase) memset(s_media, 0xFF, sizeof(s_media));
    s_write_count = s_read_count = 0;
    s_fail_write = 0;
    s_fail_torn = FALSE;
    s_crossed_page = FALSE;

    if (!pifNvram_Init(&s_nvram, PIF_ID_AUTO)) return FALSE;
    if (!pifNvram_AttachActMedia(&s_nvram, _actRead, _actWrite)) return FALSE;
    return pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, p_size, count, mode);
}

static void testLayoutRaw(void)
{
    static const uint16_t sizes[] = { 4, 20, 9 };
    static const uint16_t too_large[] = { 200, 100 };
    static const uint16_t empty[] = { 4, 0 };
    uint8_t data[21], read[21];

    // One after the other: 0, 4 and 24.
    CHECK(_setUpLayout(sizes, 3, NM_RAW, TRUE));
    CHECK(s_nvram._slot_size == 0 && s_nvram._slot_count == 3);

    _fill(data, sizeof(data), 0x31);
    CHECK(pifNvram_Write(&s_nvram, 1, data, 20) == NS_START_DONE);
    CHECK(memcmp(s_media + 4, data, 20) == 0 && s_media[3] == 0xFF && s_media[24] == 0xFF);
    CHECK(!s_crossed_page);
    CHECK(pifNvram_Write(&s_nvram, 2, data, 9) == NS_START_DONE);
    CHECK(memcmp(s_media + 24, data, 9) == 0 && s_media[33] == 0xFF);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_Read(&s_nvram, 1, read, 20) == NS_START_DONE && memcmp(read, data, 20) == 0);

    // Each slot holds its own size and no more.
    CHECK(pifNvram_Write(&s_nvram, 0, data, 5) == NS_START_FAILURE && pif_error == E_INVALID_PARAM);
    CHECK(pifNvram_Write(&s_nvram, 1, data, 21) == NS_START_FAILURE && pif_error == E_INVALID_PARAM);
    CHECK(pifNvram_Write(&s_nvram, 3, data, 1) == NS_START_FAILURE && pif_error == E_INVALID_ID);

    // More than the media holds, and a slot of nothing.
    CHECK(!pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, too_large, 2, NM_RAW) && pif_error == E_OVERFLOW_BUFFER);
    CHECK(!pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, empty, 2, NM_RAW) && pif_error == E_INVALID_PARAM);
    CHECK(!pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, sizes, 0, NM_RAW) && pif_error == E_INVALID_PARAM);
    // A refused layout leaves the one before it.
    CHECK(s_nvram._slot_count == 3 && pifNvram_Read(&s_nvram, 2, read, 9) == NS_START_DONE);
}

#ifdef PIF_USE_NVRAM_RECORD

static void testLayoutRecord(void)
{
    // Record 0 takes 0 to 20 in two copies of 10, record 1 20 to 92 in two of 36, record 2 92 to 104 in two of 6.
    static const uint16_t sizes[] = { 4, 30, 0 };
    static const uint16_t too_large[] = { 100, 30 };
    uint8_t data[31], read[31];
    uint16_t size;

    CHECK(_setUpLayout(sizes, 3, NM_RECORD, TRUE));
    CHECK(s_nvram._slot_count == 6);

    _fill(data, sizeof(data), 0x41);
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, 4) == NS_START_DONE);
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, 30) == NS_START_DONE);
    CHECK(pifNvram_WriteRecord(&s_nvram, 2, NULL, 0) == NS_START_DONE);
    CHECK(memcmp(s_media + PIF_NVRAM_RECORD_HEADER, data, 4) == 0);
    CHECK(memcmp(s_media + 20 + PIF_NVRAM_RECORD_HEADER, data, 30) == 0);
    CHECK(s_media[104] == 0xFF);

    // The second write of record 1 goes to its other copy, and record 2 next to it is untouched.
    _fill(data, sizeof(data), 0x51);
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, 30) == NS_START_DONE);
    CHECK(memcmp(s_media + 56 + PIF_NVRAM_RECORD_HEADER, data, 30) == 0);
    CHECK(s_media[20 + PIF_NVRAM_RECORD_HEADER] == 0x41);
    CHECK(pifNvram_ReadRecord(&s_nvram, 2, read, sizeof(read), &size) && size == 0);

    // Each record holds its own size and no more.
    CHECK(pifNvram_WriteRecord(&s_nvram, 0, data, 5) == NS_START_FAILURE && pif_error == E_INVALID_PARAM);
    CHECK(pifNvram_WriteRecord(&s_nvram, 2, data, 1) == NS_START_FAILURE && pif_error == E_INVALID_PARAM);

    // After a reset, the same layout finds the same records.
    CHECK(_setUpLayout(sizes, 3, NM_RECORD, FALSE));
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 1, read, sizeof(read), &size));
    CHECK(size == 30 && memcmp(read, data, 30) == 0);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 0, read, sizeof(read), &size));
    CHECK(size == 4 && read[0] == 0x41);

    // Cut short, it reads as the one before.
    _fill(data, sizeof(data), 0x61);
    s_fail_write = s_write_count + 2;
    s_fail_torn = TRUE;
    CHECK(pifNvram_WriteRecord(&s_nvram, 1, data, 30) == NS_START_FAILURE);
    memset(read, 0, sizeof(read));
    CHECK(pifNvram_ReadRecord(&s_nvram, 1, read, sizeof(read), NULL) && read[0] == 0x51);

    // Two copies with their headers have to fit.
    CHECK(!pifNvram_SetLayout(&s_nvram, VOLUME, PAGE_SIZE, too_large, 2, NM_RECORD) && pif_error == E_OVERFLOW_BUFFER);
}

#endif

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { testSetMedia, "set media" },
    { testRaw, "raw slots" },
    { testRawTimed, "raw slots carried by the timer" },
#ifdef PIF_USE_NVRAM_RECORD
    { testRecord, "records" },
    { testRecordCutShort, "record write cut short" },
    { testRecordCorrupted, "record copy gone bad" },
    { testRecordSequenceWraps, "record sequence wraps" },
    { testRecordLength, "record length" },
    { testRecordDelete, "record delete" },
    { testRecordTimed, "record carried by the timer" },
#endif
    { testLayoutRaw, "layout of raw slots" },
#ifdef PIF_USE_NVRAM_RECORD
    { testLayoutRecord, "layout of records" },
#endif
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(1, 2)) {
        printf("pif_nvram: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_nvram\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifNvram_Clear(&s_nvram);
    pifTimerManager_Clear(&s_manager);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
