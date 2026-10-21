// SPDX-License-Identifier: BSD-3-Clause
// PifDutyMotorSpeedEnc on a simulated motor whose encoder period shortens as the duty grows.
#include "motor/pif_duty_motor_speed_enc.h"

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

#define STEP_US			100
#define TICK_US			1000		// Tick of the timer manager
#define MAX_DUTY		1000
#define CONTROL_MS		5
#define PLANT_GAIN		5000000UL	// Encoder period in us is PLANT_GAIN / duty: 10ms at a duty of 500
#define TARGET_US		10000

static BOOL s_failed;
static uint32_t s_now_us;
static uint32_t s_tick_us;
static PifTimerManager s_manager;
static PifPulse s_encoder;
static PifDutyMotorSpeedEnc s_motor;
static PifDutyMotorSpeedEncStage s_stage;

static uint16_t s_duty;				// Given to act_set_duty last
static int s_duty_count;
static int s_stable_count;
static BOOL s_stalled;				// The simulated motor gives no pulses
static uint32_t s_next_edge_us;		// Next falling edge of the encoder, 0 while there is none


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static void _actSetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    (void)p_owner;
    s_duty = duty;
    s_duty_count++;
}

static void _evtStable(PifDutyMotor* p_owner)
{
    (void)p_owner;
    s_stable_count++;
}

// Moves the time on, with the encoder edges of the simulated motor, the ticks and the main loop.
static void _run(uint32_t ms)
{
    uint32_t steps = ms * 1000 / STEP_US;

    while (steps--) {
        s_now_us += STEP_US;
        if (!s_duty || s_stalled) {
            s_next_edge_us = 0;
        }
        else if (!s_next_edge_us) {
            s_next_edge_us = s_now_us + PLANT_GAIN / s_duty;
        }
        else if (s_now_us >= s_next_edge_us) {
            pifPulse_sigEdge(&s_encoder, PS_FALLING_EDGE, s_now_us);
            s_next_edge_us += PLANT_GAIN / s_duty;
        }
        s_tick_us += STEP_US;
        if (s_tick_us >= TICK_US) {
            s_tick_us = 0;
            pif_cumulative_timer1ms++;
            pifTimerManager_sigTick(&s_manager);
        }
        pifTaskManager_Loop();
    }
}

static BOOL _setUp(void)
{
    // A failed test returns early, so free what it left behind.
    pifDutyMotorSpeedEnc_Clear(&s_motor);
    pifTimerManager_Clear(&s_manager);
    pifPulse_Clear(&s_encoder);
    s_duty = 0;
    s_duty_count = 0;
    s_stable_count = 0;
    s_stalled = FALSE;
    s_next_edge_us = 0;

    if (!pifTimerManager_Init(&s_manager, PIF_ID_AUTO, TICK_US, 4)) return FALSE;
    if (!pifPulse_Init(&s_encoder, PIF_ID_AUTO)) return FALSE;
    if (!pifDutyMotorSpeedEnc_Init(&s_motor, PIF_ID_AUTO, &s_manager, MAX_DUTY, CONTROL_MS, &s_encoder)) return FALSE;
    s_motor.parent.act_set_duty = _actSetDuty;		// No act_set_direction: the motor turns one way
    s_motor.parent.evt_stable = _evtStable;
    pifPidControl_Init(pifDutyMotorSpeedEnc_GetPidControl(&s_motor), 0.2f, 0.0f, 0.0f, 0.0f);

    // Period control with no timeouts
    memset(&s_stage, 0, sizeof(s_stage));
    s_stage.mode = MM_SC_YES;
    s_stage.gs_arrive_ratio = 90;
    s_stage.gs_start_duty = 300;
    s_stage.gs_ctrl_duty = 10;
    s_stage.fs_period_us = TARGET_US;
    s_stage.fs_high_duty = 900;
    s_stage.fs_stable_err_low = 95;
    s_stage.fs_stable_err_high = 105;
    return pifDutyMotorSpeedEnc_AddStages(&s_motor, 1, &s_stage);
}

static void testPeriodReachesTarget(void)
{
    uint32_t period;

    CHECK(_setUp());
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    CHECK(s_motor.parent._state == MS_GAINED && s_duty == 300);

    _run(3000);
    CHECK(s_motor.parent._state == MS_CONST && s_stable_count == 1);
    CHECK(s_duty >= 450 && s_duty <= 550);
    period = pifPulse_GetAveragePeriod(&s_encoder);
    CHECK(period >= TARGET_US * 95 / 100 && period <= TARGET_US * 105 / 100);

    pifDutyMotorSpeedEnc_Stop(&s_motor);
    _run(20);
    CHECK(s_motor.parent._state == MS_IDLE && s_duty == 0);		// MS_STOP passes on to MS_IDLE
}

static void testPeriodWaitsForPulse(void)
{
    uint16_t duty;
    int count;

    CHECK(_setUp());
    s_stage.gs_ctrl_duty = 0;			// Straight to the constant speed, at fs_high_duty
    s_stage.fs_high_duty = 400;
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    CHECK(s_motor.parent._state == MS_CONST && s_duty == 400);

    // Until two pulses give a period there is no speed, and the duty stays.
    _run(10);
    CHECK(s_duty_count == 1 && s_duty == 400);

    // At a duty of 400 the period is 12.5ms: slower than the target, but the duty is at its limit already.
    _run(200);
    CHECK(s_duty == 400);

    // Without a timeout, a motor that stops giving pulses keeps the speed it had, and its duty.
    s_stage.fs_high_duty = 900;
    pifDutyMotorSpeedEnc_Stop(&s_motor);
    _run(20);
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    _run(500);
    duty = s_duty;
    count = s_duty_count;
    s_stalled = TRUE;
    _run(100);
    CHECK(s_duty_count == count && s_duty == duty);
}

static void testPeriodStallRaisesDuty(void)
{
    uint16_t duty;

    CHECK(_setUp());
    pifPulse_SetTimeout(&s_encoder, 3 * TARGET_US);
    s_stage.gs_ctrl_duty = 0;
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    _run(1000);
    duty = s_duty;
    CHECK(duty < 600);

    // The last pulse came less than one period before, so the timeout has not passed yet.
    s_stalled = TRUE;
    _run(15);
    CHECK(s_duty == duty);

    // After it the speed reads 0 every control period, and the duty goes up.
    _run(60);
    CHECK(s_duty > duty + 100);
}

static void testDeadBandAndLowDuty(void)
{
    uint16_t duty;
    int count;

    CHECK(_setUp());
    s_stage.gs_ctrl_duty = 0;			// Straight to the constant speed, at fs_high_duty
    s_stage.fs_high_duty = 520;			// 9.6ms: 4% faster than the target
    s_stage.fs_dead_band = 50;
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));

    // Inside the dead band the duty stays.
    _run(500);
    CHECK(s_duty_count == 1 && s_duty == 520);

    // Outside it the PID lowers the duty, down to fs_low_duty at most.
    pifDutyMotorSpeedEnc_Stop(&s_motor);
    _run(20);
    s_stage.fs_dead_band = 0;
    s_stage.fs_high_duty = 900;			// 5.6ms: much too fast
    s_stage.fs_low_duty = 700;
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    _run(500);
    CHECK(s_duty == 700);
    duty = s_duty;
    count = s_duty_count;
    _run(100);
    CHECK(s_duty == duty && s_duty_count == count);
}

static void testCountControl(void)
{
    CHECK(_setUp());
    s_stage.fs_period_us = 0;
    s_stage.fs_pulses_per_period = 2.0f;
    s_stage.gs_ctrl_duty = 20;
    CHECK(pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0));
    CHECK(s_motor.parent._state == MS_GAINED);

    // The first pulse comes after 16.7ms at a duty of 300, so the duty climbs by gs_ctrl_duty each period.
    _run(CONTROL_MS * 3);
    CHECK(s_duty == 300 + 3 * 20);
}

static void testStartChecks(void)
{
    CHECK(_setUp());
    s_motor.parent.act_set_duty = NULL;
    CHECK(!pifDutyMotorSpeedEnc_Start(&s_motor, 0, 0) && pif_error == E_INVALID_PARAM);
}


int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "period control reaches the target", testPeriodReachesTarget },
        { "period control waits for a pulse", testPeriodWaitsForPulse },
        { "period control raises the duty of a stalled motor", testPeriodStallRaisesDuty },
        { "dead band and lowest duty", testDeadBandAndLowDuty },
        { "count control", testCountControl },
        { "start checks", testStartChecks },
    };

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 2)) {
        printf("pif_duty_motor_speed_enc: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_duty_motor_speed_enc\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifDutyMotorSpeedEnc_Clear(&s_motor);
    pifTimerManager_Clear(&s_manager);
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
