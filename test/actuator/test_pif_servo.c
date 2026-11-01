// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/pif_servo.h"

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

// Steps of the simulated clock between two passes of the task manager.
#define SIM_STEP_US		100
#define PERIOD_MS		20
#define MAX_CHANNELS	12

static BOOL s_failed;
static uint32_t s_now;
static uint16_t s_pulse[MAX_CHANNELS];
static int s_act_count;
static int s_arrive_count;
static uint32_t s_arrive_mask;
static uint32_t s_arrive_ms[MAX_CHANNELS];
static int s_lock_depth;
static int s_lock_count;
static BOOL s_lock_error;
static int s_act_unlocked;
static BOOL s_check_lock;
// evt_arrive moves channel 0 here once, when set.
static int16_t s_chain_angle;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _actControl(uint8_t index, uint16_t pulse_us)
{
    if (s_check_lock && !s_lock_depth) s_act_unlocked++;
    if (index < MAX_CHANNELS) s_pulse[index] = pulse_us;
    s_act_count++;
}

// Counts the sections and lets them nest, as the chain test needs.
static void _actLock(PifServo *p_owner, BOOL lock)
{
    (void)p_owner;
    if (lock) {
        s_lock_depth++;
        s_lock_count++;
    }
    else {
        if (!s_lock_depth) s_lock_error = TRUE;
        else s_lock_depth--;
    }
}

static void _evtArrive(PifServo *p_owner, uint8_t index)
{
    s_arrive_count++;
    s_arrive_mask |= 1UL << index;
    if (index < MAX_CHANNELS) s_arrive_ms[index] = s_now / 1000;
    if (s_chain_angle && index == 0) {
        int16_t angle = s_chain_angle;

        s_chain_angle = 0;
        pifServo_MoveTo(p_owner, 0, angle, 200);
    }
}

// Runs the task manager until the clock reaches the time, the way a main loop would.
static void _runUntil(uint32_t time_ms)
{
    while (s_now < time_ms * 1000) {
        s_now += SIM_STEP_US;
        pifTaskManager_Loop();
    }
}

static void _setUpN(PifServo *p_servo, uint8_t count)
{
    pifTaskManager_Clear();
    s_now = 0;
    memset(s_pulse, 0, sizeof(s_pulse));
    memset(s_arrive_ms, 0, sizeof(s_arrive_ms));
    s_act_count = 0;
    s_arrive_count = 0;
    s_arrive_mask = 0;
    s_chain_angle = 0;
    pif_error = E_SUCCESS;

    if (!pifTaskManager_Init(2, 0) || !pifServo_Init(p_servo, PIF_ID_AUTO, count, PERIOD_MS, _actControl)) {
        s_failed = TRUE;
        return;
    }
    p_servo->evt_arrive = _evtArrive;
}

static void testInit(void)
{
    PifServo servo;

    _setUpN(&servo, 2);
    CHECK(!s_failed);
    CHECK(pifTaskManager_Count() == 1 && servo._p_task && servo._period == PERIOD_MS * 1000UL);
    CHECK(!pifServo_Init(NULL, PIF_ID_AUTO, 1, PERIOD_MS, _actControl) && pif_error == E_INVALID_PARAM);
    CHECK(!pifServo_Init(&servo, PIF_ID_AUTO, 0, PERIOD_MS, _actControl));
    CHECK(!pifServo_Init(&servo, PIF_ID_AUTO, 1, 0, _actControl));
    CHECK(!pifServo_Init(&servo, PIF_ID_AUTO, 1, PERIOD_MS, NULL));

    // No pulse at power up, and the task sleeps.
    _setUpN(&servo, 2);
    _runUntil(100);
    CHECK(s_act_count == 0 && !pifServo_IsAttached(&servo, 0) && !pifServo_IsMoving(&servo, 0));
    CHECK(pifServo_GetChannel(&servo, 1)->_min_pulse == PIF_SERVO_DEFAULT_MIN_PULSE);
    CHECK(pifServo_GetChannel(&servo, 1)->_max_angle == PIF_SERVO_DEFAULT_MAX_ANGLE);
    CHECK(!pifServo_GetChannel(&servo, 2) && pif_error == E_INVALID_PARAM);

    // An index out of range.
    CHECK(!pifServo_SetAngle(&servo, 2, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifServo_MoveTo(&servo, 2, 0, 100));
    CHECK(!pifServo_IsAttached(&servo, 2) && pifServo_GetAngle(&servo, 2) == 0);
    pifServo_Clear(&servo);
}

static void testAngleToPulse(void)
{
    PifServo servo;

    _setUpN(&servo, 1);
    CHECK(!s_failed);

    CHECK(pifServo_SetAngle(&servo, 0, 0) && s_pulse[0] == 1000 && pifServo_IsAttached(&servo, 0));
    CHECK(pifServo_SetAngle(&servo, 0, 900) && s_pulse[0] == 1500);
    CHECK(pifServo_SetAngle(&servo, 0, 1800) && s_pulse[0] == 2000);
    CHECK(pifServo_SetAngle(&servo, 0, 450) && s_pulse[0] == 1250 && pifServo_GetAngle(&servo, 0) == 450);

    // Held within the range.
    CHECK(pifServo_SetAngle(&servo, 0, -100) && s_pulse[0] == 1000 && pifServo_GetAngle(&servo, 0) == 0);
    CHECK(pifServo_SetAngle(&servo, 0, 3000) && s_pulse[0] == 2000 && pifServo_GetAngle(&servo, 0) == 1800);

    // The same pulse is not given again.
    s_act_count = 0;
    CHECK(pifServo_SetAngle(&servo, 0, 1800) && s_act_count == 0);

    // A wider range with negative angles: 500 us at -90 degrees, 2500 us at +90.
    CHECK(pifServo_SetRange(&servo, 0, 500, 2500, -900, 900));
    CHECK(s_pulse[0] == 2500 && pifServo_GetAngle(&servo, 0) == 900);
    CHECK(pifServo_SetAngle(&servo, 0, 0) && s_pulse[0] == 1500);
    CHECK(pifServo_SetAngle(&servo, 0, -450) && s_pulse[0] == 1000);

    // The trim shifts every pulse, and is applied at once.
    CHECK(pifServo_SetTrim(&servo, 0, -30) && s_pulse[0] == 970);
    CHECK(pifServo_SetAngle(&servo, 0, 0) && s_pulse[0] == 1470);

    // The reverse swaps the ends.
    CHECK(pifServo_SetReverse(&servo, 0, TRUE) && s_pulse[0] == 1470);
    CHECK(pifServo_SetAngle(&servo, 0, -900) && s_pulse[0] == 2470);
    CHECK(pifServo_SetAngle(&servo, 0, 900) && s_pulse[0] == 470);

    // Invalid ranges and trims.
    CHECK(!pifServo_SetRange(&servo, 0, 2000, 1000, 0, 1800) && pif_error == E_INVALID_PARAM);
    CHECK(!pifServo_SetRange(&servo, 0, 1000, 2000, 900, 900));
    CHECK(!pifServo_SetRange(&servo, 0, 30, 2000, 0, 1800));
    CHECK(!pifServo_SetRange(&servo, 0, 1000, 2000, PIF_SERVO_KEEP, 1800));
    CHECK(!pifServo_SetTrim(&servo, 0, 500));
    CHECK(!pifServo_SetTrim(&servo, 0, INT16_MIN));
    pifServo_Clear(&servo);
}

static void testSetPulse(void)
{
    PifServo servo;

    _setUpN(&servo, 1);
    CHECK(!s_failed);

    // The pulse goes out as it is, and the angle follows from it.
    CHECK(pifServo_SetPulse(&servo, 0, 1600) && s_pulse[0] == 1600 && pifServo_GetAngle(&servo, 0) == 1080);
    CHECK(pifServo_SetPulse(&servo, 0, 2400) && s_pulse[0] == 2400 && pifServo_GetAngle(&servo, 0) == 1800);
    CHECK(!pifServo_SetPulse(&servo, 0, 0) && pif_error == E_INVALID_PARAM && s_pulse[0] == 2400);

    // The inverse takes the trim and the reverse into account.
    CHECK(pifServo_SetTrim(&servo, 0, 100));
    CHECK(pifServo_SetReverse(&servo, 0, TRUE));
    CHECK(pifServo_SetPulse(&servo, 0, 1350) && pifServo_GetAngle(&servo, 0) == 1350);

    // A move starts from there.
    CHECK(pifServo_MoveTo(&servo, 0, 1350, 0));
    _runUntil(5);
    CHECK(s_pulse[0] == 1350);
    pifServo_Clear(&servo);
}

static void testMoveLinear(void)
{
    PifServo servo;
    int16_t angle;
    int count;

    _setUpN(&servo, 1);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 0, 0));

    CHECK(pifServo_MoveTo(&servo, 0, 1800, 1000));
    CHECK(pifServo_IsMoving(&servo, 0));

    // About half way after half the time.
    _runUntil(510);
    angle = pifServo_GetAngle(&servo, 0);
    CHECK(angle >= 840 && angle <= 930);
    CHECK(s_pulse[0] > 1450 && s_pulse[0] < 1530);

    _runUntil(990);
    CHECK(pifServo_IsMoving(&servo, 0) && s_arrive_count == 0);

    // At the target within an update of the time.
    _runUntil(1100);
    CHECK(!pifServo_IsMoving(&servo, 0) && s_pulse[0] == 2000 && pifServo_GetAngle(&servo, 0) == 1800);
    CHECK(s_arrive_count == 1 && s_arrive_mask == 1);
    CHECK(s_arrive_ms[0] >= 1000 && s_arrive_ms[0] <= 1000 + 2 * PERIOD_MS);

    // The task sleeps after it.
    count = s_act_count;
    _runUntil(2000);
    CHECK(s_act_count == count && s_arrive_count == 1);

    // Back down.
    CHECK(pifServo_MoveTo(&servo, 0, 0, 400));
    _runUntil(2500);
    CHECK(s_pulse[0] == 1000 && s_arrive_count == 2);
    pifServo_Clear(&servo);
}

static void testMoveWhileDetached(void)
{
    PifServo servo;

    _setUpN(&servo, 1);
    CHECK(!s_failed);

    // The position is not known, so it jumps at once and arrives at the next update.
    CHECK(pifServo_MoveTo(&servo, 0, 900, 1000));
    CHECK(s_pulse[0] == 1500 && pifServo_IsAttached(&servo, 0));
    _runUntil(1);
    CHECK(s_arrive_count == 1 && !pifServo_IsMoving(&servo, 0));

    // A time of 0 jumps at the next update.
    CHECK(pifServo_MoveTo(&servo, 0, 1800, 0));
    CHECK(s_pulse[0] == 1500);
    _runUntil(2);
    CHECK(s_pulse[0] == 2000 && s_arrive_count == 2);
    pifServo_Clear(&servo);
}

static void testMoveEase(void)
{
    PifServo servo;
    static const int16_t target[2] = { 1800, 1800 };
    int16_t linear, ease;

    _setUpN(&servo, 2);
    CHECK(!s_failed);
    CHECK(pifServo_SetProfile(&servo, 1, SVP_EASE));
    CHECK(!pifServo_SetProfile(&servo, 1, (PifServoProfile)5));
    CHECK(pifServo_SetAngle(&servo, 0, 0) && pifServo_SetAngle(&servo, 1, 0));
    CHECK(pifServo_MoveSync(&servo, target, 1000));

    // Slower at the start, as fast in the middle, and ahead toward the end.
    _runUntil(250);
    linear = pifServo_GetAngle(&servo, 0);
    ease = pifServo_GetAngle(&servo, 1);
    CHECK(ease < linear - 100);
    _runUntil(500);
    linear = pifServo_GetAngle(&servo, 0);
    ease = pifServo_GetAngle(&servo, 1);
    // The last update is up to a period before the middle, where the ease is 1.5 times as fast.
    CHECK(ease > linear - 40 && ease < linear + 40);
    _runUntil(750);
    linear = pifServo_GetAngle(&servo, 0);
    ease = pifServo_GetAngle(&servo, 1);
    CHECK(ease > linear + 100);

    // A change of profile does not touch the move under way.
    CHECK(pifServo_SetProfile(&servo, 1, SVP_LINEAR) && pifServo_IsMoving(&servo, 1));
    CHECK(pifServo_GetAngle(&servo, 1) == ease);

    _runUntil(1100);
    CHECK(s_pulse[0] == 2000 && s_pulse[1] == 2000 && s_arrive_mask == 3);
    pifServo_Clear(&servo);
}

static void testMoveAtSpeed(void)
{
    PifServo servo;

    _setUpN(&servo, 1);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 0, 0));

    // 90 degrees at 90 degrees per second take a second.
    CHECK(pifServo_MoveAtSpeed(&servo, 0, 900, 90));
    CHECK(servo.__p_channel[0].__move_time == 1000);
    _runUntil(1100);
    CHECK(s_pulse[0] == 1500 && s_arrive_count == 1);

    // Rounded up: 0.1 degree at 7 degrees per second.
    CHECK(pifServo_MoveAtSpeed(&servo, 0, 901, 7));
    CHECK(servo.__p_channel[0].__move_time == 15);

    // Speed 0 and a move longer than 65535 ms are refused.
    CHECK(!pifServo_MoveAtSpeed(&servo, 0, 0, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifServo_SetAngle(&servo, 0, 0));
    CHECK(!pifServo_MoveAtSpeed(&servo, 0, 1800, 1) && pif_error == E_INVALID_PARAM);
    CHECK(!pifServo_IsMoving(&servo, 0));
    pifServo_Clear(&servo);
}

static void testMoveSync(void)
{
    PifServo servo;
    static const int16_t target[3] = { 1800, PIF_SERVO_KEEP, 0 };

    _setUpN(&servo, 3);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 0, 0) && pifServo_SetAngle(&servo, 1, 900) && pifServo_SetAngle(&servo, 2, 1800));
    CHECK(!pifServo_MoveSync(&servo, NULL, 100) && pif_error == E_INVALID_PARAM);

    CHECK(pifServo_MoveSync(&servo, target, 600));
    CHECK(pifServo_IsMoving(&servo, 0) && !pifServo_IsMoving(&servo, 1) && pifServo_IsMoving(&servo, 2));

    // Mirror images of each other all the way.
    _runUntil(300);
    CHECK(pifServo_GetAngle(&servo, 0) + pifServo_GetAngle(&servo, 2) == 1800);

    _runUntil(700);
    CHECK(s_pulse[0] == 2000 && s_pulse[1] == 1500 && s_pulse[2] == 1000);
    CHECK(s_arrive_mask == 5 && s_arrive_ms[0] == s_arrive_ms[2]);
    pifServo_Clear(&servo);
}

static void testReplaceStopDetach(void)
{
    PifServo servo;
    int16_t angle;
    int count;

    _setUpN(&servo, 1);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 0, 0));

    // A new move starts from where the old one got to, without a jump: from the last update on, which is a
    // period, about 36 at this speed, behind at most.
    CHECK(pifServo_MoveTo(&servo, 0, 1800, 1000));
    _runUntil(500);
    angle = pifServo_GetAngle(&servo, 0);
    CHECK(pifServo_MoveTo(&servo, 0, 0, 500));
    CHECK(servo.__p_channel[0].__start_angle >= angle && servo.__p_channel[0].__start_angle <= angle + 40);
    _runUntil(1100);
    CHECK(s_pulse[0] == 1000 && s_arrive_count == 1);

    // Stop holds where it got to, without evt_arrive.
    CHECK(pifServo_MoveTo(&servo, 0, 1800, 1000));
    _runUntil(1600);
    CHECK(pifServo_Stop(&servo, 0) && !pifServo_IsMoving(&servo, 0));
    angle = pifServo_GetAngle(&servo, 0);
    CHECK(angle > 700 && angle < 1100);
    count = s_act_count;
    _runUntil(2500);
    CHECK(s_act_count == count && s_arrive_count == 1 && pifServo_GetAngle(&servo, 0) == angle);

    // Detach stops the pulse and the move.
    CHECK(pifServo_MoveTo(&servo, 0, 0, 1000));
    _runUntil(2700);
    CHECK(pifServo_Detach(&servo, 0) && s_pulse[0] == 0 && !pifServo_IsAttached(&servo, 0));
    CHECK(!pifServo_IsMoving(&servo, 0));
    _runUntil(4000);
    CHECK(s_pulse[0] == 0 && s_arrive_count == 1);
    pifServo_Clear(&servo);
}

static void testChain(void)
{
    PifServo servo;

    _setUpN(&servo, 1);
    CHECK(!s_failed);
    pifServo_AttachActLock(&servo, _actLock);
    CHECK(pifServo_SetAngle(&servo, 0, 0));

    // evt_arrive starts the next move, which goes on by itself.
    s_chain_angle = 900;
    CHECK(pifServo_MoveTo(&servo, 0, 1800, 200));
    _runUntil(300);
    CHECK(s_arrive_count == 1 && pifServo_IsMoving(&servo, 0));
    _runUntil(600);
    CHECK(s_arrive_count == 2 && s_pulse[0] == 1500 && !pifServo_IsMoving(&servo, 0));
    pifServo_Clear(&servo);
}

static void testLock(void)
{
    PifServo servo;
    static const int16_t target[2] = { 900, 900 };

    _setUpN(&servo, 2);
    CHECK(!s_failed);
    s_lock_depth = 0;
    s_lock_count = 0;
    s_lock_error = FALSE;
    s_act_unlocked = 0;
    s_check_lock = TRUE;
    pifServo_AttachActLock(&servo, _actLock);

    // Every call and the task work under the lock, and leave it.
    CHECK(pifServo_SetAngle(&servo, 0, 0) && pifServo_SetPulse(&servo, 1, 1200));
    CHECK(pifServo_MoveSync(&servo, target, 100) && pifServo_MoveAtSpeed(&servo, 1, 1800, 360));
    _runUntil(50);
    CHECK(pifServo_Stop(&servo, 0) && pifServo_SetTrim(&servo, 1, 10) && pifServo_SetReverse(&servo, 1, TRUE));
    CHECK(pifServo_SetRange(&servo, 1, 600, 2400, 0, 1800) && pifServo_MoveTo(&servo, 0, 1800, 100));
    _runUntil(500);
    CHECK(pifServo_Detach(&servo, 1));
    s_check_lock = FALSE;
    CHECK(s_lock_count > 10 && s_lock_depth == 0 && !s_lock_error && s_act_unlocked == 0);

    // Without a lock no locking is done.
    pifServo_AttachActLock(&servo, NULL);
    s_lock_count = 0;
    CHECK(pifServo_SetAngle(&servo, 0, 0) && s_lock_count == 0);
    pifServo_Clear(&servo);
}

static void testClear(void)
{
    PifServo servo;

    _setUpN(&servo, 2);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 0, 900) && pifServo_MoveTo(&servo, 1, 900, 1000));

    // Nothing would hold the servos once the task is gone, so they are detached.
    pifServo_Clear(&servo);
    CHECK(s_pulse[0] == 0 && s_pulse[1] == 0 && servo._count == 0 && !servo._p_task);
    CHECK(pifTaskManager_Count() == 0);
}

#ifdef COLLECT_SIGNAL_SERVO

static void testCollectSignal(void)
{
    PifServo servo;
    PifServoChannel *p_ch;

    _setUpN(&servo, 12);
    CHECK(!s_failed);
    CHECK(pifServo_SetAngle(&servo, 2, 900));
    CHECK(pifServo_SetCsFlag(&servo, PIF_CS_INDEX_ALL, SV_CSF_ALL_BIT));
    p_ch = servo.__p_channel;

    CHECK(strcmp(p_ch[0].__cs[SV_CSF_PULSE_IDX]._name, "SVP0") == 0);
    CHECK(strcmp(p_ch[11].__cs[SV_CSF_PULSE_IDX]._name, "SVP11") == 0);
    CHECK(p_ch[1].__cs[SV_CSF_PULSE_IDX]._id == servo._id && p_ch[1].__cs[SV_CSF_PULSE_IDX]._width == 16);

    // Added with the pulse it has, and a change shows on its own channel only.
    CHECK(p_ch[2].__cs[SV_CSF_PULSE_IDX]._value == 1500);
    CHECK(pifServo_SetAngle(&servo, 1, 1800));
    CHECK(p_ch[1].__cs[SV_CSF_PULSE_IDX]._value == 2000 && p_ch[0].__cs[SV_CSF_PULSE_IDX]._value == 0);
    CHECK(pifServo_Detach(&servo, 1) && p_ch[1].__cs[SV_CSF_PULSE_IDX]._value == 0);

    pifServo_ResetCsFlag(&servo, 3, SV_CSF_ALL_BIT);
    CHECK(pifServo_SetCsFlag(&servo, 3, SV_CSF_PULSE_BIT));
    pifServo_Clear(&servo);
}

#endif

static BOOL _portSetup(void)
{
    return TRUE;
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "init", testInit },
        { "angle to pulse", testAngleToPulse },
        { "set pulse", testSetPulse },
        { "move linear", testMoveLinear },
        { "move while detached", testMoveWhileDetached },
        { "move ease", testMoveEase },
        { "move at speed", testMoveAtSpeed },
        { "move sync", testMoveSync },
        { "replace, stop, detach", testReplaceStopDetach },
        { "chain", testChain },
        { "lock", testLock },
        { "clear", testClear },
#ifdef COLLECT_SIGNAL_SERVO
        { "collect signal", testCollectSignal },
#endif
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_servo: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_servo\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifTaskManager_Clear();
    return failed ? 1 : 0;
}
