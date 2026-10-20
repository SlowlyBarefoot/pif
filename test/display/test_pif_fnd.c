// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_fnd.h"

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

// Compares the display with EXP, where '.' follows the digit whose decimal point is on.
#define CHECK_SHOW(EXP) \
    do { \
        if (strcmp(_show(), EXP)) { \
            printf("  FAIL %s:%d: [%s] expected [%s]\n", __FILE__, __LINE__, _show(), EXP); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define SIM_STEP_US		100
#define MAX_DIGITS		8

static BOOL s_failed;
static uint32_t s_now;
static uint32_t s_sub_us;
#define LOG_SIZE		32

static PifFnd s_fnd;
static PifFnd s_fnd2;
static PifFnd s_fnd3;

#define TASK_SIZE		4
#define MAX_DUMMIES		TASK_SIZE

static PifTask* s_dummy[MAX_DUMMIES];
static uint8_t s_segment[MAX_DIGITS];
static uint8_t s_segment2[MAX_DIGITS];
static PifId s_log_id[LOG_SIZE];
static uint8_t s_log_digit[LOG_SIZE];
static int s_log_count;


static uint32_t _timer1us(void)
{
    return s_now;
}

// Records which display output which digit, in the order of output.
static void _log(PifId id, uint8_t digit)
{
    if (s_log_count < LOG_SIZE) {
        s_log_id[s_log_count] = id;
        s_log_digit[s_log_count] = digit;
        s_log_count++;
    }
}

static void _actDisplay(uint8_t segment, uint8_t digit)
{
    if (digit < MAX_DIGITS) s_segment[digit] = segment;
    _log(s_fnd._id, digit);
}

static void _actDisplay2(uint8_t segment, uint8_t digit)
{
    if (digit < MAX_DIGITS) s_segment2[digit] = segment;
    _log(s_fnd2._id, digit);
}

// Moves the clock on by ms, running the main loop at every step.
static void _advance(uint32_t ms)
{
    uint32_t steps = ms * 1000 / SIM_STEP_US;

    while (steps--) {
        s_now += SIM_STEP_US;
        s_sub_us += SIM_STEP_US;
        if (s_sub_us == 1000) {
            s_sub_us = 0;
            pif_cumulative_timer1ms++;
        }
        pifTaskManager_Loop();
    }
}

static const char* _show(void)
{
    static char buf[2 * MAX_DIGITS + 1];
    int i, n = 0;

    for (i = 0; i < s_fnd._digit_size; i++) {
        buf[n++] = s_fnd.__p_string[i] & 0x7F;
        if (s_fnd.__p_string[i] & 0x80) buf[n++] = '.';
    }
    buf[n] = 0;
    return buf;
}

static uint32_t _doDummy(PifTask* p_task)
{
    (void)p_task;
    return 0;
}

static void _clearDummies(void)
{
    int i;

    for (i = 0; i < MAX_DUMMIES; i++) {
        if (s_dummy[i]) pifTaskManager_Remove(s_dummy[i]);
        s_dummy[i] = NULL;
    }
}

static BOOL _setUp(uint8_t digit_size)
{
    // A failed test returns early, so free what it left behind.
    pifFnd_Clear(&s_fnd);
    pifFnd_Clear(&s_fnd2);
    pifFnd_Clear(&s_fnd3);
    _clearDummies();
    memset(s_segment, 0xFF, sizeof(s_segment));
    memset(s_segment2, 0xFF, sizeof(s_segment2));
    s_log_count = 0;
    pif_error = E_SUCCESS;

    if (!pifFnd_Init(&s_fnd, PIF_ID_AUTO, digit_size, _actDisplay)) return FALSE;
    // One digit every 1ms, so that blink is checked every 1ms.
    return pifFnd_SetPeriodPerDigit(&s_fnd, digit_size);
}

static void testInteger(void)
{
    CHECK(_setUp(4));
    pifFnd_SetInteger(&s_fnd, 0);
    CHECK_SHOW("   0");
    pifFnd_SetInteger(&s_fnd, 105);
    CHECK_SHOW(" 105");
    pifFnd_SetInteger(&s_fnd, -5);
    CHECK_SHOW("  -5");
    pifFnd_SetInteger(&s_fnd, -999);
    CHECK_SHOW("-999");

    // An overflow shows '_' on the last digit.
    pifFnd_SetInteger(&s_fnd, -1000);
    CHECK_SHOW("100_");
    pifFnd_SetInteger(&s_fnd, 12345);
    CHECK_SHOW("234_");
    pifFnd_SetInteger(&s_fnd, INT32_MIN);
    CHECK_SHOW("364_");

    pifFnd_SetFillZero(&s_fnd, TRUE);
    pifFnd_SetInteger(&s_fnd, 0);
    CHECK_SHOW("0000");
    pifFnd_SetInteger(&s_fnd, -5);
    CHECK_SHOW("-005");
    pifFnd_SetInteger(&s_fnd, -1000);
    CHECK_SHOW("-00_");

    // Sub digits are shown as zeros after the decimal point.
    pifFnd_SetFillZero(&s_fnd, FALSE);
    s_fnd.sub_numeric_digits = 1;
    pifFnd_SetInteger(&s_fnd, 12);
    CHECK_SHOW(" 12.0");

    // Sub digits are kept below the digit size, leaving one integer digit.
    s_fnd.sub_numeric_digits = 9;
    pifFnd_SetInteger(&s_fnd, 7);
    CHECK_SHOW("7.000");
}

static void testFloat(void)
{
    CHECK(_setUp(4));
    s_fnd.sub_numeric_digits = 1;
    pifFnd_SetFloat(&s_fnd, 2.3);
    CHECK_SHOW("  2.3");
    pifFnd_SetFloat(&s_fnd, 0.5);
    CHECK_SHOW("  0.5");
    pifFnd_SetFloat(&s_fnd, -0.5);
    CHECK_SHOW(" -0.5");

    // Rounding may carry into the integer part, and a value rounded to 0 has no sign.
    pifFnd_SetFloat(&s_fnd, 9.96);
    CHECK_SHOW(" 10.0");
    pifFnd_SetFloat(&s_fnd, -0.04);
    CHECK_SHOW("  0.0");

    pifFnd_SetFloat(&s_fnd, 123.4);
    CHECK_SHOW("123.4");
    pifFnd_SetFloat(&s_fnd, -123.4);
    CHECK_SHOW("123._");
    pifFnd_SetFloat(&s_fnd, 1e12);
    CHECK_SHOW("295._");

    s_fnd.sub_numeric_digits = 2;
    pifFnd_SetFloat(&s_fnd, 1.05);
    CHECK_SHOW(" 1.05");
    pifFnd_SetFloat(&s_fnd, -1.7);
    CHECK_SHOW("-1.70");

    s_fnd.sub_numeric_digits = 3;
    pifFnd_SetFloat(&s_fnd, 0.123);
    CHECK_SHOW("0.123");
    pifFnd_SetFloat(&s_fnd, -0.123);
    CHECK_SHOW("0.12_");

    pifFnd_SetFillZero(&s_fnd, TRUE);
    s_fnd.sub_numeric_digits = 1;
    pifFnd_SetFloat(&s_fnd, -2.5);
    CHECK_SHOW("-02.5");
}

static void testSegment(void)
{
    CHECK(_setUp(4));
    s_fnd.sub_numeric_digits = 1;
    pifFnd_SetFloat(&s_fnd, -1.5);
    pifFnd_Start(&s_fnd);
    _advance(8);
    CHECK(s_segment[0] == 0x00 && s_segment[1] == 0x40 && s_segment[2] == (0x06 | 0x80) && s_segment[3] == 0x6D);

    pifFnd_SetInteger(&s_fnd, 12345);
    _advance(8);
    CHECK(s_segment[3] == 0x08);
}

static void testRawSegment(void)
{
    static const uint8_t raw[4] = { 0x01, 0x20, 0x80, 0x00 };

    CHECK(_setUp(4));
    pifFnd_Start(&s_fnd);

    // Bytes that are not characters of the font go out as they are.
    pifFnd_SetSegments(&s_fnd, raw);
    _advance(8);
    CHECK(s_segment[0] == 0x01 && s_segment[1] == 0x20 && s_segment[2] == 0x80 && s_segment[3] == 0x00);

    // Blink applies to them as to text.
    CHECK(pifFnd_BlinkOn(&s_fnd, 100));
    _advance(102);
    CHECK(!s_fnd.__bt.led);
    _advance(8);
    CHECK(s_segment[0] == 0 && s_segment[1] == 0 && s_segment[2] == 0);
    pifFnd_BlinkOff(&s_fnd);

    // Text and numbers go back through the font.
    pifFnd_SetString(&s_fnd, "1-");
    _advance(8);
    CHECK(s_segment[0] == 0x06 && s_segment[1] == 0x40 && s_segment[2] == 0x00);
    pifFnd_SetSegments(&s_fnd, raw);
    pifFnd_SetInteger(&s_fnd, 7);
    _advance(8);
    CHECK(s_segment[0] == 0x00 && s_segment[3] == 0x07);
}

static void testBlink(void)
{
    CHECK(_setUp(4));
    CHECK(!pifFnd_ChangeBlinkPeriod(&s_fnd, 100) && pif_error == E_INVALID_STATE);
    CHECK(!pifFnd_BlinkOn(&s_fnd, 0) && pif_error == E_INVALID_PARAM);
    pifFnd_SetInteger(&s_fnd, 8888);
    pifFnd_Start(&s_fnd);

    CHECK(pifFnd_BlinkOn(&s_fnd, 100));
    _advance(99);
    CHECK(s_fnd.__bt.led);
    _advance(3);
    CHECK(!s_fnd.__bt.led);
    _advance(8);
    CHECK(s_segment[0] == 0 && s_segment[1] == 0 && s_segment[2] == 0 && s_segment[3] == 0);
    _advance(92);
    CHECK(s_fnd.__bt.led);

    // A changed period applies from the next toggle.
    CHECK(!pifFnd_ChangeBlinkPeriod(&s_fnd, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifFnd_ChangeBlinkPeriod(&s_fnd, 50));
    _advance(45);
    CHECK(s_fnd.__bt.led);
    _advance(10);
    CHECK(!s_fnd.__bt.led);

    // Stop in the off phase does not leave a start after it dark.
    pifFnd_Stop(&s_fnd);
    CHECK(s_segment[0] == 0 && s_segment[3] == 0);
    CHECK(s_fnd.__bt.led && !s_fnd.__bt.blink);
    pifFnd_Start(&s_fnd);
    _advance(8);
    CHECK(s_segment[0] == 0x7F && s_segment[3] == 0x7F);
    CHECK(!pifFnd_ChangeBlinkPeriod(&s_fnd, 100) && pif_error == E_INVALID_STATE);

    // Blink advances only while the display runs, and BlinkOff turns it back on.
    pifFnd_Stop(&s_fnd);
    CHECK(pifFnd_BlinkOn(&s_fnd, 10));
    _advance(100);
    CHECK(s_fnd.__bt.led);

    // A long pause gives one toggle, not a burst that catches up.
    pifFnd_Start(&s_fnd);
    _advance(5);
    CHECK(!s_fnd.__bt.led);
    pifFnd_BlinkOff(&s_fnd);
    CHECK(s_fnd.__bt.led);
    _advance(100);
    CHECK(s_fnd.__bt.led);
}

static void testShareScan(void)
{
    int i, start;

    CHECK(_setUp(4));
    CHECK(pifFnd_Init(&s_fnd2, PIF_ID_AUTO, 2, _actDisplay2));
    CHECK(!pifFnd_ShareScan(&s_fnd, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(!pifFnd_ShareScan(&s_fnd, &s_fnd) && pif_error == E_INVALID_STATE);
    CHECK(pifFnd_ShareScan(&s_fnd2, &s_fnd));
    CHECK(!s_fnd2.__p_task && pifTaskManager_Count() == 1);

    // A display can join one group only, and a member stands for its leader.
    CHECK(!pifFnd_ShareScan(&s_fnd2, &s_fnd) && pif_error == E_INVALID_STATE);
    CHECK(!pifFnd_ShareScan(&s_fnd, &s_fnd2) && pif_error == E_INVALID_STATE);

    // The frame period is that of the group, set through any of its displays.
    CHECK(pifFnd_GetPeriodPerDigit(&s_fnd2) == 4);
    CHECK(pifFnd_SetPeriodPerDigit(&s_fnd2, 6));		// One digit every 1ms
    CHECK(pifFnd_GetPeriodPerDigit(&s_fnd) == 6);

    // With only the member started, the task of the leader runs and keeps the leader dark.
    pifFnd_SetInteger(&s_fnd, 1234);
    pifFnd_SetInteger(&s_fnd2, 56);
    pifFnd_Start(&s_fnd2);
    CHECK(!s_fnd.__p_task->_pause);
    _advance(12);
    CHECK(s_segment2[0] == 0x6D && s_segment2[1] == 0x7D);
    CHECK(s_segment[0] == 0 && s_segment[3] == 0);

    // One task outputs the digits of both displays in turn, each through its own callback.
    pifFnd_Start(&s_fnd);
    s_log_count = 0;
    _advance(12);
    CHECK(s_log_count >= 12);
    for (start = 0; start < 6; start++) {
        if (s_log_id[start] == s_fnd._id && s_log_digit[start] == 0) break;
    }
    CHECK(start < 6);
    for (i = 0; i < 6; i++) {
        CHECK(s_log_id[start + i] == (i < 4 ? s_fnd._id : s_fnd2._id));
        CHECK(s_log_digit[start + i] == (i < 4 ? i : i - 4));
    }
    CHECK(s_segment[0] == 0x06 && s_segment[3] == 0x66);

    // Each display blinks by itself.
    CHECK(pifFnd_BlinkOn(&s_fnd2, 100));
    _advance(110);
    CHECK(!s_fnd2.__bt.led && s_fnd.__bt.led);
    pifFnd_BlinkOff(&s_fnd2);

    // Stopping the leader leaves the member running.
    pifFnd_Stop(&s_fnd);
    memset(s_segment2, 0xFF, sizeof(s_segment2));
    _advance(12);
    CHECK(s_segment[0] == 0 && s_segment[3] == 0);
    CHECK(s_segment2[0] == 0x6D && s_segment2[1] == 0x7D);
    pifFnd_Stop(&s_fnd2);
    CHECK(s_fnd.__p_task->_pause);

    // A cleared member leaves the group.
    pifFnd_Start(&s_fnd);
    pifFnd_Clear(&s_fnd2);
    CHECK(!s_fnd.__p_next);
    s_log_count = 0;
    _advance(12);
    for (i = 0; i < s_log_count; i++) CHECK(s_log_id[i] == s_fnd._id);

    // When the leader is cleared, a member goes back to scanning itself.
    CHECK(pifFnd_Init(&s_fnd2, PIF_ID_AUTO, 2, _actDisplay2));
    CHECK(pifFnd_ShareScan(&s_fnd2, &s_fnd));
    pifFnd_SetInteger(&s_fnd2, 56);
    pifFnd_Start(&s_fnd2);
    pifFnd_Clear(&s_fnd);
    CHECK(!s_fnd2.__p_leader && s_fnd2.__p_task && !s_fnd2.__p_task->_pause);
    CHECK(pifTaskManager_Count() == 1);
    memset(s_segment2, 0xFF, sizeof(s_segment2));
    _advance(2 * PIF_FND_PERIOD_PER_DIGIT);
    CHECK(s_segment2[0] == 0x6D && s_segment2[1] == 0x7D);
}

static void testShareScanNoSlot(void)
{
    int i;

    CHECK(_setUp(4));
    CHECK(pifFnd_Init(&s_fnd2, PIF_ID_AUTO, 2, _actDisplay2));
    CHECK(pifFnd_Init(&s_fnd3, PIF_ID_AUTO, 2, _actDisplay2));
    CHECK(pifFnd_ShareScan(&s_fnd2, &s_fnd));
    CHECK(pifFnd_ShareScan(&s_fnd3, &s_fnd2));		// Joins the group of s_fnd
    CHECK(s_fnd3.__p_leader == &s_fnd && pifTaskManager_Count() == 1);

    // Fill every free slot, so that clearing the leader frees room for one member only.
    for (i = 0; pifTaskManager_Count() < TASK_SIZE; i++) {
        s_dummy[i] = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000000, _doDummy, NULL, FALSE);
        CHECK(s_dummy[i]);
    }
    pifFnd_Clear(&s_fnd);
    CHECK(s_fnd2.__p_task && !s_fnd2.__p_leader);
    CHECK(!s_fnd3.__p_task && !s_fnd3.__p_leader);

    // A member without a task is harmless to use and can join a group again.
    pifFnd_Start(&s_fnd3);
    pifFnd_Stop(&s_fnd3);
    CHECK(pifFnd_SetPeriodPerDigit(&s_fnd3, 10));
    CHECK(!pifFnd_ShareScan(&s_fnd2, &s_fnd3) && pif_error == E_INVALID_STATE);
    CHECK(pifFnd_ShareScan(&s_fnd3, &s_fnd2));
    CHECK(s_fnd3.__p_leader == &s_fnd2 && pifTaskManager_Count() == TASK_SIZE);
    _clearDummies();
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "integer", testInteger },
        { "float", testFloat },
        { "segment", testSegment },
        { "raw segment", testRawSegment },
        { "blink", testBlink },
        { "share scan", testShareScan },
        { "share scan without a slot", testShareScanNoSlot },
    };

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(TASK_SIZE, 1)) {
        printf("pif_fnd: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_fnd\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifFnd_Clear(&s_fnd);
    pifFnd_Clear(&s_fnd2);
    pifFnd_Clear(&s_fnd3);
    _clearDummies();
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
