// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_pulse.h"

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

#define ALL_MODES	(PULSE_PMM_PERIOD | PULSE_PMM_COUNT | PULSE_PMM_LOW_WIDTH | PULSE_PMM_HIGH_WIDTH)

static BOOL s_failed;
static uint32_t s_now;
static int s_edge_count;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _evtEdge(PifPulseState state, PifIssuerP p_issuer)
{
    (void)p_issuer;
    if (state == PS_FALLING_EDGE) s_edge_count++;
}

// One pulse: high from rise to rise + high, then low until the next pulse.
static void _pulse(PifPulse* p_pulse, uint32_t rise, uint32_t high)
{
    pifPulse_sigEdge(p_pulse, PS_RISING_EDGE, rise);
    pifPulse_sigEdge(p_pulse, PS_FALLING_EDGE, rise + high);
    s_now = rise + high;
}

static void _setUp(PifPulse* p_pulse, uint8_t mode)
{
    s_now = 0;
    s_edge_count = 0;
    pif_error = E_SUCCESS;
    pifPulse_Init(p_pulse, PIF_ID_AUTO);
    if (mode) pifPulse_SetMeasureMode(p_pulse, mode);
}

static void testInit(void)
{
    PifPulse pulse;

    _setUp(&pulse, 0);
    CHECK(!pifPulse_Init(NULL, PIF_ID_AUTO) && pif_error == E_INVALID_PARAM);
    CHECK(!pifPulse_SetMeasureMode(&pulse, 0));
    CHECK(!pifPulse_SetMeasureMode(&pulse, 0x10));
    CHECK(!pifPulse_SetValidRange(&pulse, PULSE_PMM_COUNT, 0, 10));
    CHECK(!pifPulse_SetValidRange(&pulse, PULSE_PMM_PERIOD, 10, 0));
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetAveragePeriod(&pulse) == 0);
    CHECK(pifPulse_GetLowWidth(&pulse) == 0 && pifPulse_GetHighWidth(&pulse) == 0);
}

static void testMeasure(void)
{
    PifPulse pulse;

    _setUp(&pulse, ALL_MODES);
    pifPulse_AttachEvtEdge(&pulse, _evtEdge, NULL);

    // One pulse gives only the high width.
    _pulse(&pulse, 1000, 300);
    CHECK(pifPulse_GetHighWidth(&pulse) == 300);
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetLowWidth(&pulse) == 0);

    _pulse(&pulse, 2000, 400);
    CHECK(pifPulse_GetPeriod(&pulse) == 1100);
    CHECK(pifPulse_GetLowWidth(&pulse) == 700);
    CHECK(pifPulse_GetHighWidth(&pulse) == 400);
    CHECK(pulse.falling_count == 2 && s_edge_count == 2);
    CHECK(pifPulse_GetLastEdgeTime(&pulse) == 2400);
}

static void testMeasureMode(void)
{
    PifPulse pulse;

    // Without PULSE_PMM_COUNT nothing is counted, and a getter needs its own mode.
    _setUp(&pulse, PULSE_PMM_HIGH_WIDTH);
    _pulse(&pulse, 0, 100);
    _pulse(&pulse, 1000, 100);
    CHECK(pulse.falling_count == 0);
    CHECK(pifPulse_GetHighWidth(&pulse) == 100);
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetLowWidth(&pulse) == 0);

    pifPulse_ResetMeasureMode(&pulse, PULSE_PMM_HIGH_WIDTH);
    CHECK(pifPulse_GetHighWidth(&pulse) == 0);
}

static void testMissingRising(void)
{
    PifPulse pulse;

    _setUp(&pulse, ALL_MODES);
    _pulse(&pulse, 0, 100);
    // A falling edge without a rising edge before it has no widths, but still a period.
    pifPulse_sigEdge(&pulse, PS_FALLING_EDGE, 1100);
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);
    CHECK(pifPulse_GetHighWidth(&pulse) == 0 && pifPulse_GetLowWidth(&pulse) == 0);
}

static void testValidRange(void)
{
    PifPulse pulse;

    // Ranges beyond 16 bits, as for a slow tachometer.
    _setUp(&pulse, ALL_MODES);
    CHECK(pifPulse_SetValidRange(&pulse, PULSE_PMM_PERIOD, 100000UL, 2000000UL));
    CHECK(pifPulse_SetValidRange(&pulse, PULSE_PMM_HIGH_WIDTH, 0, 70000UL));
    _pulse(&pulse, 0, 50000UL);
    _pulse(&pulse, 500000UL, 80000UL);
    CHECK(pifPulse_GetPeriod(&pulse) == 530000UL);
    CHECK(pifPulse_GetHighWidth(&pulse) == 0);
    _pulse(&pulse, 550000UL, 60000UL);
    CHECK(pifPulse_GetPeriod(&pulse) == 0);
    CHECK(pifPulse_GetHighWidth(&pulse) == 60000UL);
}

static void testAveragePeriod(void)
{
    PifPulse pulse;
    uint32_t t;
    int i;

    _setUp(&pulse, PULSE_PMM_PERIOD);
    _pulse(&pulse, 0, 10);
    CHECK(pifPulse_GetAveragePeriod(&pulse) == 0);
    _pulse(&pulse, 1000, 10);
    CHECK(pifPulse_GetAveragePeriod(&pulse) == 1000);
    _pulse(&pulse, 3000, 10);
    CHECK(pifPulse_GetAveragePeriod(&pulse) == 1500);

    // Once the buffer is full only the last PIF_PULSE_DATA_SIZE falling edges count.
    _setUp(&pulse, PULSE_PMM_PERIOD);
    t = 0;
    for (i = 0; i < PIF_PULSE_DATA_SIZE * 3; i++) {
        t += (i & 1) ? 900 : 1100;
        _pulse(&pulse, t, 10);
    }
    CHECK(pifPulse_GetAveragePeriod(&pulse) >= 900 && pifPulse_GetAveragePeriod(&pulse) <= 1100);
    t += 2000;
    _pulse(&pulse, t, 10);
    CHECK(pifPulse_GetPeriod(&pulse) == 2000);
    CHECK(pifPulse_GetAveragePeriod(&pulse) > 1000 && pifPulse_GetAveragePeriod(&pulse) < 2000);
}

static void testTimeout(void)
{
    PifPulse pulse;

    _setUp(&pulse, ALL_MODES);
    pifPulse_SetTimeout(&pulse, 5000);
    _pulse(&pulse, 0, 100);
    _pulse(&pulse, 1000, 100);
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);

    s_now = 1100 + 5000;
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);
    s_now = 1100 + 5001;
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetAveragePeriod(&pulse) == 0);
    CHECK(pifPulse_GetHighWidth(&pulse) == 0 && pifPulse_GetLowWidth(&pulse) == 0);

    // The first pulse after the stop does not span the gap.
    _pulse(&pulse, 100000UL, 200);
    CHECK(pifPulse_GetPeriod(&pulse) == 0);
    CHECK(pifPulse_GetHighWidth(&pulse) == 200);
    _pulse(&pulse, 101000UL, 200);
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);

    // Timeout 0 turns it off.
    pifPulse_SetTimeout(&pulse, 0);
    s_now += 10000000UL;
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);
}

static void testRead(void)
{
    PifPulse pulse;
    uint32_t value;

    _setUp(&pulse, PULSE_PMM_PERIOD | PULSE_PMM_HIGH_WIDTH);
    pifPulse_SetTimeout(&pulse, 5000);
    CHECK(pifPulse_SetValidRange(&pulse, PULSE_PMM_PERIOD, 500, 1500));

    CHECK(pifPulse_Read(&pulse, PULSE_V_LOW_WIDTH, &value) == PULSE_R_DISABLED && value == 0);
    CHECK(pifPulse_Read(&pulse, PULSE_V_PERIOD, &value) == PULSE_R_NO_DATA && value == 0);
    CHECK(pifPulse_Read(&pulse, PULSE_V_HIGH_WIDTH, NULL) == PULSE_R_NO_DATA);

    _pulse(&pulse, 0, 100);
    _pulse(&pulse, 1000, 100);
    CHECK(pifPulse_Read(&pulse, PULSE_V_PERIOD, &value) == PULSE_R_OK && value == 1000);
    CHECK(pifPulse_Read(&pulse, PULSE_V_AVERAGE_PERIOD, &value) == PULSE_R_OK && value == 1000);
    CHECK(pifPulse_Read(&pulse, PULSE_V_HIGH_WIDTH, &value) == PULSE_R_OK && value == 100);

    // Out of range still gives the measured value, which the getter reports as 0.
    _pulse(&pulse, 3000, 100);
    CHECK(pifPulse_Read(&pulse, PULSE_V_PERIOD, &value) == PULSE_R_OUT_OF_RANGE && value == 2000);
    CHECK(pifPulse_GetPeriod(&pulse) == 0);

    s_now += 5001;
    CHECK(pifPulse_Read(&pulse, PULSE_V_PERIOD, &value) == PULSE_R_TIMEOUT && value == 0);
    CHECK(pifPulse_Read(&pulse, PULSE_V_HIGH_WIDTH, &value) == PULSE_R_TIMEOUT && value == 0);

    CHECK(pifPulse_Read(&pulse, (PifPulseValue)4, &value) == PULSE_R_DISABLED && pif_error == E_INVALID_PARAM);
}

static void testWrapAround(void)
{
    PifPulse pulse;

    _setUp(&pulse, ALL_MODES);
    pifPulse_SetTimeout(&pulse, 5000);
    _pulse(&pulse, 0xFFFFFF00UL, 100);
    _pulse(&pulse, 0xFFFFFF00UL + 1000, 100);
    CHECK(pifPulse_GetPeriod(&pulse) == 1000);
    CHECK(pifPulse_GetLowWidth(&pulse) == 900);
}

static void testReset(void)
{
    PifPulse pulse;

    _setUp(&pulse, ALL_MODES);
    _pulse(&pulse, 0, 100);
    _pulse(&pulse, 1000, 100);
    pifPulse_ResetMeasureValue(&pulse);
    CHECK(pulse.falling_count == 0 && pifPulse_GetLastEdgeTime(&pulse) == 0);
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetHighWidth(&pulse) == 0);
    _pulse(&pulse, 5000, 100);
    CHECK(pifPulse_GetPeriod(&pulse) == 0 && pifPulse_GetHighWidth(&pulse) == 100);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "init", testInit },
        { "measure", testMeasure },
        { "measure mode", testMeasureMode },
        { "missing rising", testMissingRising },
        { "valid range", testValidRange },
        { "average period", testAveragePeriod },
        { "timeout", testTimeout },
        { "read", testRead },
        { "wrap around", testWrapAround },
        { "reset", testReset },
    };

    if (!pif_Init(_timer1us)) {
        printf("pif_pulse: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_pulse\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
