// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_dot_matrix.h"

#include <stdio.h>
#include <string.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define MAX_ROWS		4
#define MAX_COL_BYTES	4

static BOOL s_failed;
static uint32_t s_now;
static PifTimerManager s_manager;
static PifDotMatrix s_dm;
static uint8_t s_row[MAX_ROWS][MAX_COL_BYTES];
static int s_display_count;
static int s_finish_count;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _actDisplay(uint8_t row, uint8_t* p_data)
{
    if (row < MAX_ROWS) memcpy(s_row[row], p_data, s_dm.__col_bytes);
    s_display_count++;
}

static void _evtShiftFinish(PifId id)
{
    (void)id;
    s_finish_count++;
}

// One tick of the timer interrupt, then one pass of the main loop.
static void _tick(int count)
{
    while (count--) {
        pifTimerManager_sigTick(&s_manager);
        pifTaskManager_Loop();
    }
}

// Runs the main loop over two frames, so that the scan task outputs every row whatever its phase.
static void _scanFrame(void)
{
    uint32_t end = s_now + 2 * s_dm.__frame_period_1ms * 1000UL;

    while (s_now < end) {
        s_now += 100;
        pifTaskManager_Loop();
    }
}

static BOOL _setUp(uint16_t col_size, uint16_t row_size)
{
    // A failed test returns early, so free what it left behind.
    pifDotMatrix_Clear(&s_dm);
    pifTimerManager_Clear(&s_manager);
    memset(s_row, 0xFF, sizeof(s_row));
    s_display_count = 0;
    s_finish_count = 0;
    pif_error = E_SUCCESS;

    if (!pifTimerManager_Init(&s_manager, PIF_ID_AUTO, 1000, 4)) return FALSE;
    if (!pifDotMatrix_Init(&s_dm, PIF_ID_AUTO, &s_manager, col_size, row_size, _actDisplay)) return FALSE;
    s_dm.evt_shift_finish = _evtShiftFinish;
    return TRUE;
}

static void testNoPattern(void)
{
    CHECK(_setUp(8, 1));
    pifDotMatrix_Start(&s_dm);
    CHECK(!pifDotMatrix_SetPosition(&s_dm, 0, 0) && pif_error == E_INVALID_STATE);
    CHECK(!pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_ONCE, 1, 0) && pif_error == E_INVALID_STATE);
    CHECK(!pifDotMatrix_SelectPattern(&s_dm, 0));
    CHECK(!pifDotMatrix_SetPatternSize(&s_dm, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDotMatrix_ChangeBlinkPeriod(&s_dm, 100) && pif_error == E_INVALID_STATE);
    CHECK(!pifDotMatrix_ChangeShiftPeriod(&s_dm, 100) && pif_error == E_INVALID_STATE);
}

static void testShiftedRender(void)
{
    // A 12 column pattern followed by a byte that must not be read.
    static uint8_t pattern[] = { 0xAA, 0x0B, 0xFF };
    static uint8_t same[] = { 0x55, 0x01 };

    CHECK(_setUp(10, 1));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 2));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 12, 1, pattern));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 10, 1, same));

    // 2 is the last position, 12 - 10.
    CHECK(pifDotMatrix_SetPosition(&s_dm, 2, 0));
    CHECK(s_dm.__p_paper[0] == 0xEA && s_dm.__p_paper[1] == 0x02);
    CHECK(!pifDotMatrix_SetPosition(&s_dm, 3, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDotMatrix_SetPosition(&s_dm, 0, 1) && pif_error == E_INVALID_PARAM);

    // A pattern of the display size has only position (0, 0), which SelectPattern() moves to.
    CHECK(pifDotMatrix_SelectPattern(&s_dm, 1));
    CHECK(s_dm.__position_x == 0 && s_dm.__p_paper[0] == 0x55 && s_dm.__p_paper[1] == 0x01);
    CHECK(pifDotMatrix_SetPosition(&s_dm, 0, 0));
}

static void testWidePattern(void)
{
    static uint8_t pattern[38];

    memset(pattern, 0, sizeof(pattern));
    pattern[37] = 0x0F;		// Columns 296 to 299
    CHECK(_setUp(8, 1));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 300, 1, pattern));
    CHECK(pifDotMatrix_SetPosition(&s_dm, 292, 0));
    CHECK(s_dm.__p_paper[0] == 0xF0);
}

static void testBlankRows(void)
{
    static uint8_t pattern[] = { 0x12, 0x34, 0x56, 0x78 };

    CHECK(_setUp(16, 2));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 16, 2, pattern));
    pifDotMatrix_Start(&s_dm);
    _scanFrame();
    CHECK(s_row[0][0] == 0x12 && s_row[0][1] == 0x34 && s_row[1][0] == 0x56 && s_row[1][1] == 0x78);

    // The off phase of a blink outputs every byte of a row as 0.
    CHECK(pifDotMatrix_BlinkOn(&s_dm, 1));
    _tick(1);
    CHECK(!s_dm.__bt.led);
    _scanFrame();
    CHECK(s_row[0][0] == 0 && s_row[0][1] == 0 && s_row[1][0] == 0 && s_row[1][1] == 0);

    // Stop outputs each row once, and a start after it is not left in the off phase.
    memset(s_row, 0xFF, sizeof(s_row));
    s_display_count = 0;
    pifDotMatrix_Stop(&s_dm);
    CHECK(s_display_count == 2);
    CHECK(s_row[0][0] == 0 && s_row[0][1] == 0 && s_row[1][0] == 0 && s_row[1][1] == 0);
    pifDotMatrix_Start(&s_dm);
    _scanFrame();
    CHECK(s_row[0][0] == 0x12 && s_row[1][1] == 0x78);
}

static void testShiftValidation(void)
{
    static uint8_t pattern[] = { 0x01, 0x00 };

    CHECK(_setUp(8, 1));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 16, 1, pattern));
    CHECK(!pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_REPEAT_VER, 1, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDotMatrix_ShiftOn(&s_dm, DMSD_UP, DMSM_PING_PONG_HOR, 1, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDotMatrix_ShiftOn(&s_dm, DMSD_NONE, DMSM_ONCE, 1, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_ONCE, 0, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_ONCE, 1, 0));
    CHECK(!pifDotMatrix_ChangeShiftPeriod(&s_dm, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifDotMatrix_ChangeShiftPeriod(&s_dm, 2));
}

static void testPingPong(void)
{
    static uint8_t pattern[] = { 0x00, 0x00 };
    static const uint16_t expected[] = { 1, 2, 1, 0, 1, 2, 1 };
    size_t i;

    CHECK(_setUp(8, 1));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 10, 1, pattern));
    CHECK(pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_PING_PONG_HOR, 1, 0));

    // Each step moves, the turning ones too.
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        _tick(1);
        CHECK(s_dm.__position_x == expected[i]);
    }
    CHECK(s_finish_count == 0);
}

static void testShiftEnd(void)
{
    static uint8_t pattern[] = { 0x00, 0x00 };

    CHECK(_setUp(8, 1));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    CHECK(pifDotMatrix_AddPattern(&s_dm, 16, 1, pattern));

    // The end of the count reports as the end of a once shift does.
    CHECK(pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_REPEAT_HOR, 1, 3));
    _tick(5);
    CHECK(s_dm.__position_x == 3 && s_finish_count == 1);

    CHECK(pifDotMatrix_ShiftOn(&s_dm, DMSD_RIGHT, DMSM_ONCE, 1, 0));
    _tick(10);
    CHECK(s_dm.__position_x == 0 && s_finish_count == 2);

    // Released patterns stop the shift.
    CHECK(pifDotMatrix_ShiftOn(&s_dm, DMSD_LEFT, DMSM_REPEAT_HOR, 1, 0));
    CHECK(pifDotMatrix_SetPatternSize(&s_dm, 1));
    _tick(3);
    CHECK(s_dm.__shift_direction == DMSD_NONE && s_dm.__position_x == 0 && s_finish_count == 2);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "no pattern", testNoPattern },
        { "shifted render", testShiftedRender },
        { "wide pattern", testWidePattern },
        { "blank rows", testBlankRows },
        { "shift validation", testShiftValidation },
        { "ping pong", testPingPong },
        { "shift end", testShiftEnd },
    };

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(2, 1)) {
        printf("pif_dot_matrix: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_dot_matrix\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifDotMatrix_Clear(&s_dm);
    pifTimerManager_Clear(&s_manager);
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
