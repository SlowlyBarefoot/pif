// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_noise_filter_bit.h"
#include "filter/pif_noise_filter_int16.h"
#include "filter/pif_noise_filter_int32.h"

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


static int16_t _int16(PifNoiseFilter* p_filter, int16_t value)
{
    return *(int16_t*)pifNoiseFilter_Process(p_filter, &value);
}

static int32_t _int32(PifNoiseFilter* p_filter, int32_t value)
{
    return *(int32_t*)pifNoiseFilter_Process(p_filter, &value);
}

static SWITCH _bit(PifNoiseFilter* p_filter, SWITCH value)
{
    return *(SWITCH*)pifNoiseFilter_Process(p_filter, &value);
}

static void testAverageFilling(void)
{
    PifNfInt16Average filter;
    int16_t buffer[5];
    PifNoiseFilter* p_filter = &filter.parent;

    CHECK(pifNoiseFilterInt16_InitAverage(&filter, buffer, 5));
    CHECK(p_filter->_value == NFV_INT16 && p_filter->_type == NFT_AVERAGE);
    // A plain average below 3 samples, then a trimmed one.
    CHECK(_int16(p_filter, 10) == 10);
    CHECK(_int16(p_filter, 20) == 15);
    CHECK(_int16(p_filter, 900) == 20);
    CHECK(_int16(p_filter, -500) == 15);
    CHECK(!p_filter->_ready);
    CHECK(_int16(p_filter, 30) == 20);
    CHECK(p_filter->_ready);

    // The oldest sample, 10, leaves the window.
    CHECK(_int16(p_filter, 40) == 30);

    pifNoiseFilter_Reset(p_filter);
    CHECK(!p_filter->_ready);
    CHECK(_int16(p_filter, 7) == 7);
}

static void testAverageConstant(void)
{
    PifNfInt16Average filter;
    int16_t buffer[5];
    int i;

    CHECK(pifNoiseFilterInt16_InitAverage(&filter, buffer, 5));
    // Two samples are left out even when they are all the same.
    for (i = 0; i < 8; i++) CHECK(_int16(&filter.parent, 100) == 100);
}

static void testAverageInt32Range(void)
{
    PifNfInt32Average filter;
    int32_t buffer[3];
    int i;

    CHECK(pifNoiseFilterInt32_InitAverage(&filter, buffer, 3));
    CHECK(filter.parent._value == NFV_INT32);
    for (i = 0; i < 4; i++) CHECK(_int32(&filter.parent, 2000000000L) == 2000000000L);
}

static void testMedianSpike(void)
{
    PifNfInt16Median filter;
    int16_t buffer[PIF_NF_MEDIAN_BUFFER_SIZE(5)];
    PifNoiseFilter* p_filter = &filter.parent;
    int i;

    CHECK(pifNoiseFilterInt16_InitMedian(&filter, buffer, 5));
    CHECK(p_filter->_type == NFT_MEDIAN);
    for (i = 0; i < 5; i++) CHECK(_int16(p_filter, 10) == 10);
    CHECK(p_filter->_ready);

    // A spike of two samples is taken out, a third one makes a step.
    CHECK(_int16(p_filter, 500) == 10);
    CHECK(_int16(p_filter, 500) == 10);
    CHECK(_int16(p_filter, 500) == 500);
}

static void testMedianEven(void)
{
    PifNfInt16Median filter;
    int16_t buffer[PIF_NF_MEDIAN_BUFFER_SIZE(4)];
    PifNfInt32Median wide;
    int32_t wide_buffer[PIF_NF_MEDIAN_BUFFER_SIZE(3)];

    CHECK(pifNoiseFilterInt16_InitMedian(&filter, buffer, 4));
    CHECK(_int16(&filter.parent, 1) == 1);
    CHECK(_int16(&filter.parent, 3) == 2);
    CHECK(_int16(&filter.parent, 2) == 2);
    CHECK(_int16(&filter.parent, 10) == 2);

    // The two middle samples are added without overflowing.
    CHECK(pifNoiseFilterInt32_InitMedian(&wide, wide_buffer, 3));
    _int32(&wide.parent, 2000000000L);
    CHECK(_int32(&wide.parent, 2000000000L) == 2000000000L);
}

static void testMedianSliding(void)
{
    PifNfInt16Median filter;
    int16_t buffer[PIF_NF_MEDIAN_BUFFER_SIZE(7)];
    int16_t history[200], sorted[7], value;
    uint32_t seed = 1;
    int i, j, k, n;

    CHECK(pifNoiseFilterInt16_InitMedian(&filter, buffer, 7));
    for (i = 0; i < 200; i++) {
        seed = seed * 1103515245UL + 12345UL;
        history[i] = (int16_t)((seed >> 16) % 2001) - 1000;
        // Few values, so equal samples leave the window too.
        if (i & 1) history[i] /= 250;

        // The median of the last samples, by sorting them again.
        n = i + 1 < 7 ? i + 1 : 7;
        for (j = 0; j < n; j++) {
            value = history[i - j];
            for (k = j; k > 0 && sorted[k - 1] > value; k--) sorted[k] = sorted[k - 1];
            sorted[k] = value;
        }
        value = (n & 1) ? sorted[n / 2] : (int16_t)((sorted[n / 2 - 1] + sorted[n / 2]) / 2);
        CHECK(_int16(&filter.parent, history[i]) == value);
    }
}

static void testBitCount(void)
{
    PifNfBit filter;
    PifNoiseFilter* p_filter = &filter.parent;

    CHECK(pifNoiseFilterBit_InitCount(&filter, 5));
    CHECK(p_filter->_value == NFV_SWITCH);
    // A strict majority of the window.
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == ON);
    CHECK(_bit(p_filter, OFF) == ON);
    CHECK(!p_filter->_ready);
    CHECK(_bit(p_filter, OFF) == ON);
    CHECK(p_filter->_ready);
    CHECK(_bit(p_filter, OFF) == OFF);

    pifNoiseFilter_Reset(p_filter);
    CHECK(filter._result == OFF && !p_filter->_ready);
    CHECK(_bit(p_filter, ON) == OFF);
}

static void testBitContinue(void)
{
    PifNfBit filter;
    PifNoiseFilter* p_filter = &filter.parent;
    int i;

    CHECK(pifNoiseFilterBit_InitContinue(&filter, 5));
    // A single ON among OFF is a glitch.
    _bit(p_filter, ON);
    for (i = 0; i < 6; i++) CHECK(_bit(p_filter, OFF) == OFF);

    // A lasting ON shows after size / 2 samples of delay, and a single OFF then does not break it.
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == ON);
    CHECK(_bit(p_filter, OFF) == ON);
    for (i = 0; i < 6; i++) CHECK(_bit(p_filter, ON) == ON);
}

static void testEma(void)
{
    PifNfInt16Ema filter;
    PifNoiseFilter* p_filter = &filter.parent;
    int i;

    CHECK(pifNoiseFilterInt16_InitEma(&filter, 2));
    CHECK(p_filter->_type == NFT_EMA && p_filter->_value == NFV_INT16 && !p_filter->_ready);
    // The first sample is taken as it is, then each one moves the result by a quarter of the way.
    CHECK(_int16(p_filter, 100) == 100);
    CHECK(p_filter->_ready);
    CHECK(_int16(p_filter, 0) == 75);
    CHECK(_int16(p_filter, 0) == 56);
    for (i = 0; i < 40; i++) _int16(p_filter, 0);
    CHECK(_int16(p_filter, 0) == 0);
    // The fraction is kept, so the result reaches a level instead of stopping short of it.
    for (i = 0; i < 100; i++) _int16(p_filter, 1000);
    CHECK(_int16(p_filter, 1000) == 1000);

    // Negative levels behave as positive ones do.
    pifNoiseFilter_Reset(p_filter);
    CHECK(!p_filter->_ready);
    CHECK(_int16(p_filter, -100) == -100);
    CHECK(_int16(p_filter, 0) == -75);
    CHECK(_int16(p_filter, 0) == -56);
}

static void testEmaRange(void)
{
    PifNfInt16Ema narrow;
    PifNfInt32Ema wide;
    int i;

    CHECK(!pifNoiseFilterInt16_InitEma(&narrow, 0));
    CHECK(!pifNoiseFilterInt16_InitEma(&narrow, 16));
    CHECK(pifNoiseFilterInt16_InitEma(&narrow, 15));
    CHECK(_int16(&narrow.parent, INT16_MIN) == INT16_MIN);
    for (i = 0; i < 10; i++) CHECK(_int16(&narrow.parent, INT16_MIN) == INT16_MIN);
    CHECK(_int16(&narrow.parent, INT16_MAX) == INT16_MIN + 2);

    CHECK(!pifNoiseFilterInt32_InitEma(&wide, 32));
    CHECK(pifNoiseFilterInt32_InitEma(&wide, 31));
    CHECK(_int32(&wide.parent, 2000000000L) == 2000000000L);
    CHECK(_int32(&wide.parent, -2000000000L) == 1999999998L);
}

static void testDeadband(void)
{
    PifNfInt16Deadband filter;
    PifNfInt16Deadband wide;
    PifNoiseFilter* p_filter = &filter.parent;

    CHECK(!pifNoiseFilterInt16_InitDeadband(&filter, 0));
    CHECK(pifNoiseFilterInt16_InitDeadband(&filter, 5));
    CHECK(p_filter->_type == NFT_DEADBAND && p_filter->_value == NFV_INT16);
    CHECK(_int16(p_filter, 100) == 100);
    CHECK(p_filter->_ready);
    // A jitter smaller than the band keeps the result, and a change of the band or more is taken as it is.
    CHECK(_int16(p_filter, 104) == 100);
    CHECK(_int16(p_filter, 96) == 100);
    CHECK(_int16(p_filter, 105) == 105);
    CHECK(_int16(p_filter, 101) == 105);
    CHECK(_int16(p_filter, 100) == 100);

    pifNoiseFilter_Reset(p_filter);
    CHECK(_int16(p_filter, 102) == 102);

    // The distance between the ends of the range does not overflow.
    CHECK(pifNoiseFilterInt16_InitDeadband(&wide, 30000));
    CHECK(_int16(&wide.parent, -30000) == -30000);
    CHECK(_int16(&wide.parent, 30000) == 30000);
}

static void testBitIntegrator(void)
{
    PifNfBitIntegrator filter;
    PifNoiseFilter* p_filter = &filter.parent;
    int i;

    CHECK(!pifNoiseFilterBit_InitIntegrator(&filter, 0, 5));
    CHECK(!pifNoiseFilterBit_InitIntegrator(&filter, 3, 0));
    CHECK(pifNoiseFilterBit_InitIntegrator(&filter, 3, 5));
    CHECK(p_filter->_type == NFT_BIT_INTEGRATOR && p_filter->_value == NFV_SWITCH && !p_filter->_ready);

    // A bounce takes one count back instead of starting over.
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(p_filter->_ready);
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, OFF) == OFF);
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == ON);

    // Turning OFF takes 5 net OFF samples.
    for (i = 0; i < 4; i++) CHECK(_bit(p_filter, OFF) == ON);
    CHECK(_bit(p_filter, ON) == ON);
    CHECK(_bit(p_filter, OFF) == ON);
    CHECK(_bit(p_filter, OFF) == OFF);

    // ON while ON does not count beyond off_count.
    CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == OFF);
    for (i = 0; i < 10; i++) CHECK(_bit(p_filter, ON) == ON);
    for (i = 0; i < 4; i++) CHECK(_bit(p_filter, OFF) == ON);
    CHECK(_bit(p_filter, OFF) == OFF);

    pifNoiseFilter_Reset(p_filter);
    CHECK(filter._result == OFF && !p_filter->_ready);

    // Far longer than the 31 samples of a PifNfBit.
    CHECK(pifNoiseFilterBit_InitIntegrator(&filter, 1000, 1));
    for (i = 0; i < 999; i++) CHECK(_bit(p_filter, ON) == OFF);
    CHECK(_bit(p_filter, ON) == ON);
    CHECK(_bit(p_filter, OFF) == OFF);
}

static void testInvalid(void)
{
    PifNfBit bit;
    PifNfInt16Average average;
    PifNfInt32Median median;
    int16_t buffer[4];
    int32_t wide_buffer[PIF_NF_MEDIAN_BUFFER_SIZE(4)];

    CHECK(!pifNoiseFilterBit_InitCount(&bit, 2));
    CHECK(!pifNoiseFilterBit_InitContinue(&bit, 32));
    CHECK(!pifNoiseFilterInt16_InitAverage(&average, buffer, 2));
    CHECK(!pifNoiseFilterInt16_InitAverage(&average, NULL, 4));
    CHECK(!pifNoiseFilterInt32_InitMedian(NULL, wide_buffer, 4));
    CHECK(pif_error == E_INVALID_PARAM);

    CHECK(pifNoiseFilterInt32_InitMedian(&median, wide_buffer, 4));
    CHECK(pifNoiseFilter_Check(NULL, NFV_INT16));
    CHECK(pifNoiseFilter_Check(&median.parent, NFV_INT32));
    CHECK(!pifNoiseFilter_Check(&median.parent, NFV_INT16));
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "average filling", testAverageFilling },
        { "average constant", testAverageConstant },
        { "average int32 range", testAverageInt32Range },
        { "median spike", testMedianSpike },
        { "median even", testMedianEven },
        { "median sliding", testMedianSliding },
        { "bit count", testBitCount },
        { "bit continue", testBitContinue },
        { "ema", testEma },
        { "ema range", testEmaRange },
        { "deadband", testDeadband },
        { "bit integrator", testBitIntegrator },
        { "invalid", testInvalid },
    };

    printf("pif_noise_filter\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
