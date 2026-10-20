// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_task_manager.h"

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

#define RT_PERIOD_US	1000
#define RT_RUN_US		10
#define WORK_RUN_US		300

static BOOL s_failed;
static uint32_t s_now;
static int s_rt_runs;
static int s_work_runs;
static int s_other_runs;
static uint32_t s_work_start;
static uint32_t s_work_return;

static PifTask *s_p_rt;
static PifTask *s_p_work;
static PifTask *s_p_other;


static uint32_t _timer1us(void)
{
    return s_now;
}

// Each run moves the clock on by its length, so that the scheduler measures what it would on a
// target.
#ifdef PIF_USE_TASK_REALTIME
static uint32_t _doRealtime(PifTask *p_task)
{
    (void)p_task;
    s_rt_runs++;
    s_now += RT_RUN_US;
    return 0;
}
#endif

static uint32_t _doWork(PifTask *p_task)
{
    (void)p_task;
    s_work_runs++;
    s_work_start = s_now;
    s_now += WORK_RUN_US;
    return s_work_return;
}

static uint32_t _doOther(PifTask *p_task)
{
    (void)p_task;
    s_other_runs++;
    return 0;
}

// A realtime task released every RT_PERIOD_US from 0, and a TM_EXTERNAL worker whose run of
// WORK_RUN_US is declared up front, so that the very first trigger is already judged by it.
static void _setUp(BOOL realtime)
{
    pifTaskManager_Clear();
    s_now = 0;
    s_rt_runs = 0;
    s_work_runs = 0;
    s_other_runs = 0;
    s_work_start = 0;
    s_work_return = 0;
    pif_error = E_SUCCESS;

    if (!pifTaskManager_Init(4, 0)) {
        s_failed = TRUE;
        return;
    }
#ifdef PIF_USE_TASK_REALTIME
    s_p_rt = realtime ? pifTaskManager_Add(PIF_ID_AUTO, TM_REALTIME, RT_PERIOD_US, _doRealtime, NULL, TRUE) : NULL;
#else
    s_p_rt = NULL;
#endif
    s_p_work = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doWork, NULL, FALSE);
    s_p_other = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doOther, NULL, FALSE);
    if ((realtime && !s_p_rt) || !s_p_work || !s_p_other) s_failed = TRUE;
}

static void _loop(int count)
{
    while (count--) pifTaskManager_Loop();
}

#ifdef PIF_USE_TASK_REALTIME

static void testTriggerWaitsForSlack(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // 100us before the realtime release: a 300us run does not fit.
    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(1);
    CHECK(s_work_runs == 0 && s_rt_runs == 0);

    // The release comes first, and the worker runs in the period that follows.
    s_now = RT_PERIOD_US;
    _loop(1);
    CHECK(s_rt_runs == 1 && s_work_runs == 0);
    _loop(1);
    CHECK(s_work_runs == 1 && s_work_start == RT_PERIOD_US + RT_RUN_US);

    // Consumed: nothing runs again.
    _loop(3);
    CHECK(s_work_runs == 1);
}

static void testTriggerDelay(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // Due at 500us, with the whole 500us before the release still free.
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 500));
    s_now = 499;
    _loop(2);
    CHECK(s_work_runs == 0);
    s_now = 500;
    _loop(2);
    CHECK(s_work_runs == 1 && s_work_start == 500);
}

static void testReturnedDelayWaitsForSlack(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // The first run asks to run again 300us after it ends, at 900us: 100us before the release.
    s_now = 300;
    s_work_return = 300;
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(1);
    CHECK(s_work_runs == 1 && s_now == 600);
    s_work_return = 0;
    s_now = RT_PERIOD_US - 100;
    _loop(2);
    CHECK(s_work_runs == 1);
    s_now = RT_PERIOD_US;
    _loop(3);
    CHECK(s_rt_runs == 1 && s_work_runs == 2 && s_work_start == RT_PERIOD_US + RT_RUN_US);
}

static void testTriggerBounded(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // Refused once, then let through on the next visit.
    s_p_work->max_skip = 2;
    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(1);
    CHECK(s_work_runs == 0);
    _loop(1);
    CHECK(s_work_runs == 1 && s_rt_runs == 0);
}

static void testMaxSkipOne(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // 1 means never held back.
    s_p_work->max_skip = 1;
    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(1);
    CHECK(s_work_runs == 1 && s_work_start == RT_PERIOD_US - 100 && s_rt_runs == 0);
}

static void testCutinAheadOfRealtime(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // The realtime release is due, and the cut in still goes first.
    s_now = RT_PERIOD_US;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetCutinTrigger(s_p_work));
    _loop(1);
    CHECK(s_work_runs == 1 && s_work_start == RT_PERIOD_US && s_rt_runs == 0);
}

static void testCutinTakenWaitsForSlack(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    // The slot is taken, so the worker gets a plain trigger, which waits for the slack.
    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetCutinTrigger(s_p_other));
    CHECK(pifTask_SetCutinTrigger(s_p_work));
    _loop(1);
    CHECK(s_other_runs == 1 && s_work_runs == 0);
    _loop(2);
    CHECK(s_work_runs == 0 && s_rt_runs == 0);

    s_now = RT_PERIOD_US;
    _loop(1);
    CHECK(s_rt_runs == 1 && s_work_runs == 0);
    _loop(1);
    CHECK(s_work_runs == 1 && s_work_start == RT_PERIOD_US + RT_RUN_US);
}

#endif

static void testTriggerWithoutRealtime(void)
{
    _setUp(FALSE);
    CHECK(!s_failed);

    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(2);
    CHECK(s_work_runs == 1);
}

#ifdef PIF_USE_TASK_REALTIME

static void testTriggerOnRealtime(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    s_now = 10;
    CHECK(pifTask_SetTrigger(s_p_rt, 0));
    _loop(1);
    CHECK(s_rt_runs == 1);
}

#else

static void testRealtimeLeftOut(void)
{
    _setUp(FALSE);
    CHECK(!s_failed);

    // The mode is refused, and with no release to keep free a long run starts as soon as it is due.
    CHECK(!pifTaskManager_Add(PIF_ID_AUTO, TM_REALTIME, RT_PERIOD_US, _doOther, NULL, TRUE));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(!pifTask_ChangeMode(s_p_other, TM_REALTIME, RT_PERIOD_US) && pif_error == E_INVALID_PARAM);
    s_now = RT_PERIOD_US - 100;
    pifTask_SetNextBlockTime(s_p_work, 10 * WORK_RUN_US);
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(1);
    CHECK(s_work_runs == 1 && s_work_start == RT_PERIOD_US - 100);
}

#endif

static void testCancelTrigger(void)
{
    _setUp(FALSE);
    CHECK(!s_failed);

    CHECK(pifTask_SetTrigger(s_p_work, 500));
    pifTask_CancelTrigger(s_p_work);
    s_now = 1000;
    _loop(3);
    CHECK(s_work_runs == 0);

    // A cut in is withdrawn too.
    CHECK(pifTask_SetCutinTrigger(s_p_work));
    pifTask_CancelTrigger(s_p_work);
    _loop(3);
    CHECK(s_work_runs == 0);

    // A paused task left alone still gets its trigger.
    CHECK(pifTask_SetTrigger(s_p_work, 0));
    _loop(3);
    CHECK(s_work_runs == 1);
}

static void testRestart(void)
{
    PifTask *p_task;

    _setUp(FALSE);
    CHECK(!s_failed);
    p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000, _doOther, NULL, FALSE);
    CHECK(p_task);

    // Paused well past its period: clearing pause would release it at once, Restart does not.
    s_now = 5000;
    pifTask_Restart(p_task);
    CHECK(!p_task->_pause);
    s_now = 5999;
    _loop(4);
    CHECK(s_other_runs == 0);
    s_now = 6000;
    _loop(4);
    CHECK(s_other_runs == 1);
}

static void testSubPeriod(void)
{
    uint32_t time_us;

    s_now = 100;
    time_us = s_now;
    s_now = 1099;
    CHECK(!pifTask_IsSubPeriodElapsed(&time_us, 1000));
    s_now = 1100;
    CHECK(pifTask_IsSubPeriodElapsed(&time_us, 1000) && time_us == 1100);
    // A late check keeps the grid.
    s_now = 2300;
    CHECK(pifTask_IsSubPeriodElapsed(&time_us, 1000) && time_us == 2100);
    CHECK(!pifTask_IsSubPeriodElapsed(&time_us, 1000));
    // A gap of more than a period starts over from now.
    s_now = 9000;
    CHECK(pifTask_IsSubPeriodElapsed(&time_us, 1000) && time_us == 9000);
}

static PifTaskTimeout s_timeout[2];
static int s_expired[2];
static int s_timeout_runs;

static uint32_t _doTimeouts(PifTask *p_task)
{
    int i;

    (void)p_task;
    s_timeout_runs++;
    for (i = 0; i < 2; i++) {
        if (pifTaskTimeout_IsExpired(&s_timeout[i])) s_expired[i]++;
    }
    return 0;
}

static void testTaskTimeout(void)
{
    PifTask *p_task;

    _setUp(FALSE);
    CHECK(!s_failed);
    p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTimeouts, NULL, FALSE);
    CHECK(p_task);
    pifTaskTimeout_Init(&s_timeout[0], p_task);
    pifTaskTimeout_Init(&s_timeout[1], p_task);
    s_expired[0] = s_expired[1] = 0;
    s_timeout_runs = 0;

    // The later one started first: the nearer one still brings the trigger forward.
    pifTaskTimeout_Start(&s_timeout[1], 800);
    pifTaskTimeout_Start(&s_timeout[0], 300);
    CHECK(pifTaskTimeout_IsRunning(&s_timeout[0]) && pifTaskTimeout_IsRunning(&s_timeout[1]));
    s_now = 299;
    _loop(4);
    CHECK(s_timeout_runs == 0);
    s_now = 300;
    _loop(4);
    CHECK(s_timeout_runs == 1 && s_expired[0] == 1 && s_expired[1] == 0);
    CHECK(!pifTaskTimeout_IsRunning(&s_timeout[0]));

    // The check of the one still running armed the trigger for it.
    s_now = 799;
    _loop(4);
    CHECK(s_timeout_runs == 1);
    s_now = 800;
    _loop(4);
    CHECK(s_timeout_runs == 2 && s_expired[0] == 1 && s_expired[1] == 1);

    // Nothing is running: the task stays asleep.
    s_now = 5000;
    _loop(4);
    CHECK(s_timeout_runs == 2);

    // A stopped timeout costs one run with nothing due, and is never reported.
    pifTaskTimeout_Start(&s_timeout[0], 100);
    pifTaskTimeout_Stop(&s_timeout[0]);
    s_now = 5100;
    _loop(4);
    CHECK(s_timeout_runs == 3 && s_expired[0] == 1);
    s_now = 9000;
    _loop(4);
    CHECK(s_timeout_runs == 3);

    // A restart moves the deadline back.
    pifTaskTimeout_Start(&s_timeout[0], 100);
    s_now = 9050;
    pifTaskTimeout_Start(&s_timeout[0], 100);
    s_now = 9100;
    _loop(4);
    CHECK(s_expired[0] == 1);
    s_now = 9150;
    _loop(4);
    CHECK(s_expired[0] == 2);
}

static void testPolledTimeout(void)
{
    PifTaskTimeout timeout = { 0 };

    s_now = 0;
    CHECK(!pifTaskTimeout_IsRunning(&timeout) && !pifTaskTimeout_IsExpired(&timeout));
    pifTaskTimeout_Start(&timeout, 0);
    CHECK(!pifTaskTimeout_IsExpired(&timeout));
    s_now = 1;
    CHECK(pifTaskTimeout_IsExpired(&timeout));
    CHECK(!pifTaskTimeout_IsExpired(&timeout));
}

static void testInvalid(void)
{
    CHECK(!pifTask_SetTrigger(NULL, 0) && pif_error == E_INVALID_PARAM);
    pif_error = E_SUCCESS;
    CHECK(!pifTask_SetCutinTrigger(NULL) && pif_error == E_INVALID_PARAM);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
#ifdef PIF_USE_TASK_REALTIME
        { "trigger waits for slack", testTriggerWaitsForSlack },
        { "trigger delay", testTriggerDelay },
        { "returned delay waits for slack", testReturnedDelayWaitsForSlack },
        { "trigger bounded", testTriggerBounded },
        { "max skip one", testMaxSkipOne },
        { "cut in ahead of realtime", testCutinAheadOfRealtime },
        { "cut in taken waits for slack", testCutinTakenWaitsForSlack },
#endif
        { "trigger without realtime", testTriggerWithoutRealtime },
#ifdef PIF_USE_TASK_REALTIME
        { "trigger on realtime", testTriggerOnRealtime },
#else
        { "realtime left out", testRealtimeLeftOut },
#endif
        { "cancel trigger", testCancelTrigger },
        { "restart", testRestart },
        { "sub period", testSubPeriod },
        { "task timeout", testTaskTimeout },
        { "polled timeout", testPolledTimeout },
        { "invalid", testInvalid },
    };

    // Before pif_Init() the time base stays still and the loop does not reach the missing timer.
    pif_sigTimer1ms();
    pifTaskManager_Loop();
    if (pif_ready || pif_cumulative_timer1ms) {
        printf("pif_task_manager: ran before pif_Init\n");
        return 1;
    }
    // Nor do the setup calls, nor their cleanup, reach the missing timer.
    if (pifTaskManager_Init(1, 0) || pif_error != E_INVALID_STATE) {
        printf("pif_task_manager: init before pif_Init\n");
        return 1;
    }
    if (pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000, _doOther, NULL, TRUE)) {
        printf("pif_task_manager: add before pif_Init\n");
        return 1;
    }
    pif_Exit();

    if (!pif_Init(_timer1us)) {
        printf("pif_task_manager: setup failed (%d)\n", pif_error);
        return 1;
    }
    if (!pif_ready) {
        printf("pif_task_manager: pif_ready not set\n");
        return 1;
    }

    printf("pif_task_manager\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    uint32_t ms_at_exit = pif_cumulative_timer1ms;
    pif_Exit();
    pif_sigTimer1ms();
    pifTaskManager_Loop();
    if (pif_ready || pif_cumulative_timer1ms != ms_at_exit) {
        printf("  FAIL ran after pif_Exit\n");
        failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
