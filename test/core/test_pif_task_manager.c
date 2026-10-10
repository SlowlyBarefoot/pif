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
static uint32_t _doRealtime(PifTask *p_task)
{
    (void)p_task;
    s_rt_runs++;
    s_now += RT_RUN_US;
    return 0;
}

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
    s_p_rt = realtime ? pifTaskManager_Add(PIF_ID_AUTO, TM_REALTIME, RT_PERIOD_US, _doRealtime, NULL, TRUE) : NULL;
    s_p_work = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doWork, NULL, FALSE);
    s_p_other = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doOther, NULL, FALSE);
    if ((realtime && !s_p_rt) || !s_p_work || !s_p_other) s_failed = TRUE;
}

static void _loop(int count)
{
    while (count--) pifTaskManager_Loop();
}

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

static void testTriggerOnRealtime(void)
{
    _setUp(TRUE);
    CHECK(!s_failed);

    s_now = 10;
    CHECK(pifTask_SetTrigger(s_p_rt, 0));
    _loop(1);
    CHECK(s_rt_runs == 1);
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
        { "trigger waits for slack", testTriggerWaitsForSlack },
        { "trigger delay", testTriggerDelay },
        { "returned delay waits for slack", testReturnedDelayWaitsForSlack },
        { "trigger bounded", testTriggerBounded },
        { "max skip one", testMaxSkipOne },
        { "cut in ahead of realtime", testCutinAheadOfRealtime },
        { "cut in taken waits for slack", testCutinTakenWaitsForSlack },
        { "trigger without realtime", testTriggerWithoutRealtime },
        { "trigger on realtime", testTriggerOnRealtime },
        { "invalid", testInvalid },
    };

    if (!pif_Init(_timer1us)) {
        printf("pif_task_manager: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_task_manager\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
