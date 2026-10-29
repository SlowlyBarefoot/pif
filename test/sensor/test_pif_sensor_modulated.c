// SPDX-License-Identifier: BSD-3-Clause
// PifSensorModulated on its task, with the samples of the ADC interrupt given between the passes of the main loop
// or read by act_acquire, and the levels handed to a PifSensorThreshold.
#include "sensor/pif_sensor_modulated.h"

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

#define CHANNELS		2
#define SETTLE_US		100
#define SAMPLE_US		200
#define CYCLE_US		(2 * (SETTLE_US + SAMPLE_US))
#define STEP_US			10		// Time between two passes of the main loop
#define GLITCH			5000	// What a channel reads while the light has not settled yet

static BOOL s_failed;
static uint32_t s_now;
static PifSensorModulated s_sensor;

// The fake optics: each channel reads its level with the LED on or off, but only s_light_delay after the LED
// was switched. Before that it reads GLITCH, which no window may take in.
static SWITCH s_led;
static int s_led_switches;
static uint32_t s_led_time;
static uint32_t s_light_delay;
static int32_t s_on_value[CHANNELS];
static int32_t s_off_value[CHANNELS];

static BOOL s_isr;					// The interrupt gives a sample of every channel between two passes
static int s_acquires[CHANNELS];

static int s_cycles;
static uint32_t s_cycle_time[2];
static BOOL s_stop_on_cycle;

static int s_events;
static SWITCH s_event_state;
static int32_t s_event_level;


static uint32_t _timer1us(void)
{
    return s_now;
}

static BOOL _portSetup(void)
{
    return TRUE;
}

static int32_t _light(uint8_t channel)
{
    if (s_now - s_led_time < s_light_delay) return GLITCH;
    return s_led ? s_on_value[channel] : s_off_value[channel];
}

static void _actLed(PifSensorModulated* p_owner, SWITCH state)
{
    (void)p_owner;
    if (state != s_led) {
        s_led_switches++;
        s_led_time = s_now;
    }
    s_led = state;
}

static int32_t _actAcquire(PifSensorModulated* p_owner, uint8_t channel)
{
    (void)p_owner;
    s_acquires[channel]++;
    return _light(channel);
}

static void _evtCycle(PifSensorModulated* p_owner)
{
    if (s_cycles < 2) s_cycle_time[s_cycles] = s_now;
    s_cycles++;
    if (s_stop_on_cycle) pifSensorModulated_Stop(p_owner);
}

static void _evtChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer)
{
    (void)p_owner;
    (void)p_issuer;
    s_events++;
    s_event_state = state;
    s_event_level = p_value ? *(int32_t*)p_value : 0;
}

// Runs the main loop until the clock reaches until, and plays the interrupt before every pass.
static void _runUntil(uint32_t until)
{
    uint8_t i;

    while ((int32_t)(until - s_now) > 0) {
        if (s_isr) {
            for (i = 0; i < CHANNELS; i++) pifSensorModulated_sigSample(&s_sensor, i, _light(i));
        }
        pifTaskManager_Loop();
        s_now += STEP_US;
    }
}

// A fresh task manager, the LED off, and a modulated sensor with the default timing that is not started yet.
static void _setUp(void)
{
    uint8_t i;

    s_now = 0;
    pifTaskManager_Clear();
    s_led = OFF;
    s_led_switches = 0;
    s_led_time = 0;
    s_light_delay = SETTLE_US / 2;
    for (i = 0; i < CHANNELS; i++) {
        s_on_value[i] = 900 + 100 * i;
        s_off_value[i] = 100 + 50 * i;
        s_acquires[i] = 0;
    }
    s_isr = TRUE;
    s_cycles = 0;
    s_cycle_time[0] = 0;
    s_cycle_time[1] = 0;
    s_stop_on_cycle = FALSE;
    s_events = 0;
    s_event_state = OFF;
    s_event_level = 0;
    pif_error = E_SUCCESS;

    if (!pifTaskManager_Init(2, 0) ||
            !pifSensorModulated_Init(&s_sensor, PIF_ID_AUTO, CHANNELS, _actLed) ||
            !pifSensorModulated_SetTiming(&s_sensor, SETTLE_US, SAMPLE_US)) {
        s_failed = TRUE;
        return;
    }
    s_sensor.evt_cycle = _evtCycle;
}

static void testParams(void)
{
    PifSensorModulated sensor;

    _setUp();
    CHECK(!s_failed);

    CHECK(!pifSensorModulated_Init(&sensor, PIF_ID_AUTO, 0, _actLed) && pif_error == E_INVALID_PARAM);
    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_Init(&sensor, PIF_ID_AUTO, 1, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSensorModulated_Init(NULL, PIF_ID_AUTO, 1, _actLed));

    // Not started before the timing is set.
    CHECK(pifSensorModulated_Init(&sensor, 9, 1, _actLed));
    CHECK(sensor._id == 9 && sensor._channel_count == 1 && sensor._phase == SMP_IDLE && sensor._p_task);
    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_Start(&sensor) && pif_error == E_INVALID_STATE);
    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_SetTiming(&sensor, 10, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifSensorModulated_SetTiming(&sensor, 0, 1));

    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_AttachActAcquire(&sensor, _actAcquire, 0) && pif_error == E_INVALID_PARAM);
    CHECK(pifSensorModulated_AttachActAcquire(&sensor, NULL, 0));

    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_AttachSensor(&sensor, 1, NULL, SML_ON) && pif_error == E_INVALID_PARAM);
    pif_error = E_SUCCESS;
    CHECK(!pifSensorModulated_AttachSensor(&sensor, 0, NULL, (PifSensorModulatedLevel)3) &&
            pif_error == E_INVALID_PARAM);
    CHECK(pifSensorModulated_AttachSensor(&sensor, 0, NULL, SML_DIFF));

    CHECK(pifSensorModulated_GetLevel(&sensor, 1, SML_ON) == 0);
    CHECK(pifSensorModulated_GetLevel(NULL, 0, SML_ON) == 0);

    // Clear gives the task back and frees the channels.
    CHECK(pifTaskManager_Count() == 2);
    pifSensorModulated_Clear(&sensor);
    CHECK(pifTaskManager_Count() == 1 && !sensor._p_task && sensor._channel_count == 0);
    pifSensorModulated_Clear(&s_sensor);
}

static void testInterruptSamples(void)
{
    uint8_t i;

    _setUp();
    CHECK(!s_failed);

    CHECK(pifSensorModulated_Start(&s_sensor));
    CHECK(s_led == OFF && s_sensor._phase == SMP_IDLE);

    // The first pass switches the LED on and waits for the light.
    _runUntil(STEP_US);
    CHECK(s_led == ON && s_led_switches == 1 && s_sensor._phase == SMP_ON_SETTLE);

    _runUntil(CYCLE_US + STEP_US);
    CHECK(s_cycles == 1 && s_cycle_time[0] == CYCLE_US && s_sensor._cycle_count == 1);
    // The next cycle has already begun with the LED on.
    CHECK(s_led == ON && s_led_switches == 3 && s_sensor._phase == SMP_ON_SETTLE);

    // The samples before the light settled fall in the settle steps and are left out.
    for (i = 0; i < CHANNELS; i++) {
        CHECK(pifSensorModulated_GetLevel(&s_sensor, i, SML_ON) == s_on_value[i]);
        CHECK(pifSensorModulated_GetLevel(&s_sensor, i, SML_OFF) == s_off_value[i]);
        CHECK(pifSensorModulated_GetLevel(&s_sensor, i, SML_DIFF) == s_on_value[i] - s_off_value[i]);
    }

    // The ambient light changes, and the next cycle follows it.
    s_on_value[0] = 1500;
    s_off_value[0] = 700;
    _runUntil(2 * CYCLE_US + STEP_US);
    CHECK(s_cycles == 2 && s_cycle_time[1] == 2 * CYCLE_US);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_ON) == 1500);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_OFF) == 700);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_DIFF) == 800);
    pifSensorModulated_Clear(&s_sensor);
}

static void testSampleOutsideWindow(void)
{
    _setUp();
    CHECK(!s_failed);

    // Stopped: the samples are ignored.
    pifSensorModulated_sigSample(&s_sensor, 0, 1);
    CHECK(s_sensor.__p_channel[0].__count == 0);

    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(SETTLE_US / 2);
    CHECK(s_sensor._phase == SMP_ON_SETTLE);
    pifSensorModulated_sigSample(&s_sensor, 0, 1);
    CHECK(s_sensor.__p_channel[0].__count == 0);

    // The window has just opened, after the interrupt of that pass. A channel that does not exist is ignored.
    _runUntil(SETTLE_US + STEP_US);
    CHECK(s_sensor._phase == SMP_ON_SAMPLE);
    s_isr = FALSE;
    pifSensorModulated_sigSample(&s_sensor, CHANNELS, 1);
    CHECK(s_sensor.__p_channel[0].__count == 0 && s_sensor.__p_channel[1].__count == 0);

    // A window takes at most 65535 samples, and the average stays right.
    while (s_sensor.__p_channel[0].__count < 0xFFFF) pifSensorModulated_sigSample(&s_sensor, 0, s_on_value[0]);
    pifSensorModulated_sigSample(&s_sensor, 0, 0);
    CHECK(s_sensor.__p_channel[0].__count == 0xFFFF);
    _runUntil(SETTLE_US + SAMPLE_US + STEP_US);
    CHECK(s_sensor._phase == SMP_OFF_SETTLE);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_ON) == s_on_value[0]);
    pifSensorModulated_Clear(&s_sensor);
}

static void testActAcquire(void)
{
    uint8_t i;

    _setUp();
    CHECK(!s_failed);
    s_isr = FALSE;

    // Read at the opening of a window and every 50us after: 0, 50, 100 and 150 of a 200us window.
    CHECK(pifSensorModulated_AttachActAcquire(&s_sensor, _actAcquire, 50));
    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(CYCLE_US + STEP_US);
    CHECK(s_cycles == 1 && s_cycle_time[0] == CYCLE_US);
    for (i = 0; i < CHANNELS; i++) {
        CHECK(s_acquires[i] == 8);
        CHECK(pifSensorModulated_GetLevel(&s_sensor, i, SML_ON) == s_on_value[i]);
        CHECK(pifSensorModulated_GetLevel(&s_sensor, i, SML_OFF) == s_off_value[i]);
    }

    // A period as long as the window reads once per window, from the next window on.
    CHECK(pifSensorModulated_AttachActAcquire(&s_sensor, _actAcquire, SAMPLE_US));
    _runUntil(2 * CYCLE_US + STEP_US);
    CHECK(s_cycles == 2 && s_cycle_time[1] == 2 * CYCLE_US && s_acquires[0] == 10);

    // Together with the interrupt.
    s_isr = TRUE;
    s_off_value[1] = 300;
    _runUntil(3 * CYCLE_US + STEP_US);
    CHECK(s_cycles == 3 && s_acquires[1] == 12);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 1, SML_OFF) == 300);
    pifSensorModulated_Clear(&s_sensor);
}

static void testNoSettle(void)
{
    _setUp();
    CHECK(!s_failed);
    s_isr = FALSE;
    s_light_delay = 0;

    // The window opens with the switch of the LED, so a cycle is two windows long.
    CHECK(pifSensorModulated_SetTiming(&s_sensor, 0, SAMPLE_US));
    CHECK(pifSensorModulated_AttachActAcquire(&s_sensor, _actAcquire, 1000));
    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(STEP_US);
    CHECK(s_led == ON && s_sensor._phase == SMP_ON_SAMPLE && s_acquires[0] == 1);

    _runUntil(2 * SAMPLE_US + STEP_US);
    CHECK(s_cycles == 1 && s_cycle_time[0] == 2 * SAMPLE_US && s_acquires[0] == 3);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_DIFF) == s_on_value[0] - s_off_value[0]);
    pifSensorModulated_Clear(&s_sensor);
}

static void testThreshold(void)
{
    PifSensorThreshold threshold;

    _setUp();
    CHECK(!s_failed);

    CHECK(pifSensorThreshold_Init(&threshold, PIF_ID_AUTO, OFF, NULL));
    CHECK(pifSensorThreshold_SetThreshold(&threshold, 300, 500));
    pifSensor_AttachEvtChange(&threshold.parent, _evtChange, NULL);
    CHECK(pifSensorModulated_AttachSensor(&s_sensor, 0, &threshold, SML_DIFF));

    // An object reflects the LED: 900 - 100.
    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(CYCLE_US + STEP_US);
    CHECK(s_events == 1 && s_event_state == ON && s_event_level == 800);
    CHECK(threshold.parent._curr_state == ON);

    // More ambient light raises both levels but not their difference.
    s_on_value[0] += 2000;
    s_off_value[0] += 2000;
    _runUntil(2 * CYCLE_US + STEP_US);
    CHECK(s_events == 1 && threshold._curr_level == 800);

    // The object goes away.
    s_on_value[0] = s_off_value[0] + 100;
    _runUntil(3 * CYCLE_US + STEP_US);
    CHECK(s_events == 2 && s_event_state == OFF && s_event_level == 100);

    // Detached: nothing is handed over any more.
    CHECK(pifSensorModulated_AttachSensor(&s_sensor, 0, NULL, SML_DIFF));
    s_on_value[0] += 1000;
    _runUntil(4 * CYCLE_US + STEP_US);
    CHECK(s_cycles == 4 && s_events == 2 && threshold._curr_level == 100);
    pifSensorModulated_Clear(&s_sensor);
}

static void testNoSamples(void)
{
    PifSensorThreshold threshold;

    _setUp();
    CHECK(!s_failed);
    s_isr = FALSE;

    CHECK(pifSensorThreshold_Init(&threshold, PIF_ID_AUTO, OFF, NULL));
    CHECK(pifSensorThreshold_SetThreshold(&threshold, 300, 500));
    pifSensor_AttachEvtChange(&threshold.parent, _evtChange, NULL);
    CHECK(pifSensorModulated_AttachSensor(&s_sensor, 0, &threshold, SML_ON));

    // Windows without samples end the cycle, but hand nothing over.
    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(CYCLE_US + STEP_US);
    CHECK(s_cycles == 1 && s_events == 0 && !threshold.__level_set);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_ON) == 0);

    // Only the on window has samples: SML_ON is handed over, SML_DIFF would not be.
    CHECK(pifSensorModulated_AttachSensor(&s_sensor, 1, &threshold, SML_DIFF));
    s_isr = TRUE;
    _runUntil(CYCLE_US + SETTLE_US + SAMPLE_US + STEP_US);
    s_isr = FALSE;
    _runUntil(2 * CYCLE_US + STEP_US);
    CHECK(s_cycles == 2 && s_events == 1 && s_event_level == s_on_value[0]);
    pifSensorModulated_Clear(&s_sensor);
}

static void testStop(void)
{
    uint8_t i;

    _setUp();
    CHECK(!s_failed);

    // Stopped from the cycle event: the LED goes off and the task runs no more.
    s_stop_on_cycle = TRUE;
    CHECK(pifSensorModulated_Start(&s_sensor));
    _runUntil(3 * CYCLE_US);
    CHECK(s_cycles == 1 && s_led == OFF && s_led_switches == 2 && s_sensor._phase == SMP_IDLE);
    CHECK(pifSensorModulated_GetLevel(&s_sensor, 0, SML_DIFF) == s_on_value[0] - s_off_value[0]);

    // A start begins a new count and does not hand the old levels over again.
    s_stop_on_cycle = FALSE;
    CHECK(pifSensorModulated_Start(&s_sensor));
    for (i = 0; i < CHANNELS; i++) CHECK(!s_sensor.__p_channel[i].__on_valid);
    CHECK(s_sensor._cycle_count == 0);
    _runUntil(4 * CYCLE_US + STEP_US);
    CHECK(s_sensor._cycle_count == 1 && s_led == ON);

    // Stopped in the middle of a window.
    _runUntil(4 * CYCLE_US + SETTLE_US + SAMPLE_US / 2);
    CHECK(s_sensor._phase == SMP_ON_SAMPLE);
    pifSensorModulated_Stop(&s_sensor);
    CHECK(s_led == OFF && s_sensor._phase == SMP_IDLE);
    _runUntil(6 * CYCLE_US);
    CHECK(s_sensor._cycle_count == 1 && s_led == OFF);
    pifSensorModulated_Clear(&s_sensor);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "params", testParams },
        { "interrupt samples", testInterruptSamples },
        { "sample outside window", testSampleOutsideWindow },
        { "act_acquire", testActAcquire },
        { "no settle", testNoSettle },
        { "threshold", testThreshold },
        { "no samples", testNoSamples },
        { "stop", testStop },
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_sensor_modulated: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_sensor_modulated\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
