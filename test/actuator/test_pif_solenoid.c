// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/pif_solenoid.h"

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
#define MAX_ACTS		16

typedef struct {
    uint8_t index;
    uint32_t time_ms;
    SWITCH action;
    PifSolenoidDir dir;
} Act;

static BOOL s_failed;
static uint32_t s_now;
static Act s_act[MAX_ACTS];
static int s_act_count;
static int s_off_count;
static uint32_t s_off_mask;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _actControl(uint8_t index, SWITCH action, PifSolenoidDir dir)
{
    if (s_act_count < MAX_ACTS) {
        s_act[s_act_count].index = index;
        s_act[s_act_count].time_ms = s_now / 1000;
        s_act[s_act_count].action = action;
        s_act[s_act_count].dir = dir;
    }
    s_act_count++;
}

static void _evtOff(PifSolenoid* p_owner, uint8_t index)
{
    (void)p_owner;
    s_off_count++;
    s_off_mask |= 1U << index;
}

static BOOL _actOf(int index, uint8_t channel, uint32_t time_ms, SWITCH action, PifSolenoidDir dir)
{
    return index < s_act_count && s_act[index].index == channel && s_act[index].time_ms == time_ms &&
            s_act[index].action == action && s_act[index].dir == dir;
}

// The single channel tests run on channel 0.
static BOOL _act(int index, uint32_t time_ms, SWITCH action, PifSolenoidDir dir)
{
    return _actOf(index, 0, time_ms, action, dir);
}

// Runs the task manager until the clock reaches the time, the way a main loop would.
static void _runUntil(uint32_t time_ms)
{
    while (s_now < time_ms * 1000) {
        s_now += SIM_STEP_US;
        pifTaskManager_Loop();
    }
}

static void _setUpN(PifSolenoid* p_solenoid, uint8_t count, PifSolenoidType type, uint16_t on_time)
{
    pifTaskManager_Clear();
    s_now = 0;
    s_act_count = 0;
    s_off_count = 0;
    s_off_mask = 0;
    pif_error = E_SUCCESS;

    if (!pifTaskManager_Init(2, 0) || !pifSolenoid_Init(p_solenoid, PIF_ID_AUTO, count, type, on_time, _actControl)) {
        s_failed = TRUE;
        return;
    }
    p_solenoid->evt_off = _evtOff;
}

static void _setUp(PifSolenoid* p_solenoid, PifSolenoidType type, uint16_t on_time)
{
    _setUpN(p_solenoid, 1, type, on_time);
}

static void testInit(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 100);
    CHECK(!s_failed);
    CHECK(pifTaskManager_Count() == 1 && solenoid._p_task);
    CHECK(!pifSolenoid_Init(NULL, PIF_ID_AUTO, 1, ST_1POINT, 100, _actControl) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSolenoid_Init(&solenoid, PIF_ID_AUTO, 1, ST_1POINT, 100, NULL));

    // No room for the task.
    pifSolenoid_Clear(&solenoid);
    {
        PifSolenoid more[2];
        CHECK(pifSolenoid_Init(&more[0], PIF_ID_AUTO, 1, ST_1POINT, 100, _actControl));
        CHECK(pifSolenoid_Init(&more[1], PIF_ID_AUTO, 1, ST_1POINT, 100, _actControl));
        CHECK(!pifSolenoid_Init(&solenoid, PIF_ID_AUTO, 1, ST_1POINT, 100, _actControl));
        pifSolenoid_Clear(&more[0]);
        pifSolenoid_Clear(&more[1]);
    }
    CHECK(pifTaskManager_Count() == 0);
}

static void testPulse(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 500);
    CHECK(!s_failed);

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    CHECK(s_act_count == 1 && _act(0, 0, ON, SD_INVALID));
    _runUntil(499);
    CHECK(s_act_count == 1 && s_off_count == 0);
    _runUntil(500);
    CHECK(s_act_count == 2 && _act(1, 500, OFF, SD_INVALID) && s_off_count == 1);

    // Nothing more once it is off.
    _runUntil(2000);
    CHECK(s_act_count == 2 && s_off_count == 1);
    pifSolenoid_Clear(&solenoid);
}

static void testRestartExtendsPulse(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 500);
    CHECK(!s_failed);

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    _runUntil(300);
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    _runUntil(799);
    CHECK(s_act_count == 2 && s_off_count == 0);
    _runUntil(800);
    CHECK(s_act_count == 3 && _act(2, 800, OFF, SD_INVALID) && s_off_count == 1);
    pifSolenoid_Clear(&solenoid);
}

static void testNoOnTime(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 0);
    CHECK(!s_failed);

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    _runUntil(5000);
    CHECK(s_act_count == 1 && s_off_count == 0);
    pifSolenoid_ActionOff(&solenoid, 0);
    CHECK(s_act_count == 2 && _act(1, 5000, OFF, SD_INVALID) && s_off_count == 0);
    pifSolenoid_Clear(&solenoid);
}

static void testDelay(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 50);
    CHECK(!s_failed);

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(s_act_count == 0);
    _runUntil(99);
    CHECK(s_act_count == 0);
    _runUntil(100);
    CHECK(s_act_count == 1 && _act(0, 100, ON, SD_INVALID));
    _runUntil(150);
    CHECK(s_act_count == 2 && _act(1, 150, OFF, SD_INVALID) && s_off_count == 1);
    pifSolenoid_Clear(&solenoid);
}

static void testBuffer(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 20);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetBuffer(&solenoid, 0, 4));

    // All three are counted from now: at 100, at 100 as well, and at 300.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 300));
    _runUntil(1000);
    CHECK(s_act_count == 5);
    CHECK(_act(0, 100, ON, SD_INVALID) && _act(1, 100, ON, SD_INVALID) && _act(2, 120, OFF, SD_INVALID));
    CHECK(_act(3, 300, ON, SD_INVALID) && _act(4, 320, OFF, SD_INVALID));
    CHECK(s_off_count == 2);
    pifSolenoid_Clear(&solenoid);
}

static void testBufferAfterZeroDelay(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 20);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetBuffer(&solenoid, 0, 4));

    // A queued command with no delay left does not hold up the one behind it.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 50));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 200));
    _runUntil(1000);
    CHECK(s_act_count == 5);
    CHECK(_act(0, 100, ON, SD_INVALID) && _act(1, 100, ON, SD_INVALID) && _act(2, 120, OFF, SD_INVALID));
    CHECK(_act(3, 200, ON, SD_INVALID) && _act(4, 220, OFF, SD_INVALID));

    // With the queue empty, a new command starts on its own again.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    _runUntil(1100);
    CHECK(s_act_count == 6 && _act(5, 1100, ON, SD_INVALID));
    pifSolenoid_Clear(&solenoid);
}

static void testLateRunKeepsQueueTiming(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 0);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetBuffer(&solenoid, 0, 4));

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 300));

    // The task gets to run only at 250ms, and the second command still comes at 300ms.
    s_now = 250 * 1000;
    pifTaskManager_Loop();
    CHECK(s_act_count == 1 && _act(0, 250, ON, SD_INVALID));
    _runUntil(299);
    CHECK(s_act_count == 1);
    _runUntil(300);
    CHECK(s_act_count == 2 && _act(1, 300, ON, SD_INVALID));
    pifSolenoid_Clear(&solenoid);
}

static void testActionOffCancels(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 500);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetBuffer(&solenoid, 0, 4));

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 200));
    _runUntil(50);
    pifSolenoid_ActionOff(&solenoid, 0);
    CHECK(s_act_count == 2 && _act(1, 50, OFF, SD_INVALID));

    // Neither the delay, the queue nor the end of the pulse does anything after it.
    _runUntil(2000);
    CHECK(s_act_count == 2 && s_off_count == 0);

    // And it works as before afterwards, with no queued command left over to follow it.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 10));
    _runUntil(3000);
    CHECK(s_act_count == 4 && _act(2, 2010, ON, SD_INVALID) && _act(3, 2510, OFF, SD_INVALID));
    pifSolenoid_Clear(&solenoid);
}

static void testTwoPoint(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_2POINT, 30);
    CHECK(!s_failed);

    CHECK(!pifSolenoid_ActionOn(&solenoid, 0, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSolenoid_ActionOnDir(&solenoid, 0, 0, SD_INVALID));
    CHECK(s_act_count == 0);

    CHECK(pifSolenoid_ActionOnDir(&solenoid, 0, 0, SD_LEFT));
    _runUntil(100);
    CHECK(s_act_count == 2 && _act(0, 0, ON, SD_LEFT) && _act(1, 30, OFF, SD_INVALID));

    // Already on the left: skipped.
    CHECK(pifSolenoid_ActionOnDir(&solenoid, 0, 0, SD_LEFT));
    _runUntil(200);
    CHECK(s_act_count == 2);

    CHECK(pifSolenoid_ActionOnDir(&solenoid, 0, 0, SD_RIGHT));
    _runUntil(300);
    CHECK(s_act_count == 4 && _act(2, 200, ON, SD_RIGHT) && _act(3, 230, OFF, SD_INVALID));

    // Unless the direction is forgotten.
    pifSolenoid_SetInvalidDirection(&solenoid, 0);
    CHECK(pifSolenoid_ActionOnDir(&solenoid, 0, 0, SD_RIGHT));
    CHECK(s_act_count == 5 && _act(4, 300, ON, SD_RIGHT));
    pifSolenoid_Clear(&solenoid);
}

static void testClear(void)
{
    PifSolenoid solenoid;

    _setUp(&solenoid, ST_1POINT, 500);
    CHECK(!s_failed);

    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    pifSolenoid_Clear(&solenoid);
    CHECK(s_act_count == 2 && _act(1, 0, OFF, SD_INVALID));
    CHECK(pifTaskManager_Count() == 0 && !solenoid._p_task);
}

static void testCount(void)
{
    PifSolenoid solenoid;

    _setUpN(&solenoid, PIF_SOLENOID_MAX_COUNT, ST_1POINT, 100);
    CHECK(!s_failed);
    CHECK(solenoid._count == PIF_SOLENOID_MAX_COUNT);
    pifSolenoid_Clear(&solenoid);
    CHECK(solenoid._count == 0 && pifTaskManager_Count() == 0);

    CHECK(!pifSolenoid_Init(&solenoid, PIF_ID_AUTO, 0, ST_1POINT, 100, _actControl) && pif_error == E_INVALID_PARAM);
#if PIF_SOLENOID_MAX_COUNT < 255
    CHECK(!pifSolenoid_Init(&solenoid, PIF_ID_AUTO, PIF_SOLENOID_MAX_COUNT + 1, ST_1POINT, 100, _actControl));
#endif

    // An index past the last channel is refused everywhere.
    CHECK(pifSolenoid_Init(&solenoid, PIF_ID_AUTO, 2, ST_1POINT, 100, _actControl));
    pif_error = E_SUCCESS;
    CHECK(!pifSolenoid_ActionOn(&solenoid, 2, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSolenoid_ActionOnDir(&solenoid, 2, 0, SD_LEFT));
    CHECK(!pifSolenoid_ActionOff(&solenoid, 2));
    CHECK(!pifSolenoid_SetBuffer(&solenoid, 2, 4));
    CHECK(!pifSolenoid_SetOnTime(&solenoid, 2, 10));
    CHECK(!pifSolenoid_SetInvalidDirection(&solenoid, 2));
    CHECK(!pifSolenoid_IsOn(&solenoid, 2));
    CHECK(s_act_count == 0);
    pifSolenoid_Clear(&solenoid);
}

static void testChannelsIndependent(void)
{
    PifSolenoid solenoid;

    _setUpN(&solenoid, 3, ST_1POINT, 100);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetOnTime(&solenoid, 1, 30));

    // One task times all three, each by its own deadlines.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 0));
    _runUntil(10);
    CHECK(pifSolenoid_ActionOn(&solenoid, 1, 0));
    CHECK(pifSolenoid_ActionOn(&solenoid, 2, 40));
    CHECK(pifSolenoid_IsOn(&solenoid, 0) && pifSolenoid_IsOn(&solenoid, 1) && !pifSolenoid_IsOn(&solenoid, 2));
    _runUntil(1000);
    CHECK(s_act_count == 6);
    CHECK(_actOf(0, 0, 0, ON, SD_INVALID) && _actOf(1, 1, 10, ON, SD_INVALID));
    CHECK(_actOf(2, 1, 40, OFF, SD_INVALID) && _actOf(3, 2, 50, ON, SD_INVALID));
    CHECK(_actOf(4, 0, 100, OFF, SD_INVALID) && _actOf(5, 2, 150, OFF, SD_INVALID));
    CHECK(s_off_count == 3 && s_off_mask == 7);
    pifSolenoid_Clear(&solenoid);
}

static void testChannelsSeparateQueues(void)
{
    PifSolenoid solenoid;

    _setUpN(&solenoid, 2, ST_1POINT, 0);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetBuffer(&solenoid, 1, 4));

    // Channel 0 has no buffer, so its second delay replaces the first. Channel 1 queues both.
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 0, 200));
    CHECK(pifSolenoid_ActionOn(&solenoid, 1, 100));
    CHECK(pifSolenoid_ActionOn(&solenoid, 1, 200));
    _runUntil(150);
    CHECK(s_act_count == 1 && _actOf(0, 1, 100, ON, SD_INVALID));

    // Switching channel 1 off cancels its queue and leaves channel 0 alone.
    CHECK(pifSolenoid_ActionOff(&solenoid, 1));
    _runUntil(1000);
    CHECK(s_act_count == 3 && _actOf(1, 1, 150, OFF, SD_INVALID) && _actOf(2, 0, 200, ON, SD_INVALID));
    CHECK(pifSolenoid_IsOn(&solenoid, 0) && !pifSolenoid_IsOn(&solenoid, 1));

    // Clear switches off what is still on.
    pifSolenoid_Clear(&solenoid);
    CHECK(s_act_count == 4 && _actOf(3, 0, 1000, OFF, SD_INVALID));
}

#ifdef PIF_COLLECT_SIGNAL

static void testCollectSignal(void)
{
    PifSolenoid solenoid;
    PifSolenoidChannel* p_ch;

    _setUpN(&solenoid, 12, ST_2POINT, 30);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetCsFlag(&solenoid, SN_CSF_ALL_BIT));
    p_ch = solenoid.__p_channel;

    // Every channel has its own pair, named after its index and tagged with the instance.
    CHECK(strcmp(p_ch[0].__cs[SN_CSF_ACTION_IDX]._name, "SNA0") == 0);
    CHECK(strcmp(p_ch[2].__cs[SN_CSF_ACTION_IDX]._name, "SNA2") == 0);
    CHECK(strcmp(p_ch[2].__cs[SN_CSF_DIR_IDX]._name, "SND2") == 0);
    CHECK(strcmp(p_ch[11].__cs[SN_CSF_ACTION_IDX]._name, "SNA11") == 0);
    CHECK(strcmp(p_ch[11].__cs[SN_CSF_DIR_IDX]._name, "SND11") == 0);
    CHECK(p_ch[1].__cs[SN_CSF_ACTION_IDX]._id == solenoid._id && p_ch[1].__cs[SN_CSF_DIR_IDX]._id == solenoid._id);
    CHECK(p_ch[1].__cs[SN_CSF_ACTION_IDX]._width == 1 && p_ch[1].__cs[SN_CSF_DIR_IDX]._width == 2);

    // A change shows on its own channel only.
    CHECK(pifSolenoid_ActionOnDir(&solenoid, 1, 0, SD_RIGHT));
    CHECK(p_ch[1].__cs[SN_CSF_ACTION_IDX]._value == 1 && p_ch[1].__cs[SN_CSF_DIR_IDX]._value == SD_RIGHT);
    CHECK(p_ch[0].__cs[SN_CSF_ACTION_IDX]._value == 0 && p_ch[2].__cs[SN_CSF_ACTION_IDX]._value == 0);
    CHECK(p_ch[0].__cs[SN_CSF_DIR_IDX]._value == SD_INVALID && p_ch[2].__cs[SN_CSF_DIR_IDX]._value == SD_INVALID);

    // The direction is the one driven, so it goes back to SD_INVALID with the pulse.
    _runUntil(30);
    CHECK(p_ch[1].__cs[SN_CSF_ACTION_IDX]._value == 0 && p_ch[1].__cs[SN_CSF_DIR_IDX]._value == SD_INVALID);

    // Removed from every channel, and Clear removes them as well.
    pifSolenoid_ResetCsFlag(&solenoid, SN_CSF_DIR_BIT);
    CHECK(p_ch[0].__cs[SN_CSF_DIR_IDX].__state == 0 && p_ch[2].__cs[SN_CSF_DIR_IDX].__state == 0);
    CHECK(p_ch[0].__cs[SN_CSF_ACTION_IDX].__state != 0 && p_ch[2].__cs[SN_CSF_ACTION_IDX].__state != 0);
    pifSolenoid_Clear(&solenoid);

#if PIF_SOLENOID_MAX_COUNT >= 255
    // Three digits at the last index there can be.
    _setUpN(&solenoid, 255, ST_1POINT, 30);
    CHECK(!s_failed);
    CHECK(pifSolenoid_SetCsFlag(&solenoid, SN_CSF_ALL_BIT));
    p_ch = solenoid.__p_channel;
    CHECK(strcmp(p_ch[254].__cs[SN_CSF_ACTION_IDX]._name, "SNA254") == 0);
    CHECK(strcmp(p_ch[254].__cs[SN_CSF_DIR_IDX]._name, "SND254") == 0);
    CHECK(strcmp(p_ch[100].__cs[SN_CSF_ACTION_IDX]._name, "SNA100") == 0);
    pifSolenoid_Clear(&solenoid);
#endif
}

#endif

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "init", testInit },
        { "pulse", testPulse },
        { "restart extends pulse", testRestartExtendsPulse },
        { "no on time", testNoOnTime },
        { "delay", testDelay },
        { "buffer", testBuffer },
        { "buffer after zero delay", testBufferAfterZeroDelay },
        { "late run keeps queue timing", testLateRunKeepsQueueTiming },
        { "action off cancels", testActionOffCancels },
        { "two point", testTwoPoint },
        { "clear", testClear },
        { "count", testCount },
        { "channels independent", testChannelsIndependent },
        { "channels separate queues", testChannelsSeparateQueues },
#ifdef PIF_COLLECT_SIGNAL
        { "collect signal", testCollectSignal },
#endif
    };

    if (!pif_Init(_timer1us)) {
        printf("pif_solenoid: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_solenoid\n");
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
