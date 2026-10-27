// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_led.h"

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

// Compares the last output with EXP.
#define CHECK_STATE(EXP) \
    do { \
        if (s_state != (uint32_t)(EXP)) { \
            printf("  FAIL %s:%d: state 0x%lX expected 0x%lX\n", __FILE__, __LINE__, \
                   (unsigned long)s_state, (unsigned long)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define SIM_STEP_US		100
#define PERIOD_MS		10
// Half a period, the middle of a blink phase, so that a check never meets a toggle.
#define HALF_MS			(PERIOD_MS / 2)

#define TASK_SIZE		4

static BOOL s_failed;
static uint32_t s_now;

static PifLed s_led;
static PifTask* s_dummy[TASK_SIZE];
static uint32_t s_state;
static int s_act_count;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _actState(PifId id, uint32_t state)
{
    (void)id;
    s_state = state;
    s_act_count++;
}

// Moves the clock on by ms, running the main loop at every step.
static void _advance(uint32_t ms)
{
    uint32_t steps = ms * 1000 / SIM_STEP_US;

    while (steps--) {
        s_now += SIM_STEP_US;
        pifTaskManager_Loop();
    }
}

static uint32_t _doDummy(PifTask* p_task)
{
    (void)p_task;
    return 0;
}

static void _clearDummies(void)
{
    int i;

    for (i = 0; i < TASK_SIZE; i++) {
        if (s_dummy[i]) pifTaskManager_Remove(s_dummy[i]);
        s_dummy[i] = NULL;
    }
}

static BOOL _setUp(uint8_t count)
{
    // A failed test returns early, so free what it left behind.
    pifLed_Clear(&s_led);
    _clearDummies();
    s_state = 0;
    s_act_count = 0;
    pif_error = E_SUCCESS;

    return pifLed_Init(&s_led, PIF_ID_AUTO, count, _actState);
}

static void testInit(void)
{
    CHECK(_setUp(1));
    CHECK(!pifLed_Init(NULL, PIF_ID_AUTO, 1, _actState));
    CHECK(!pifLed_Init(&s_led, PIF_ID_AUTO, 0, _actState));
    CHECK(!pifLed_Init(&s_led, PIF_ID_AUTO, 33, _actState));
    CHECK(!pifLed_Init(&s_led, PIF_ID_AUTO, 1, NULL));
    CHECK(pif_error == E_INVALID_PARAM);

    CHECK(_setUp(32));
    pifLed_AllOn(&s_led);
    CHECK_STATE(0xFFFFFFFFUL);

    CHECK(_setUp(3));
    pifLed_AllToggle(&s_led);
    CHECK_STATE(0x7);
    pifLed_PartOff(&s_led, 0x2);
    CHECK_STATE(0x5);
    pifLed_PartChange(&s_led, 0x2, ON);
    CHECK_STATE(0x7);
    pifLed_PartToggle(&s_led, 0x1);
    CHECK_STATE(0x6);
    pifLed_AllOff(&s_led);
    CHECK_STATE(0x0);
}

static void testSBlink(void)
{
    int tasks;
    int count;

    CHECK(_setUp(2));
    tasks = pifTaskManager_Count();
    CHECK(pifLed_AttachSBlink(&s_led, PERIOD_MS));
    CHECK(pifTaskManager_Count() == tasks + 1);

    // Nothing blinks yet, so the task does not run.
    _advance(5 * PERIOD_MS);
    CHECK(s_act_count == 0);

    pifLed_PartOn(&s_led, 0x2);
    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(HALF_MS);
    CHECK_STATE(0x3);
    _advance(PERIOD_MS);
    CHECK_STATE(0x2);
    _advance(PERIOD_MS);
    CHECK_STATE(0x3);

    // Once the last LED stops, the task pauses again.
    pifLed_SBlinkOff(&s_led, 0x1, OFF);
    CHECK_STATE(0x2);
    count = s_act_count;
    _advance(5 * PERIOD_MS);
    CHECK(s_act_count == count);

    pifLed_DetachBlink(&s_led);
    CHECK(pifTaskManager_Count() == tasks);
}

static void testResumeStartsOn(void)
{
    CHECK(_setUp(1));
    CHECK(pifLed_AttachSBlink(&s_led, PERIOD_MS));

    // Stopped in the on phase, but with the LED turned off.
    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(HALF_MS);
    CHECK_STATE(0x1);
    pifLed_SBlinkOff(&s_led, 0x1, OFF);
    CHECK_STATE(0x0);

    // Blinking again turns the LED on first instead of carrying on from the old phase.
    _advance(5 * PERIOD_MS);
    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(HALF_MS);
    CHECK_STATE(0x1);
    _advance(PERIOD_MS);
    CHECK_STATE(0x0);
}

static void testMBlink(void)
{
    CHECK(_setUp(3));
    CHECK(pifLed_AttachMBlink(&s_led, PERIOD_MS, 2, 1, 3));

    // Profile 0 toggles every period, profile 1 every 3 periods.
    pifLed_MBlinkOn(&s_led, 0x1, 0);
    pifLed_MBlinkOn(&s_led, 0x2, 1);
    _advance(HALF_MS);
    CHECK_STATE(0x3);
    _advance(PERIOD_MS);
    CHECK_STATE(0x2);
    _advance(PERIOD_MS);
    CHECK_STATE(0x3);
    _advance(PERIOD_MS);
    CHECK_STATE(0x0);
    _advance(PERIOD_MS);
    CHECK_STATE(0x1);

    // An LED moved to another profile leaves the old one.
    pifLed_MBlinkOn(&s_led, 0x1, 1);
    CHECK(s_led.__p_blinks[0].flag == 0);
    CHECK(s_led.__p_blinks[1].flag == 0x3);

    // An index out of range is ignored.
    pifLed_MBlinkOn(&s_led, 0x4, 2);
    CHECK(s_led.__p_blinks[0].flag == 0 && s_led.__p_blinks[1].flag == 0x3);
}

static void testSingleProfileMultiple(void)
{
    CHECK(_setUp(1));
    CHECK(pifLed_AttachMBlink(&s_led, PERIOD_MS, 1, 3));

    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(HALF_MS);
    CHECK_STATE(0x1);
    _advance(2 * PERIOD_MS);
    CHECK_STATE(0x1);
    _advance(PERIOD_MS);
    CHECK_STATE(0x0);
}

static void testAttachFail(void)
{
    int tasks;

    CHECK(_setUp(1));
    tasks = pifTaskManager_Count();
    CHECK(!pifLed_AttachSBlink(&s_led, 0));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(!pifLed_AttachMBlink(&s_led, PERIOD_MS, 0));
    CHECK(pifTaskManager_Count() == tasks);

    CHECK(pifLed_AttachSBlink(&s_led, PERIOD_MS));
    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(HALF_MS);
    CHECK_STATE(0x1);

    // A zero multiple fails and leaves the running blink as it was.
    pif_error = E_SUCCESS;
    CHECK(!pifLed_AttachMBlink(&s_led, PERIOD_MS, 2, 2, 0));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(s_led.__blink_count == 1 && s_led.__p_blinks[0].flag == 0x1);
    _advance(PERIOD_MS);
    CHECK_STATE(0x0);
    _advance(PERIOD_MS);
    CHECK_STATE(0x1);

    // Attaching again keeps the task but clears which LEDs blink.
    CHECK(pifLed_AttachMBlink(&s_led, PERIOD_MS, 2, 1, 2));
    CHECK(pifTaskManager_Count() == tasks + 1);
    CHECK(s_led._p_task->_pause);
}

static void testNoSlot(void)
{
    int i;

    CHECK(_setUp(1));
    for (i = 0; i < TASK_SIZE; i++) {
        s_dummy[i] = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000, _doDummy, NULL, FALSE);
        if (!s_dummy[i]) break;
    }
    CHECK(i > 0);

    CHECK(!pifLed_AttachSBlink(&s_led, PERIOD_MS));
    CHECK(!s_led._p_task && !s_led.__p_blinks && !s_led.__blink_count);

    // Without blink the calls that need it do nothing.
    pifLed_SBlinkOn(&s_led, 0x1);
    CHECK(s_act_count == 0);
    _clearDummies();
}

static void testMask(void)
{
    CHECK(_setUp(3));
    pifLed_PartOn(&s_led, 0xFF);
    CHECK_STATE(0x7);
    pifLed_PartToggle(&s_led, 0xF8);
    CHECK_STATE(0x7);
    pifLed_AllChange(&s_led, 0xFA);
    CHECK_STATE(0x2);
    pifLed_PartChange(&s_led, 0x8, ON);
    CHECK_STATE(0x2);

    // An LED past count does not blink and does not wake the task.
    CHECK(pifLed_AttachSBlink(&s_led, PERIOD_MS));
    pifLed_SBlinkOn(&s_led, 0x8);
    CHECK(s_led.__p_blinks[0].flag == 0);
    CHECK(s_led._p_task->_pause);
}

static void testMBlinkOffOther(void)
{
    CHECK(_setUp(2));
    CHECK(pifLed_AttachMBlink(&s_led, PERIOD_MS, 2, 1, 3));
    pifLed_MBlinkOn(&s_led, 0x1, 0);
    pifLed_MBlinkOn(&s_led, 0x2, 1);
    CHECK_STATE(0x3);

    // LED 1 blinks in profile 1, so stopping profile 0 for it leaves it alone.
    pifLed_MBlinkOff(&s_led, 0x3, 0, OFF);
    CHECK_STATE(0x2);
    CHECK(s_led.__p_blinks[1].flag == 0x2);
    _advance(2 * PERIOD_MS + HALF_MS);
    CHECK_STATE(0x2);
    _advance(PERIOD_MS);
    CHECK_STATE(0x0);
}

static void testChangePeriod(void)
{
    CHECK(_setUp(1));
    CHECK(!pifLed_ChangeBlinkPeriod(&s_led, PERIOD_MS));
    CHECK(pif_error == E_INVALID_STATE);

    CHECK(pifLed_AttachSBlink(&s_led, PERIOD_MS));
    CHECK(!pifLed_ChangeBlinkPeriod(&s_led, 0));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(pifLed_ChangeBlinkPeriod(&s_led, 2 * PERIOD_MS));

    pifLed_SBlinkOn(&s_led, 0x1);
    _advance(PERIOD_MS);
    CHECK_STATE(0x1);
    _advance(2 * PERIOD_MS);
    CHECK_STATE(0x0);
    _advance(2 * PERIOD_MS);
    CHECK_STATE(0x1);
}

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(TASK_SIZE, 1);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "init", testInit },
        { "single blink", testSBlink },
        { "resume starts on", testResumeStartsOn },
        { "multi blink", testMBlink },
        { "single profile multiple", testSingleProfileMultiple },
        { "attach fail", testAttachFail },
        { "no task slot", testNoSlot },
        { "change period", testChangePeriod },
        { "mask past count", testMask },
        { "multi blink off in another profile", testMBlinkOffOther },
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_led: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_led\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifLed_Clear(&s_led);
    _clearDummies();
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
