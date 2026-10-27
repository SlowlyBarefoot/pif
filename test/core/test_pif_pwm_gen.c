// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_pwm_gen.h"

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

#define MAX_TIMERS		2

static BOOL s_failed;
static PifTimerManager s_manager;
static PifPwmGen s_pwm;
static uint32_t s_now_us;
static char s_out[64];
static int s_out_len;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static void _actPwm(SWITCH value)
{
    if (s_out_len < (int)sizeof(s_out) - 1) s_out[s_out_len++] = value ? '1' : '0';
}

static void _clearOut(void)
{
    s_out_len = 0;
    memset(s_out, 0, sizeof(s_out));
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
    _clearOut();
    if (!pifTimerManager_Init(&s_manager, PIF_ID_AUTO, 1000, MAX_TIMERS)) return FALSE;
    return pifPwmGen_Init(&s_pwm, PIF_ID_AUTO, &s_manager, _actPwm);
}

static void testInit(void)
{
    PifPwmGen pwm;

    CHECK(_setUp());
    CHECK(!pifPwmGen_Init(NULL, PIF_ID_AUTO, &s_manager, _actPwm));
    CHECK(!pifPwmGen_Init(&pwm, PIF_ID_AUTO, NULL, _actPwm));
    CHECK(!pifPwmGen_Init(&pwm, PIF_ID_AUTO, &s_manager, NULL));
    CHECK(!s_pwm._running && s_pwm._duty == 0 && pifTimerManager_Count(&s_manager) == 1);
    CHECK(!pifPwmGen_Start(&s_pwm, 0) && !pifPwmGen_SetPeriod(&s_pwm, 0));

    // One timer each, so the manager runs out.
    CHECK(pifPwmGen_Init(&pwm, PIF_ID_AUTO, &s_manager, _actPwm));
    CHECK(!pifPwmGen_Init(&pwm, PIF_ID_AUTO, &s_manager, _actPwm));
    CHECK(strcmp(s_out, "") == 0);
}

static void testDuty(void)
{
    CHECK(_setUp());

    // A duty set before the start is kept, and nothing is output until then.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 4);
    CHECK(strcmp(s_out, "") == 0);
    CHECK(pifPwmGen_Start(&s_pwm, 4) && s_pwm._running);
    // On for the first quarter of each period, off for the rest.
    CHECK(strcmp(s_out, "1") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "10") == 0);
    _tick(2);
    CHECK(strcmp(s_out, "10") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "101") == 0);
    _tick(4);
    CHECK(strcmp(s_out, "10101") == 0);

    // A duty beyond the maximum is the maximum.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY + 1);
    CHECK(s_pwm._duty == PIF_PWM_GEN_MAX_DUTY);
}

static void testHold(void)
{
    CHECK(_setUp());

    // At 0 the output is held off and the timer does not run.
    CHECK(pifPwmGen_Start(&s_pwm, 4));
    CHECK(strcmp(s_out, "0") == 0 && s_pwm.__p_timer->_step == TS_STOP);
    _tick(8);
    CHECK(strcmp(s_out, "0") == 0);

    // A held output has no period under way, so a new duty applies at once.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY);
    CHECK(strcmp(s_out, "01") == 0 && s_pwm.__p_timer->_step == TS_STOP);
    _tick(8);
    CHECK(strcmp(s_out, "01") == 0);

    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(strcmp(s_out, "011") == 0 && s_pwm.__p_timer->_step == TS_RUNNING);
    _tick(2);
    CHECK(strcmp(s_out, "0110") == 0);
    _tick(2);
    CHECK(strcmp(s_out, "01101") == 0);
}

static void testNextPeriod(void)
{
    CHECK(_setUp());
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(pifPwmGen_Start(&s_pwm, 4));
    _tick(1);

    // The period under way keeps its phases, and the next one has the new duty.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 4);
    _tick(1);
    CHECK(strcmp(s_out, "10") == 0);
    _tick(2);
    CHECK(strcmp(s_out, "101") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "1010") == 0);
    _tick(3);
    CHECK(strcmp(s_out, "10101") == 0);

    // Going to the maximum, the output finishes the period and then stays on.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY);
    _tick(1);
    CHECK(strcmp(s_out, "101010") == 0);
    _tick(3);
    CHECK(strcmp(s_out, "1010101") == 0 && s_pwm.__p_timer->_step == TS_STOP);
    _tick(8);
    CHECK(strcmp(s_out, "1010101") == 0 && s_pwm._running);

    // Out of the hold a new period begins at once, and going to 0 the output stays off from the
    // next one.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(strcmp(s_out, "10101011") == 0);
    pifPwmGen_SetDuty(&s_pwm, 0);
    _tick(2);
    CHECK(strcmp(s_out, "101010110") == 0);
    _tick(2);
    CHECK(strcmp(s_out, "1010101100") == 0 && s_pwm.__p_timer->_step == TS_STOP);
    _tick(8);
    CHECK(strcmp(s_out, "1010101100") == 0);
}

static void testPeriod(void)
{
    CHECK(_setUp());
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 4);
    CHECK(pifPwmGen_Start(&s_pwm, 4));

    // The ratio stays a quarter: on for 2 ticks and off for 6 from the next period.
    CHECK(pifPwmGen_SetPeriod(&s_pwm, 8) && s_pwm._period == 8);
    _tick(4);
    CHECK(strcmp(s_out, "101") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "101") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "1010") == 0);
    _tick(5);
    CHECK(strcmp(s_out, "1010") == 0);
    _tick(1);
    CHECK(strcmp(s_out, "10101") == 0);

    // A period too short for the ratio holds the output off, from the next period.
    CHECK(pifPwmGen_SetPeriod(&s_pwm, 2));
    _tick(2);
    CHECK(strcmp(s_out, "101010") == 0);
    _tick(6);
    CHECK(strcmp(s_out, "1010100") == 0 && s_pwm.__p_timer->_step == TS_STOP);
}

static void testStop(void)
{
    CHECK(_setUp());
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(pifPwmGen_Start(&s_pwm, 4));

    // Stopped in the on-time, the output is left off and nothing more comes.
    pifPwmGen_Stop(&s_pwm);
    CHECK(strcmp(s_out, "10") == 0 && !s_pwm._running);
    _tick(8);
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY);
    pifPwmGen_SetPeriod(&s_pwm, 2);
    CHECK(strcmp(s_out, "10") == 0);

    // A start begins a new period with what was set meanwhile.
    CHECK(pifPwmGen_Start(&s_pwm, 4));
    CHECK(strcmp(s_out, "101") == 0);

    // Out of the hold a new period begins at once, and a restart in the middle of one begins
    // another.
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(strcmp(s_out, "1011") == 0);
    _tick(3);
    CHECK(strcmp(s_out, "10110") == 0);
    CHECK(pifPwmGen_Start(&s_pwm, 4));
    CHECK(strcmp(s_out, "101101") == 0);
    _tick(2);
    CHECK(strcmp(s_out, "1011010") == 0);
}

static void testClear(void)
{
    CHECK(_setUp());
    pifPwmGen_SetDuty(&s_pwm, PIF_PWM_GEN_MAX_DUTY / 2);
    CHECK(pifPwmGen_Start(&s_pwm, 4));

    // The output is left off, and the timer goes back to the manager.
    pifPwmGen_Clear(&s_pwm);
    CHECK(strcmp(s_out, "10") == 0 && !s_pwm._running && !s_pwm.__p_timer);
    _tick(8);
    CHECK(strcmp(s_out, "10") == 0 && pifTimerManager_Count(&s_manager) == 0);
    pifPwmGen_Clear(&s_pwm);
    CHECK(strcmp(s_out, "10") == 0);
}


typedef struct StTestCase
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "init", testInit },
    { "duty", testDuty },
    { "hold", testHold },
    { "next period", testNextPeriod },
    { "period", testPeriod },
    { "stop", testStop },
    { "clear", testClear }
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(1, 2);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_pwm_gen: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_pwm_gen\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
