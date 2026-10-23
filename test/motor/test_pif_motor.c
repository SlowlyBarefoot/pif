// SPDX-License-Identifier: BSD-3-Clause
#include "motor/pif_duty_motor.h"
#include "motor/pif_step_motor.h"

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

#define TICK_US			100
#define MAX_TIMERS		8
#define MAX_PHASES		32

static BOOL s_failed;
static PifTimerManager s_manager;
static uint32_t s_now_us;
static PifStepMotor s_step;
static PifDutyMotor s_duty;
static uint16_t s_phase[MAX_PHASES];
static int s_phase_count;
static void* s_p_act_owner;
static int s_stop_count;
static int s_error_count;
static int s_duty_count;
static uint16_t s_last_duty;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static uint32_t _doNothing(PifTask* p_task)
{
    (void)p_task;
    return 0;
}

static void _actSetStep(PifStepMotor* p_owner, uint16_t phase)
{
    s_p_act_owner = p_owner;
    if (s_phase_count < MAX_PHASES) s_phase[s_phase_count] = phase;
    s_phase_count++;
}

static void _actSetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    s_p_act_owner = p_owner;
    s_last_duty = duty;
    s_duty_count++;
}

static void _evtStepStop(PifStepMotor* p_owner)
{
    (void)p_owner;
    s_stop_count++;
}

static void _evtStepError(PifStepMotor* p_owner)
{
    (void)p_owner;
    s_error_count++;
}

static void _evtDutyError(PifDutyMotor* p_owner)
{
    (void)p_owner;
    s_error_count++;
}

// One tick of the interrupt, then one pass of the main loop.
static void _tick(int count)
{
    while (count--) {
        s_now_us += TICK_US;
        pifTimerManager_sigTick(&s_manager);
        pifTaskManager_Loop();
    }
}

static BOOL _setUp(void)
{
    // A failed test returns early, so free what it left behind.
    pifTimerManager_Clear(&s_manager);
    s_phase_count = 0;
    s_p_act_owner = NULL;
    s_stop_count = 0;
    s_error_count = 0;
    s_duty_count = 0;
    s_last_duty = 0;
    return pifTimerManager_Init(&s_manager, PIF_ID_AUTO, TICK_US, MAX_TIMERS);
}

static BOOL _setUpStep(PifStepMotorOperation operation)
{
    if (!_setUp() || !pifStepMotor_Init(&s_step, PIF_ID_AUTO, &s_manager, 200, operation)) return FALSE;
    s_step.act_set_step = _actSetStep;
    s_step.evt_stop = _evtStepStop;
    s_step.evt_error = _evtStepError;
    return TRUE;
}

static void testStepInitChecks(void)
{
    CHECK(_setUp());
    CHECK(!pifStepMotor_Init(&s_step, PIF_ID_AUTO, &s_manager, 200, (PifStepMotorOperation)99));
    CHECK(!pifStepMotor_Init(&s_step, PIF_ID_AUTO, &s_manager, 0, SMO_2P_4W_1S));
    CHECK(_setUpStep(SMO_2P_4W_1S));
    CHECK(!pifStepMotor_SetReductionGearRatio(&s_step, 0));
    CHECK(!pifStepMotor_SetPps(&s_step, 0));
}

static void testStepLowPps(void)
{
    CHECK(_setUpStep(SMO_2P_4W_1S));
    // 15 pps used to overflow the 16-bit period.
    CHECK(pifStepMotor_SetPps(&s_step, 15));
    CHECK(s_step.__step_period1us == 66667);
    CHECK(pifStepMotor_SetPps(&s_step, 1));
    CHECK(s_step.__step_period1us == 1000000);
}

#ifdef PIF_USE_STEP_MOTOR_STEP_DIR

static void testStepDirPulses(void)
{
    int i;

    CHECK(_setUpStep(SMO_STEP_DIR));
    CHECK(pifStepMotor_SetDirection(&s_step, 1));
    CHECK(pifStepMotor_SetPps(&s_step, 1000));				// 500us per tick: 5 ticks of the timer manager
    CHECK(s_step.__step_period1us == 500);
    CHECK(pifStepMotor_Start(&s_step, 3));

    // Start enables the driver and sets DIR before the first STEP edge.
    CHECK(s_phase_count == 1 && s_phase[0] == (SMSD_ENABLE | SMSD_DIR));
    CHECK(s_p_act_owner == &s_step);

    _tick(5 * 10);
    CHECK(s_phase_count == 7);
    for (i = 1; i < 7; i++) {
        uint16_t step = (i & 1) ? SMSD_STEP : 0;
        CHECK(s_phase[i] == (SMSD_ENABLE | SMSD_DIR | step));
    }
    CHECK(s_step._current_pulse == 3);
    CHECK(s_stop_count == 1);

    pifStepMotor_Release(&s_step);
    CHECK(s_phase[s_phase_count - 1] == 0);
}

#endif

static void testStepPulseUnit(void)
{
    CHECK(_setUpStep(SMO_2P_4W_1_2S));
    CHECK(pifStepMotor_SetReductionGearRatio(&s_step, 2));
    // A pulse is 2 motor full steps of 2 half steps each: 4 ticks.
    CHECK(pifStepMotor_SetPps(&s_step, 250));
    CHECK(s_step.__step_period1us == 1000);
    CHECK(pifStepMotor_Start(&s_step, 2));
    _tick(10 * 10);
    CHECK(s_phase_count == 8);
    CHECK(s_step._current_pulse == 2);
    CHECK(pifStepMotor_GetRpm(&s_step) == 60.0f * 250 / 200);
}

static void testStepGearRatioKeepsSpeed(void)
{
    CHECK(_setUpStep(SMO_2P_4W_1S));
    CHECK(pifStepMotor_SetPps(&s_step, 500));
    CHECK(s_step.__step_period1us == 2000);
    // Changing the ratio or the operation reapplies the current pps.
    CHECK(pifStepMotor_SetReductionGearRatio(&s_step, 4));
    CHECK(s_step.__step_period1us == 500);
    CHECK(pifStepMotor_SetOperation(&s_step, SMO_2P_4W_1_2S));
    CHECK(s_step.__step_period1us == 250);
}

static void testStepErrorOnce(void)
{
    CHECK(_setUpStep(SMO_2P_4W_1S));
    s_step._p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000000, _doNothing, NULL, FALSE);
    CHECK(s_step._p_task);
    CHECK(pifStepMotor_StartControl(&s_step));

    pifStepMotor_SetState(&s_step, MS_CONST);
    s_step._error = ME_STOP_POSITION;
    pifStepMotor_Control(&s_step);
    CHECK(s_step._state == MS_BRAKE);
    CHECK(s_error_count == 1);
    pifStepMotor_Control(&s_step);
    CHECK(s_error_count == 1);

    // A new run reports a new error again.
    CHECK(pifStepMotor_StartControl(&s_step));
    pifStepMotor_Control(&s_step);
    CHECK(s_error_count == 2);

    pifTaskManager_Remove(s_step._p_task);
}

static void testStepOperatingTimeCancelled(void)
{
    CHECK(_setUpStep(SMO_2P_4W_1S));
    s_step._p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000000, _doNothing, NULL, FALSE);
    CHECK(s_step._p_task);

    CHECK(pifStepMotor_SetOperatingTime(&s_step, 10));
    pifStepMotor_SetState(&s_step, MS_CONST);
    _tick(3);
    // The run stops early.
    pifStepMotor_SetState(&s_step, MS_STOP);
    pifStepMotor_Control(&s_step);
    CHECK(s_step._state == MS_IDLE);

    // The next run, without an operating time, is not cut short by the old one.
    pifStepMotor_SetState(&s_step, MS_CONST);
    _tick(20);
    CHECK(s_step._state == MS_CONST);

    pifTaskManager_Remove(s_step._p_task);
}

#ifdef PIF_USE_STEP_MOTOR_5PHASE

static int _bitCount(uint16_t value)
{
    int count = 0;

    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

static void testStep5PhasePentagon(void)
{
    static const uint16_t forward[10] = { 0x12, 0x1A, 0x0A, 0x0B, 0x09, 0x0D, 0x05, 0x15, 0x14, 0x16 };
    int i;

    CHECK(_setUpStep(SMO_5P_PG));
    CHECK(pifStepMotor_SetPps(&s_step, 1000));				// Full step: one tick per pulse
    CHECK(s_step.__step_period1us == 1000);
    CHECK(pifStepMotor_Start(&s_step, 10));
    _tick(10 * 10);
    CHECK(s_phase_count == 10 && s_step._current_pulse == 10);
    for (i = 0; i < 10; i++) {
        uint16_t prev = s_phase[(i + 9) % 10];

        CHECK(s_phase[i] == forward[i]);
        // One terminal moves per step, so 3 and 2 terminals are high in turn.
        CHECK(_bitCount(s_phase[i] ^ prev) == 1);
        CHECK(_bitCount(s_phase[i]) == ((i & 1) ? 3 : 2));
    }

    // Backward runs the same states in reverse, from where the motor stopped.
    s_phase_count = 0;
    CHECK(pifStepMotor_SetDirection(&s_step, 1));
    CHECK(pifStepMotor_Start(&s_step, 10));
    _tick(10 * 10);
    CHECK(s_phase_count == 10);
    for (i = 0; i < 10; i++) {
        CHECK(s_phase[i] == forward[(18 - i) % 10]);
    }

    pifStepMotor_Release(&s_step);
    CHECK(s_phase[s_phase_count - 1] == 0);
}

static void testStep5PhasePentagonHalf(void)
{
    static const uint16_t full[10] = { 0x16, 0x12, 0x1A, 0x0A, 0x0B, 0x09, 0x0D, 0x05, 0x15, 0x14 };
    int i;

    CHECK(_setUpStep(SMO_5P_PG_HALF));
    CHECK(pifStepMotor_SetPps(&s_step, 500));				// Half step: two ticks per pulse
    CHECK(s_step.__step_period1us == 1000);
    CHECK(pifStepMotor_Start(&s_step, 10));
    _tick(10 * 20);
    CHECK(s_phase_count == 20 && s_step._current_pulse == 10);
    for (i = 0; i < 20; i++) {
        // Step i + 1 of the table: the full steps of SMO_5P_PG at even steps, half steps between them.
        int step = (i + 1) % 20;
        uint16_t prev = full[step / 2];
        uint16_t next = full[(step / 2 + 1) % 10];
        uint16_t phase = s_phase[i];

        if (step & 1) {
            // The terminal that moves is off and its level is clear.
            CHECK((phase & SM5P_LEVEL_MASK) == (prev & next));
            CHECK((phase >> 8) == (prev ^ next));
        }
        else {
            CHECK(phase == prev);
        }
    }

    pifStepMotor_Release(&s_step);
    CHECK(s_phase[s_phase_count - 1] == 0);
}

#endif

#ifdef PIF_USE_STEP_MOTOR_STEP_DIR

static uint16_t s_microstep;

static void _actSetMicrostep(PifStepMotor* p_owner, uint16_t microstep)
{
    (void)p_owner;
    s_microstep = microstep;
}

static void testStepMicrostep(void)
{
    int i;

    CHECK(_setUpStep(SMO_STEP_DIR));
    s_step.act_set_microstep = _actSetMicrostep;
    s_microstep = 0;
    CHECK(s_step._microstep == 1);
    CHECK(!pifStepMotor_SetMicrostep(&s_step, 0));
    CHECK(!pifStepMotor_SetMicrostep(&s_step, 3));
    CHECK(!pifStepMotor_SetMicrostep(&s_step, 512));
    CHECK(s_microstep == 0);

    CHECK(pifStepMotor_SetPps(&s_step, 125));
    CHECK(s_step.__step_period1us == 4000);
    // The speed in full steps is kept: 4 STEP pulses per full step, so the ticks are 4 times shorter.
    CHECK(pifStepMotor_SetMicrostep(&s_step, 4));
    CHECK(s_microstep == 4 && s_step._microstep == 4);
    CHECK(s_step.__step_period1us == 1000);
    CHECK(s_step._current_pps == 125);

    CHECK(pifStepMotor_Start(&s_step, 2));
    CHECK(!pifStepMotor_SetMicrostep(&s_step, 8));			// Not while stepping
    _tick(10 * 20);
    // The enable before the first edge, then 2 full steps of 4 STEP pulses.
    CHECK(s_phase_count == 1 + 2 * 4 * 2);
    for (i = 1; i < s_phase_count; i++) {
        CHECK(s_phase[i] == (SMSD_ENABLE | ((i & 1) ? SMSD_STEP : 0)));
    }
    CHECK(s_step._current_pulse == 2);
    CHECK(pifStepMotor_GetRpm(&s_step) == 60.0f * 125 / 200);
}

#endif

#if !defined(PIF_USE_STEP_MOTOR_5PHASE) || !defined(PIF_USE_STEP_MOTOR_STEP_DIR)

static void testStepLeftOutOperations(void)
{
    CHECK(_setUp());
    // An operation left out of the build is refused as an unknown one, at Init and later.
#ifndef PIF_USE_STEP_MOTOR_5PHASE
    pif_error = E_SUCCESS;
    CHECK(!pifStepMotor_Init(&s_step, PIF_ID_AUTO, &s_manager, 200, SMO_5P_PG) && pif_error == E_INVALID_PARAM);
#endif
#ifndef PIF_USE_STEP_MOTOR_STEP_DIR
    pif_error = E_SUCCESS;
    CHECK(!pifStepMotor_Init(&s_step, PIF_ID_AUTO, &s_manager, 200, SMO_STEP_DIR) && pif_error == E_INVALID_PARAM);
#endif
    CHECK(_setUpStep(SMO_2P_4W_1S));
#ifndef PIF_USE_STEP_MOTOR_5PHASE
    CHECK(!pifStepMotor_SetOperation(&s_step, SMO_5P_PG_HALF));
#endif
#ifndef PIF_USE_STEP_MOTOR_STEP_DIR
    CHECK(!pifStepMotor_SetOperation(&s_step, SMO_STEP_DIR));
#endif
    CHECK(s_step._operation == SMO_2P_4W_1S);
    CHECK(pifStepMotor_SetOperation(&s_step, SMO_2P_4W_1_2S));
}

#endif

static void testDutyStart(void)
{
    CHECK(_setUp());
    CHECK(pifDutyMotor_Init(&s_duty, PIF_ID_AUTO, &s_manager, 100));
    CHECK(!pifDutyMotor_Start(&s_duty, 50));
    s_duty.act_set_duty = _actSetDuty;
    // act_set_direction is optional and the duty is clamped to the maximum.
    CHECK(pifDutyMotor_Start(&s_duty, 150));
    CHECK(s_duty_count == 1 && s_last_duty == 100 && s_duty._current_duty == 100);
    CHECK(s_p_act_owner == &s_duty);
}

static void testDutyNoBrakeCallback(void)
{
    CHECK(_setUp());
    CHECK(pifDutyMotor_Init(&s_duty, PIF_ID_AUTO, &s_manager, 100));
    s_duty.act_set_duty = _actSetDuty;
    // No act_operate_brake: the operating time ending outside a run must not call it.
    CHECK(pifDutyMotor_SetOperatingTime(&s_duty, 2));
    _tick(5);
    CHECK(s_duty._state == MS_IDLE);
}

static void testDutyErrorOnce(void)
{
    CHECK(_setUp());
    CHECK(pifDutyMotor_Init(&s_duty, PIF_ID_AUTO, &s_manager, 100));
    s_duty.act_set_duty = _actSetDuty;
    s_duty.evt_error = _evtDutyError;
    s_duty._p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 1000000, _doNothing, NULL, FALSE);
    CHECK(s_duty._p_task);
    CHECK(pifDutyMotor_StartControl(&s_duty));

    pifDutyMotor_SetState(&s_duty, MS_CONST);
    s_duty._error = ME_STABLE_TIMEOUT;
    pifDutyMotor_Control(&s_duty);
    pifDutyMotor_Control(&s_duty);
    CHECK(s_duty._state == MS_BRAKE);
    CHECK(s_last_duty == 0);
    CHECK(s_error_count == 1);

    pifTaskManager_Remove(s_duty._p_task);
}


typedef struct StTestCase
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "step init checks", testStepInitChecks },
    { "step low pps", testStepLowPps },
#ifdef PIF_USE_STEP_MOTOR_STEP_DIR
    { "step STEP/DIR pulses", testStepDirPulses },
#endif
    { "step pulse unit", testStepPulseUnit },
    { "step gear ratio keeps speed", testStepGearRatioKeepsSpeed },
    { "step error once", testStepErrorOnce },
    { "step operating time cancelled", testStepOperatingTimeCancelled },
#ifdef PIF_USE_STEP_MOTOR_5PHASE
    { "step 5-phase pentagon", testStep5PhasePentagon },
    { "step 5-phase pentagon half step", testStep5PhasePentagonHalf },
#endif
#ifdef PIF_USE_STEP_MOTOR_STEP_DIR
    { "step microstep", testStepMicrostep },
#endif
#if !defined(PIF_USE_STEP_MOTOR_5PHASE) || !defined(PIF_USE_STEP_MOTOR_STEP_DIR)
    { "step left out operations", testStepLeftOutOperations },
#endif
    { "duty start", testDutyStart },
    { "duty without brake callback", testDutyNoBrakeCallback },
    { "duty error once", testDutyErrorOnce }
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 2)) {
        printf("pif_motor: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_motor\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
