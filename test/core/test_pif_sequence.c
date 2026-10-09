// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_sequence.h"

#include <stdio.h>


// Only pif_sequence.c is linked, so the globals normally defined in pif.c live here.
PifError pif_error = E_SUCCESS;
volatile uint32_t pif_cumulative_timer1ms = 0;

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

// Steps of the simulated clock, so that a release can fall between two ticks of the millisecond
// counter the way it does on a target.
#define SIM_STEP_US		100

static BOOL s_failed;
static char s_trace[32];
static int s_len;

// A single TM_PERIOD task stands in for the task manager: it runs when it is not paused and its
// period has passed since its last release, and what the loop returns is its next period.
static PifTask s_task;
static PifEvtTaskLoop s_loop;
static int s_task_count;
static uint32_t s_now_us;		// Free running, apart from the millisecond counter
static uint32_t s_sub_us;
static uint32_t s_pretime_us;
static uint32_t s_period_us;
static uint32_t s_default_us;
static int s_runs;


PifTask *pifTaskManager_Add(PifId id, PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void *p_client, BOOL start)
{
    memset(&s_task, 0, sizeof(s_task));
    s_task._mode = mode;
    s_task._p_client = p_client;
    s_task.pause = !start;
    s_loop = evt_loop;
    s_default_us = period;
    s_period_us = period;
    s_pretime_us = s_now_us;
    s_task_count++;
    return &s_task;
}

void pifTaskManager_Remove(PifTask *p_task)
{
    s_task_count--;
}

BOOL pifTask_ChangePeriod(PifTask *p_owner, uint32_t period)
{
    s_default_us = period;
    s_period_us = period;
    return TRUE;
}

static void _mark(char c)
{
    if (s_len < (int)sizeof(s_trace) - 1) s_trace[s_len++] = c;
    s_trace[s_len] = 0;
}

static void _reset(uint32_t now_ms)
{
    s_now_us = 0;
    s_sub_us = 0;
    pif_cumulative_timer1ms = now_ms;
    pif_error = E_SUCCESS;
    s_task_count = 0;
    s_runs = 0;
    s_len = 0;
    s_trace[0] = 0;
}

static void _schedule()
{
    uint32_t period;

    if (!s_task_count || s_task.pause || s_now_us - s_pretime_us < s_period_us) return;
    s_pretime_us = s_now_us;
    s_runs++;
    period = (*s_loop)(&s_task);
    s_period_us = period ? period : s_default_us;
}

// Moves the clock on by ms, letting the task run wherever it is due.
static void _advance(uint32_t ms)
{
    uint32_t steps = ms * 1000 / SIM_STEP_US;

    _schedule();
    while (steps--) {
        s_now_us += SIM_STEP_US;
        s_sub_us += SIM_STEP_US;
        if (s_sub_us == 1000) {
            s_sub_us = 0;
            pif_cumulative_timer1ms++;
        }
        _schedule();
    }
}

static void _stepEnd(PifSequence *p_seq) { _mark('E'); }
static void _stepTimeout(PifSequence *p_seq) { _mark('T'); }
static void _stepC(PifSequence *p_seq) { _mark('C'); pifSequence_Next(p_seq, _stepEnd); }
static void _stepB(PifSequence *p_seq) { _mark('B'); pifSequence_Delay(p_seq, _stepC, 10); }
static void _stepA(PifSequence *p_seq) { _mark('A'); pifSequence_Next(p_seq, _stepB); }
static void _stepZero(PifSequence *p_seq) { _mark('Z'); pifSequence_Delay(p_seq, _stepEnd, 0); }
static void _stepWait(PifSequence *p_seq) { _mark('W'); pifSequence_Wait(p_seq, _stepEnd, 20, _stepTimeout); }
static void _stepWaitEnd(PifSequence *p_seq) { _mark('W'); pifSequence_Wait(p_seq, _stepEnd, 20, NULL); }
static void _stepWaitForever(PifSequence *p_seq) { _mark('W'); pifSequence_Wait(p_seq, _stepEnd, 0, NULL); }
static void _stepDelayThenWait(PifSequence *p_seq) { _mark('D'); pifSequence_Delay(p_seq, _stepWait, 10); }

// The answer arrives before the step that sent the request has returned.
static void _stepWaitAnswered(PifSequence *p_seq)
{
    _mark('W');
    pifSequence_Wait(p_seq, _stepEnd, 20, _stepTimeout);
    pifSequence_Signal(p_seq);
}

static void _start(PifSequence *p_seq, PifSequenceStep step)
{
    pifSequence_Init(p_seq, PIF_ID_AUTO, NULL);
    pifSequence_Start(p_seq, step);
}

// Releases the task once at the current time, whether it is due or not, and returns the period
// its loop asked for.
static uint32_t _release()
{
    return (*s_loop)(&s_task);
}

// The period the task asks for after each step.
static void _testPeriod()
{
    PifSequence seq;

    _reset(0);
    CHECK(pifSequence_Init(&seq, PIF_ID_AUTO, NULL));
    CHECK(!pifSequence_Start(&seq, NULL));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(pifSequence_Start(&seq, _stepA));
    CHECK(!pifSequence_Start(&seq, _stepA));
    CHECK(pif_error == E_INVALID_STATE);

    // One step per release, even when a step asks for its successor without delay.
    CHECK(_release() == PIF_SEQUENCE_NEXT_US);
    CHECK(strcmp(s_trace, "A") == 0);
    CHECK(_release() == 10000);
    CHECK(strcmp(s_trace, "AB") == 0);

    // A release before the step is due asks again for the time left.
    pif_cumulative_timer1ms += 4;
    CHECK(_release() == 6000);
    pif_cumulative_timer1ms += 6;
    CHECK(_release() == PIF_SEQUENCE_NEXT_US);
    CHECK(strcmp(s_trace, "ABC") == 0);
    CHECK(_release() == 0);
    CHECK(strcmp(s_trace, "ABCE") == 0);
    CHECK(!pifSequence_IsRunning(&seq));
    CHECK(s_task.pause);

    // A wait with a long timeout still comes back often enough to see its signal.
    CHECK(pifSequence_Start(&seq, _stepWait));
    CHECK(_release() == PIF_SEQUENCE_WAIT_POLL_US);
    pif_cumulative_timer1ms += 19;
    CHECK(_release() == 1000);

    // A delay of zero is due at once.
    pifSequence_Stop(&seq);
    CHECK(s_task.pause);
    CHECK(pifSequence_Start(&seq, _stepZero));
    CHECK(_release() == PIF_SEQUENCE_NEXT_US);
    CHECK(_release() == 0);
    CHECK(strstr(s_trace, "ZE") != NULL);

    // Without its task the sequence cannot start.
    pifSequence_Clear(&seq);
    CHECK(!pifSequence_Start(&seq, _stepA));
    CHECK(pif_error == E_INVALID_STATE);
}

static void _testOwnTask()
{
    PifSequence seq;

    _reset(0);
    CHECK(pifSequence_Init(&seq, PIF_ID_AUTO, NULL));
    CHECK(s_task_count == 1);
    CHECK(seq._p_task == &s_task);
    CHECK(s_task._mode == TM_PERIOD);
    CHECK(s_task.pause);

    CHECK(pifSequence_Start(&seq, _stepA));
    CHECK(!s_task.pause);
    _advance(2);
    CHECK(strcmp(s_trace, "AB") == 0);
    _advance(7);
    CHECK(strcmp(s_trace, "AB") == 0);
    _advance(3);
    CHECK(strcmp(s_trace, "ABCE") == 0);

    // An idle sequence pauses its task, so it costs no runs until it is started again.
    CHECK(s_task.pause);
    s_runs = 0;
    _advance(50);
    CHECK(s_runs == 0);
    CHECK(pifSequence_Start(&seq, _stepEnd));
    _advance(2);
    CHECK(strcmp(s_trace, "ABCEE") == 0);

    pifSequence_Clear(&seq);
    CHECK(s_task_count == 0);
    CHECK(seq._p_task == NULL);
}

static void _testDelayWrap()
{
    PifSequence seq;

    _reset(0xFFFFFFFAu);
    _start(&seq, _stepB);
    _advance(9);
    CHECK(strcmp(s_trace, "B") == 0);
    _advance(3);
    CHECK(strcmp(s_trace, "BCE") == 0);
    pifSequence_Clear(&seq);
}

static void _testSignal()
{
    PifSequence seq;

    // A signal raised before the wait begins is discarded.
    _reset(100);
    _start(&seq, _stepWait);
    pifSequence_Signal(&seq);
    _advance(5);
    CHECK(strcmp(s_trace, "W") == 0);
    pifSequence_Signal(&seq);
    _advance(2);
    CHECK(strcmp(s_trace, "WE") == 0);
    pifSequence_Clear(&seq);

    // A signal during a delay neither cuts it short nor carries into the following wait.
    _reset(100);
    _start(&seq, _stepDelayThenWait);
    _advance(1);
    pifSequence_Signal(&seq);
    _advance(8);
    CHECK(strcmp(s_trace, "D") == 0);
    _advance(2);
    CHECK(strcmp(s_trace, "DW") == 0);
    _advance(5);
    CHECK(strcmp(s_trace, "DW") == 0);
    pifSequence_Clear(&seq);

    // A signal raised within the waiting step is not held back until the next look.
    _reset(100);
    _start(&seq, _stepWaitAnswered);
    _advance(1);
    CHECK(strcmp(s_trace, "W") == 0);
    s_now_us += SIM_STEP_US;
    _schedule();
    CHECK(strcmp(s_trace, "WE") == 0);
    pifSequence_Clear(&seq);
}

static void _testTimeout()
{
    PifSequence seq;

    _reset(0);
    _start(&seq, _stepWait);
    _advance(19);
    CHECK(strcmp(s_trace, "W") == 0);
    _advance(2);
    CHECK(strcmp(s_trace, "WT") == 0);
    CHECK(!pifSequence_IsRunning(&seq));
    pifSequence_Clear(&seq);

    // Without a timeout step the sequence ends and reports the timeout.
    _reset(0);
    _start(&seq, _stepWaitEnd);
    _advance(21);
    CHECK(strcmp(s_trace, "W") == 0);
    CHECK(!pifSequence_IsRunning(&seq));
    CHECK(pif_error == E_TIMEOUT);
    pifSequence_Clear(&seq);

    // A zero timeout waits forever.
    _reset(0);
    _start(&seq, _stepWaitForever);
    _advance(70000);
    CHECK(pifSequence_IsRunning(&seq));
    pifSequence_Signal(&seq);
    _advance(2);
    CHECK(strcmp(s_trace, "WE") == 0);
    pifSequence_Clear(&seq);
}

static void _testStop()
{
    PifSequence seq;

    _reset(0);
    _start(&seq, _stepB);
    _advance(1);
    pifSequence_Stop(&seq);
    CHECK(!pifSequence_IsRunning(&seq));
    CHECK(s_task.pause);
    _advance(20);
    CHECK(strcmp(s_trace, "B") == 0);

    // A restart is not held back by the long delay the stop interrupted.
    CHECK(pifSequence_Start(&seq, _stepEnd));
    _advance(2);
    CHECK(strcmp(s_trace, "BE") == 0);
    pifSequence_Clear(&seq);
}

int main()
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "period", _testPeriod },
        { "own task", _testOwnTask },
        { "delay wrap", _testDelayWrap },
        { "signal", _testSignal },
        { "timeout", _testTimeout },
        { "stop", _testStop },
    };
    int failed = 0;

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        s_failed = FALSE;
        tests[i].run();
        printf("%s %s\n", s_failed ? "FAIL" : "ok  ", tests[i].name);
        if (s_failed) failed++;
    }
    return failed ? 1 : 0;
}
