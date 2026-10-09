// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_timer_manager.h"

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

#define MAX_TIMERS		4

static BOOL s_failed;
static PifTimerManager s_manager;
static uint32_t s_now_us;
static int s_finish[MAX_TIMERS];
static PifTimer *s_p_victim;
static char s_pwm[32];
static int s_pwm_len;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static void _evtFinish(PifIssuerP p_issuer)
{
    s_finish[(intptr_t)p_issuer]++;
}

static void _evtFinishRemove(PifIssuerP p_issuer)
{
    s_finish[(intptr_t)p_issuer]++;
    pifTimerManager_Remove(s_p_victim);
}

static void _actPwm(SWITCH value)
{
    if (s_pwm_len < (int)sizeof(s_pwm) - 1) s_pwm[s_pwm_len++] = value ? '1' : '0';
}

// One tick of the interrupt, then one pass of the main loop.
static void _tick(int count)
{
    while (count--) {
        pifTimerManager_sigTick(&s_manager);
        pifTaskManager_Loop();
    }
}

static BOOL _setUp(void)
{
    // A failed test returns early, so free what it left behind.
    pifTimerManager_Clear(&s_manager);
    memset(s_finish, 0, sizeof(s_finish));
    s_pwm_len = 0;
    memset(s_pwm, 0, sizeof(s_pwm));
    return pifTimerManager_Init(&s_manager, PIF_ID_AUTO, 1000, MAX_TIMERS);
}

static void testOnce(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_ONCE);
    CHECK(p_timer && p_timer->_step == TS_STOP);
    pifTimer_AttachEvtFinish(p_timer, _evtFinish, (PifIssuerP)0);
    CHECK(pifTimer_Start(p_timer, 3));
    _tick(2);
    CHECK(s_finish[0] == 0 && pifTimer_Remain(p_timer) == 1 && pifTimer_Elapsed(p_timer) == 2);
    _tick(1);
    CHECK(s_finish[0] == 1 && p_timer->_step == TS_STOP);
    _tick(5);
    CHECK(s_finish[0] == 1);
}

static void testRepeat(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_REPEAT);
    pifTimer_AttachEvtFinish(p_timer, _evtFinish, (PifIssuerP)0);
    CHECK(pifTimer_Start(p_timer, 2));
    _tick(6);
    CHECK(s_finish[0] == 3 && p_timer->_step == TS_RUNNING);
    pifTimer_Stop(p_timer);
    _tick(4);
    CHECK(s_finish[0] == 3);
    pifTimer_Reset(p_timer);
    _tick(2);
    CHECK(s_finish[0] == 4);
}

static void testIntFinish(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_REPEAT);
    pifTimer_AttachEvtIntFinish(p_timer, _evtFinish, (PifIssuerP)0);
    CHECK(pifTimer_Start(p_timer, 1));
    pifTimerManager_sigTick(&s_manager);
    pifTimerManager_sigTick(&s_manager);
    CHECK(s_finish[0] == 2);

    // Without a callback the handler only counts.
    pifTimer_AttachEvtIntFinish(p_timer, NULL, NULL);
    pifTimerManager_sigTick(&s_manager);
    CHECK(s_finish[0] == 2);
}

static void testPwm(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_PWM);
    p_timer->act_pwm = _actPwm;
    CHECK(pifTimer_Start(p_timer, 4));
    pifTimer_SetPwmDuty(p_timer, PIF_PWM_MAX_DUTY / 4);
    // Off at the start of each period, on for the last quarter of it.
    _tick(8);
    CHECK(strcmp(s_pwm, "1010") == 0);

    s_pwm_len = 0;
    memset(s_pwm, 0, sizeof(s_pwm));
    pifTimer_SetPwmDuty(p_timer, PIF_PWM_MAX_DUTY);
    _tick(8);
    CHECK(strcmp(s_pwm, "1") == 0);

    pifTimer_Stop(p_timer);
    CHECK(strcmp(s_pwm, "10") == 0);
}

static void testSetTarget(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_REPEAT);
    pifTimer_AttachEvtFinish(p_timer, _evtFinish, (PifIssuerP)0);
    CHECK(!pifTimer_SetTarget(p_timer, 0) && p_timer->_target == 0);
    CHECK(pifTimer_Start(p_timer, 2));
    _tick(1);

    // The period under way keeps its length, and the next one has the new count.
    CHECK(pifTimer_SetTarget(p_timer, 4) && p_timer->_step == TS_RUNNING && p_timer->_target == 4);
    _tick(1);
    CHECK(s_finish[0] == 1);
    _tick(3);
    CHECK(s_finish[0] == 1);
    _tick(1);
    CHECK(s_finish[0] == 2);
    CHECK(!pifTimer_SetTarget(p_timer, 0) && p_timer->_target == 4);

    // A finished one-shot timer is not brought back to life.
    pifTimer_Stop(p_timer);
    p_timer->_type = TT_ONCE;
    CHECK(pifTimer_Start(p_timer, 1));
    _tick(1);
    CHECK(p_timer->_step == TS_STOP && s_finish[0] == 3);
    CHECK(pifTimer_SetTarget(p_timer, 3) && p_timer->_step == TS_STOP && p_timer->_target == 3);
}

static void testPwmTarget(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_PWM);
    p_timer->act_pwm = _actPwm;

    // A duty set before the timer runs leaves it stopped.
    pifTimer_SetPwmDuty(p_timer, PIF_PWM_MAX_DUTY / 2);
    CHECK(p_timer->_step == TS_STOP);

    CHECK(pifTimer_Start(p_timer, 4));
    pifTimer_SetPwmDuty(p_timer, PIF_PWM_MAX_DUTY / 4);
    CHECK(p_timer->_step == TS_RUNNING && p_timer->__pwm_duty == 1);

    // The on-time follows the period, so the ratio stays a quarter.
    CHECK(pifTimer_SetTarget(p_timer, 8));
    CHECK(p_timer->_step == TS_RUNNING && p_timer->__pwm_duty == 2);
    _tick(4);
    s_pwm_len = 0;
    memset(s_pwm, 0, sizeof(s_pwm));
    // Off for 6 ticks, then on for 2.
    _tick(5);
    CHECK(strcmp(s_pwm, "") == 0);
    _tick(1);
    CHECK(strcmp(s_pwm, "1") == 0);
    _tick(2);
    CHECK(strcmp(s_pwm, "10") == 0);

    // A duty beyond the maximum is the maximum: the output stays on.
    pifTimer_SetPwmDuty(p_timer, PIF_PWM_MAX_DUTY + 1);
    CHECK(p_timer->__pwm_duty == 8 && strcmp(s_pwm, "101") == 0);
    _tick(16);
    CHECK(strcmp(s_pwm, "101") == 0);
}

static void testClear(void)
{
    PifTimer *p_pwm, *p_once;

    CHECK(_setUp());
    p_pwm = pifTimerManager_Add(&s_manager, TT_PWM);
    p_pwm->act_pwm = _actPwm;
    p_once = pifTimerManager_Add(&s_manager, TT_ONCE);
    pifTimer_AttachEvtIntFinish(p_once, _evtFinish, (PifIssuerP)0);
    CHECK(pifTimer_Start(p_pwm, 4) && pifTimer_Start(p_once, 2));
    pifTimer_SetPwmDuty(p_pwm, PIF_PWM_MAX_DUTY);
    CHECK(strcmp(s_pwm, "1") == 0);

    // The output is left off, and ticks that keep coming find nothing to count.
    pifTimerManager_Clear(&s_manager);
    CHECK(strcmp(s_pwm, "10") == 0 && pifTimerManager_Count(&s_manager) == 0);
    pifTimerManager_sigTick(&s_manager);
    pifTimerManager_sigTick(&s_manager);
    CHECK(s_finish[0] == 0 && strcmp(s_pwm, "10") == 0);
}

static void testRemove(void)
{
    PifTimer *p_timer;

    CHECK(_setUp());
    p_timer = pifTimerManager_Add(&s_manager, TT_ONCE);
    pifTimer_AttachEvtFinish(p_timer, _evtFinish, (PifIssuerP)0);
    CHECK(pifTimer_Start(p_timer, 1));

    // The tick raises the event, but the timer is removed before the task delivers it.
    pifTimerManager_sigTick(&s_manager);
    CHECK(p_timer->__event);
    pifTimerManager_Remove(p_timer);
    CHECK(pifTimer_Start(p_timer, 5) && p_timer->_step == TS_REMOVE);
    pifTimer_Stop(p_timer);
    pifTimer_Reset(p_timer);
    CHECK(p_timer->_step == TS_REMOVE);
    pifTaskManager_Loop();
    CHECK(s_finish[0] == 0 && pifTimerManager_Count(&s_manager) == 0);

    // The freed slot is handed out again, cleared, and the handler passes over it meanwhile.
    _tick(3);
    CHECK(pifTimerManager_Add(&s_manager, TT_ONCE) == p_timer && p_timer->_step == TS_STOP);
}

static void testRemoveFromCallback(void)
{
    PifTimer *p_first, *p_second;
    int i;

    CHECK(_setUp());
    // Added last, so the first is ahead of the second in the list the task walks.
    p_second = pifTimerManager_Add(&s_manager, TT_REPEAT);
    p_first = pifTimerManager_Add(&s_manager, TT_REPEAT);
    pifTimer_AttachEvtFinish(p_first, _evtFinishRemove, (PifIssuerP)0);
    pifTimer_AttachEvtFinish(p_second, _evtFinish, (PifIssuerP)1);
    s_p_victim = p_second;
    CHECK(pifTimer_Start(p_first, 1) && pifTimer_Start(p_second, 1));

    _tick(1);
    CHECK(s_finish[0] == 1 && s_finish[1] == 0 && pifTimerManager_Count(&s_manager) == 1);

    // A timer removed by its own callback is freed after the next tick, and its slot is reused.
    s_p_victim = p_first;
    _tick(1);
    CHECK(s_finish[0] == 2 && pifTimerManager_Count(&s_manager) == 1);
    _tick(1);
    CHECK(s_finish[0] == 2 && pifTimerManager_Count(&s_manager) == 0);
    for (i = 0; i < MAX_TIMERS; i++) CHECK(pifTimerManager_Add(&s_manager, TT_ONCE));
    CHECK(!pifTimerManager_Add(&s_manager, TT_ONCE));
}


typedef struct StTestCase
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "once", testOnce },
    { "repeat", testRepeat },
    { "interrupt finish", testIntFinish },
    { "pwm", testPwm },
    { "set target", testSetTarget },
    { "pwm target", testPwmTarget },
    { "clear", testClear },
    { "remove", testRemove },
    { "remove from callback", testRemoveFromCallback }
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(1, 2)) {
        printf("pif_timer: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_timer\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
