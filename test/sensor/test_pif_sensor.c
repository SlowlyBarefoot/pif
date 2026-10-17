// SPDX-License-Identifier: BSD-3-Clause
// PifSensor and the sensors on it: pifSensorSwitch, pifSensorThreshold and the thresholds of pifMax31855.
#include "sensor/pif_sensor_switch.h"
#include "sensor/pif_sensor_threshold.h"
#include "filter/pif_noise_filter_bit.h"
#include "filter/pif_noise_filter_int32.h"

// Included to run the measuring task directly.
#include "../source/sensor/pif_max31855.c"

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

static BOOL s_failed;
static int32_t s_sample;			// What act_acquire answers
static int s_events;
static SWITCH s_event_state;
static SWITCH s_event_curr_state;	// _curr_state seen by the event
static BOOL s_event_has_value;
static int32_t s_event_level;
static double s_event_temperature;
static PifSensor* s_p_sensor;
static int16_t s_quarter;			// Thermocouple of the fake MAX31855 in 0.25 degree units


static uint32_t _timer1us(void)
{
    return 0;
}

static int32_t _actAcquire(PifSensor* p_owner)
{
    (void)p_owner;
    return s_sample;
}

static void _evtChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer)
{
    (void)p_issuer;
    s_events++;
    s_p_sensor = p_owner;
    s_event_state = state;
    s_event_curr_state = p_owner->_curr_state;
    s_event_has_value = p_value != NULL;
    if (p_value) s_event_level = *(int32_t*)p_value;
}

static void _evtTemperature(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer)
{
    (void)p_owner;
    (void)p_issuer;
    s_events++;
    s_event_state = state;
    s_event_temperature = *(double*)p_value;
}

static void _clearEvents(void)
{
    s_events = 0;
    s_event_state = OFF;
    s_event_curr_state = OFF;
    s_event_has_value = FALSE;
    s_event_level = 0;
    s_p_sensor = NULL;
}

// Answers like a MAX31855 without faults whose cold junction is at 0 degree.
static void _spiTransfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
    uint16_t data = (uint16_t)s_quarter & 0x3FFF;

    (void)p_owner;
    (void)p_write;
    if (size != 4) return;
    p_read[0] = data >> 6;
    p_read[1] = (data & 0x3F) << 2;
    p_read[2] = 0;
    p_read[3] = 0;
}

static void testHysteresis(void)
{
    CHECK(pifSensor_Hysteresis(OFF, 299, 100, 300) == OFF);
    CHECK(pifSensor_Hysteresis(OFF, 300, 100, 300) == ON);
    CHECK(pifSensor_Hysteresis(ON, 100, 100, 300) == ON);
    CHECK(pifSensor_Hysteresis(ON, 99, 100, 300) == OFF);
    CHECK(pifSensor_Hysteresis(OFF, -5, -10, -5) == ON);
    CHECK(pifSensor_Hysteresis(ON, -11, -10, -5) == OFF);

    // Equal thresholds: the level of the threshold keeps either state.
    CHECK(pifSensor_Hysteresis(OFF, 200, 200, 200) == ON);
    CHECK(pifSensor_Hysteresis(ON, 200, 200, 200) == ON);
    CHECK(pifSensor_Hysteresis(ON, 199, 200, 200) == OFF);
}

static void testSwitch(void)
{
    PifSensorSwitch sensor;

    _clearEvents();
    CHECK(pifSensorSwitch_Init(&sensor, PIF_ID_AUTO, OFF, _actAcquire));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);

    s_sample = 0;
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 0);

    // Any sample but OFF is ON.
    s_sample = 0x100;
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == ON && s_event_curr_state == ON && !s_event_has_value);
    CHECK(s_p_sensor == &sensor.parent && sensor.parent._curr_state == ON);
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 1);

    s_sample = OFF;
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 2 && s_event_state == OFF && sensor.parent._curr_state == OFF);
    pifSensorSwitch_Clear(&sensor);
}

static void testSwitchSigData(void)
{
    PifSensorSwitch sensor;

    _clearEvents();
    CHECK(pifSensorSwitch_Init(&sensor, PIF_ID_AUTO, ON, NULL));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);
    CHECK(sensor.parent._curr_state == ON);

    pifSensorSwitch_InitialState(&sensor);
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 0);

    pifSensorSwitch_sigData(&sensor, OFF);
    CHECK(s_events == 0);
    pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == OFF);
}

static void testSwitchFilter(void)
{
    PifNfBit filter;
    PifNfInt32Average wrong;
    int32_t buffer[3];
    PifSensorSwitch sensor;
    int i;

    _clearEvents();
    CHECK(pifSensorSwitch_Init(&sensor, PIF_ID_AUTO, OFF, _actAcquire));
    CHECK(pifNoiseFilterBit_InitCount(&filter, 5));
    CHECK(pifNoiseFilterInt32_InitAverage(&wrong, buffer, 3));
    // A filter of another value type is refused.
    CHECK(!pifSensorSwitch_AttachFilter(&sensor, &wrong.parent));
    CHECK(pifSensorSwitch_AttachFilter(&sensor, &filter.parent));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);

    // A single glitch is filtered out, a lasting change is not.
    s_sample = ON;
    pifSensorSwitch_ProcessAcquire(&sensor);
    s_sample = OFF;
    for (i = 0; i < 5; i++) pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 0);

    s_sample = ON;
    for (i = 0; i < 5; i++) pifSensorSwitch_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == ON);
}

static void testThresholdInit(void)
{
    PifSensorThreshold sensor;

    _clearEvents();
    CHECK(pifSensorThreshold_Init(&sensor, 7, ON, _actAcquire));
    CHECK(sensor.parent._id == 7 && sensor.parent._init_state == ON && sensor.parent._curr_state == ON);
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);

    // Nothing changes before the thresholds are set.
    s_sample = -1000;
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 0 && sensor.parent._curr_state == ON && sensor._curr_level == -1000);

    CHECK(!pifSensorThreshold_SetThreshold(&sensor, 300, 200) && pif_error == E_INVALID_PARAM);
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 0);

    CHECK(pifSensorThreshold_SetThreshold(&sensor, 200, 300));
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == OFF && s_event_curr_state == OFF && s_event_level == -1000);

    pifSensorThreshold_InitialState(&sensor);
    CHECK(sensor.parent._curr_state == ON && s_events == 1);
    pifSensorThreshold_Clear(&sensor);
}

static void testThresholdWithoutLevel(void)
{
    PifSensorThreshold sensor;

    _clearEvents();
    CHECK(pifSensorThreshold_Init(&sensor, PIF_ID_AUTO, ON, NULL));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);
    CHECK(pifSensorThreshold_SetThreshold(&sensor, 200, 300));

    // No level was given yet, so the initial state stays.
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 0 && sensor.parent._curr_state == ON);

    pifSensorThreshold_sigData(&sensor, 250);
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 0);
    pifSensorThreshold_sigData(&sensor, 199);
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == OFF && s_event_level == 199);

    // InitialState forgets the level.
    pifSensorThreshold_InitialState(&sensor);
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && sensor.parent._curr_state == ON);
}

static void testThresholdHysteresis(void)
{
    PifSensorThreshold sensor;
    static const struct {
        int32_t level;
        SWITCH state;
    } steps[] = {
        { 299, OFF }, { 300, ON }, { 250, ON }, { 200, ON }, { 199, OFF }, { 250, OFF }, { 70000, ON }, { -70000, OFF }
    };
    size_t i;
    int expected = 0;
    SWITCH last = OFF;

    _clearEvents();
    CHECK(pifSensorThreshold_Init(&sensor, PIF_ID_AUTO, OFF, _actAcquire));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);
    CHECK(pifSensorThreshold_SetThreshold(&sensor, 200, 300));

    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        s_sample = steps[i].level;
        pifSensorThreshold_ProcessAcquire(&sensor);
        if (steps[i].state != last) expected++;
        last = steps[i].state;
        CHECK(sensor.parent._curr_state == steps[i].state && s_events == expected);
    }
    CHECK(s_event_level == -70000);
}

static void testThresholdSingle(void)
{
    PifSensorThreshold sensor;
    int i;

    _clearEvents();
    CHECK(pifSensorThreshold_Init(&sensor, PIF_ID_AUTO, OFF, _actAcquire));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);
    CHECK(pifSensorThreshold_SetThreshold(&sensor, 550, 550));

    // Staying at the threshold does not toggle the state on every sample.
    s_sample = 550;
    for (i = 0; i < 4; i++) pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && sensor.parent._curr_state == ON);

    s_sample = 549;
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 2 && sensor.parent._curr_state == OFF);
}

static void testThresholdFilter(void)
{
    PifNfInt32Average filter;
    PifNfBit wrong;
    int32_t buffer[4];
    PifSensorThreshold sensor;
    int i;

    _clearEvents();
    CHECK(pifSensorThreshold_Init(&sensor, PIF_ID_AUTO, OFF, _actAcquire));
    CHECK(pifNoiseFilterInt32_InitAverage(&filter, buffer, 4));
    CHECK(pifNoiseFilterBit_InitCount(&wrong, 5));
    // A filter of another value type is refused.
    CHECK(!pifSensorThreshold_AttachFilter(&sensor, &wrong.parent));
    CHECK(pifSensorThreshold_AttachFilter(&sensor, &filter.parent));
    pifSensor_AttachEvtChange(&sensor.parent, _evtChange, NULL);
    CHECK(pifSensorThreshold_SetThreshold(&sensor, 100000, 100000));

    // Levels are not used while the filter fills its window, and levels beyond 16 bits pass it.
    s_sample = 200000;
    for (i = 0; i < 3; i++) pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 0 && sensor._curr_level == 0);
    pifSensorThreshold_ProcessAcquire(&sensor);
    CHECK(s_events == 1 && s_event_state == ON && sensor._curr_level == 200000);
}

static void testMax31855Threshold(void)
{
    static PifSpiPort s_port;
    PifMax31855 sensor;
    PifTask task;
    double temperature;

    _clearEvents();
    CHECK(pifSpiPort_Init(&s_port, PIF_ID_AUTO, 1));
    s_port.act_transfer = _spiTransfer;
    CHECK(pifMax31855_Init(&sensor, PIF_ID_AUTO, &s_port, NULL));
    pifSensor_AttachEvtChange(&sensor.parent, _evtTemperature, NULL);
    memset(&task, 0, sizeof(task));
    task._p_client = &sensor;

    s_quarter = -40;					// -10 degree
    CHECK(pifMax31855_Measure(&sensor, &temperature, NULL) && temperature == -10.0);

    // Nothing changes before the thresholds are set.
    s_quarter = 400;
    _doTask(&task);
    CHECK(s_events == 0 && sensor.parent._curr_state == OFF);

    // 25.1 rounds up to 100.4, so 25.0 stays below it and 25.25 is above.
    CHECK(!pifMax31855_SetThreshold(&sensor, 30.0, 20.0));
    CHECK(pifMax31855_SetThreshold(&sensor, -0.1, 25.1));
    s_quarter = 100;					// 25.0
    _doTask(&task);
    CHECK(s_events == 0);
    s_quarter = 101;					// 25.25
    _doTask(&task);
    CHECK(s_events == 1 && s_event_state == ON && s_event_temperature == 25.25);

    // -0.1 rounds up to 0: 0 keeps ON and -0.25 turns OFF.
    s_quarter = 0;
    _doTask(&task);
    CHECK(s_events == 1);
    s_quarter = -1;
    _doTask(&task);
    CHECK(s_events == 2 && s_event_state == OFF && s_event_temperature == -0.25);

    CHECK(_toQuarter(1e12) == 2147483647L && _toQuarter(-1e12) == -2147483647L - 1);
    CHECK(_toQuarter(-1.3) == -5 && _toQuarter(1.3) == 6 && _toQuarter(2.0) == 8);
    pifMax31855_Clear(&sensor);
    pifSpiPort_Clear(&s_port);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "hysteresis", testHysteresis },
        { "switch", testSwitch },
        { "switch sigData", testSwitchSigData },
        { "switch filter", testSwitchFilter },
        { "threshold init", testThresholdInit },
        { "threshold without level", testThresholdWithoutLevel },
        { "threshold hysteresis", testThresholdHysteresis },
        { "threshold single", testThresholdSingle },
        { "threshold filter", testThresholdFilter },
        { "max31855 threshold", testMax31855Threshold },
    };

    if (!pif_Init(_timer1us)) {
        printf("pif_sensor: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_sensor\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
