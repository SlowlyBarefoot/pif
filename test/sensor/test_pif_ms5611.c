// SPDX-License-Identifier: BSD-3-Clause
// PifMs5611 against an MS5611 in memory on a fake I2C port: the reset and the PROM with its CRC, the
// conversion commands and the times they are left for, the compensation of the datasheet, and the two
// drivers that carry a reading.
#include "ms5611_emu.h"

#include "../source/sensor/baro/pif_ms5611.c"

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

#define CHECK_EQ(GOT, EXP) \
    do { \
        if ((long)(GOT) != (long)(EXP)) { \
            printf("  FAIL %s:%d: %s is %ld, expected %ld\n", __FILE__, __LINE__, #GOT, \
                   (long)(GOT), (long)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

// A hundredth is the last digit the device resolves, so anything closer than half of one is the same value.
#define CHECK_NEAR(GOT, EXP) \
    do { \
        double d = (double)(GOT) - (double)(EXP); \
        if (d < -0.005 || d > 0.005) { \
            printf("  FAIL %s:%d: %s is %f, expected %f\n", __FILE__, __LINE__, #GOT, \
                   (double)(GOT), (double)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define MS5611_ADDR		MS5611_I2C_ADDR(1)


static BOOL s_failed;
static uint32_t s_now_us;

static PifI2cPort s_port;
static PifTimerManager s_manager;
static Ms5611Emu s_emu;
static PifMs5611 s_baro;

static int s_reads;					// Readings reported through the task event
static float s_pressure, s_temperature;
static int s_timer_reads;			// Readings reported through the timer event
static BOOL s_timer_result;
static float s_timer_pressure, s_timer_temperature;


static uint32_t _timer1us(void)
{
    return s_now_us;
}

static PifI2cReturn _actRead(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    if (p_device->addr != MS5611_ADDR) return IR_ERROR;
    return ms5611Emu_Read(&s_emu, iaddr, isize, p_data, size);
}

static PifI2cReturn _actWrite(PifI2cDevice* p_device, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
    if (p_device->addr != MS5611_ADDR) return IR_ERROR;
    return ms5611Emu_Write(&s_emu, iaddr, isize, p_data, size);
}

static void _evtRead(float pressure, float temperature)
{
    s_reads++;
    s_pressure = pressure;
    s_temperature = temperature;
}

static void _evtTimerRead(PifMs5611* p_owner, BOOL result, float pressure, float temperature)
{
    s_timer_reads++;
    s_timer_result = result;
    s_timer_pressure = pressure;
    s_timer_temperature = temperature;
}

// One tick of the millisecond interrupt, then one pass of the main loop, where the timer callback runs.
static void _tick(int count)
{
    while (count--) {
        pifTimerManager_sigTick(&s_manager);
        pifTaskManager_Loop();
    }
}

static BOOL _begin(void)
{
    ms5611Emu_Reset(&s_emu);
    s_reads = 0;
    s_timer_reads = 0;
    s_timer_result = FALSE;
    return pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL);
}

static void _end(void)
{
    pifMs5611_Clear(&s_baro);
}

static void _testInitParams(void)
{
    PifMs5611 baro;

    ms5611Emu_Reset(&s_emu);
    CHECK(!pifMs5611_Init(NULL, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifMs5611_Init(&baro, PIF_ID_AUTO, NULL, MS5611_ADDR, NULL));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
}

static void _testInit(void)
{
    int i;

    CHECK(_begin());
    // The device is reset once, and all eight words of the PROM are read.
    CHECK_EQ(s_emu.resets, 1);
    CHECK_EQ(s_emu.prom_reads, 8);
    for (i = 0; i < 8; i++) CHECK_EQ(s_baro._prom[i], s_ms5611_emu_prom[i]);
    // Nothing is converted yet, and the oversampling starts at the lowest ratio.
    CHECK_EQ(s_emu.d1_convs, 0);
    CHECK_EQ(s_emu.d2_convs, 0);
    CHECK_EQ(s_baro._over_sampling_rate, MS5611_OSR_256);
    CHECK_EQ(s_baro._conversion_time, 2);
    CHECK_EQ(s_baro._state, MS5611_STATE_IDLE);
    CHECK(s_baro._id != PIF_ID_AUTO);
    CHECK(!s_baro._p_task);
    _end();

    // The device is off the port, so another one fits in its place.
    CHECK(_begin());
    _end();
}

static void _testInitChecksProm(void)
{
    // The example sequence of the datasheet, whose documented CRC-4 is 0xB. The CRC is taken over the low
    // byte of the last word as 0, so only the nibble it is held in tells the right one from the wrong ones.
    static const uint16_t example[8] = { 0x3132, 0x3334, 0x3536, 0x3738, 0x3940, 0x4142, 0x4344, 0x4540 };
    int i;

    for (i = 0; i < 16; i++) {
        ms5611Emu_Reset(&s_emu);
        memcpy(s_emu.prom, example, sizeof(example));
        s_emu.prom[7] = (uint16_t)(example[7] | i);
        if (i == 0xB) {
            CHECK(pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));
            _end();
        }
        else {
            CHECK(!pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));
            CHECK_EQ(pif_error, E_MISMATCH_CRC);
        }
    }

    // A bus that reads as zeros passes the CRC, and is still no device.
    ms5611Emu_Reset(&s_emu);
    memset(s_emu.prom, 0, sizeof(s_emu.prom));
    CHECK(!pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));
    CHECK_EQ(pif_error, E_WRONG_DATA);

    // A device that stops answering during the PROM read.
    ms5611Emu_Reset(&s_emu);
    s_emu.fail_in = 4;
    CHECK(!pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));

    // One that never answers at all.
    ms5611Emu_Reset(&s_emu);
    s_emu.fail_in = 0;
    CHECK(!pifMs5611_Init(&s_baro, PIF_ID_AUTO, &s_port, MS5611_ADDR, NULL));
    CHECK_EQ(s_emu.resets, 0);
}

static void _testOverSamplingRate(void)
{
    static const struct { uint16_t osr; uint8_t time; } table[] = {
        { MS5611_OSR_256, 2 }, { MS5611_OSR_512, 3 }, { MS5611_OSR_1024, 4 },
        { MS5611_OSR_2048, 6 }, { MS5611_OSR_4096, 10 }
    };
    size_t t;
    uint16_t wait1ms;

    CHECK(_begin());
    for (t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        pifMs5611_SetOverSamplingRate(&s_baro, table[t].osr);
        CHECK_EQ(s_baro._over_sampling_rate, table[t].osr);
        CHECK_EQ(s_baro._conversion_time, table[t].time);

        // The ratio goes out as part of the conversion command, and the wait is the time of that ratio.
        s_baro._state = MS5611_STATE_TEMPERATURE_START;
        CHECK(_stepReading(&s_baro, &wait1ms));
        CHECK_EQ(s_emu.conv_cmd, MS5611_REG_CONV_D2 + table[t].osr);
        CHECK_EQ(wait1ms, table[t].time);

        s_baro._state = MS5611_STATE_PRESSURE_START;
        CHECK(_stepReading(&s_baro, &wait1ms));
        CHECK_EQ(s_emu.conv_cmd, MS5611_REG_CONV_D1 + table[t].osr);
        CHECK_EQ(wait1ms, table[t].time);
    }
    _end();
}

static void _testTaskReading(void)
{
    CHECK(_begin());
    CHECK(pifMs5611_AttachTaskForReading(&s_baro, PIF_ID_AUTO, 50, _evtRead, FALSE, NULL));
    CHECK(s_baro._p_task && !strcmp(s_baro._p_task->name, "MS5611"));
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_START);

    // The temperature goes first, and the device is left for its conversion time.
    CHECK_EQ(_doTask(s_baro._p_task), 2 * 1000UL);
    CHECK_EQ(s_emu.d2_convs, 1);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_WAIT);

    // Reading it out needs nothing left for it, so the pressure conversion follows at once.
    CHECK_EQ(_doTask(s_baro._p_task), 1UL);
    CHECK_EQ(s_emu.adc_reads, 1);
    CHECK_EQ(s_baro.__D2, MS5611_EMU_D2);
    CHECK_EQ(s_baro._state, MS5611_STATE_PRESSURE_START);

    CHECK_EQ(_doTask(s_baro._p_task), 2 * 1000UL);
    CHECK_EQ(s_emu.d1_convs, 1);
    CHECK_EQ(s_baro._state, MS5611_STATE_PRESSURE_WAIT);

    CHECK_EQ(_doTask(s_baro._p_task), 1UL);
    CHECK_EQ(s_emu.adc_reads, 2);
    CHECK_EQ(s_baro.__D1, MS5611_EMU_D1);
    CHECK_EQ(s_baro._state, MS5611_STATE_CALCURATE);

    // The compensation of the two, which is the worked example of the datasheet.
    CHECK_EQ(s_reads, 0);
    (void)_doTask(s_baro._p_task);
    CHECK_EQ(s_reads, 1);
    CHECK_NEAR(s_temperature, MS5611_EMU_TEMPERATURE);
    CHECK_NEAR(s_pressure, MS5611_EMU_PRESSURE);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_START);
    _end();
    CHECK(!s_baro._p_task);
}

static void _testTaskKeepsTheReadPeriod(void)
{
    int i;

    CHECK(_begin());
    CHECK(pifMs5611_AttachTaskForReading(&s_baro, PIF_ID_AUTO, 50, _evtRead, FALSE, NULL));

    // A reading that took 10 of the 50ms leaves 40 before the next one.
    pif_cumulative_timer1ms = 1000;
    for (i = 0; i < 4; i++) (void)_doTask(s_baro._p_task);
    pif_cumulative_timer1ms = 1010;
    CHECK_EQ(_doTask(s_baro._p_task), 40 * 1000UL);
    CHECK_EQ(s_reads, 1);

    // One that overran the period starts the next without waiting.
    for (i = 0; i < 4; i++) (void)_doTask(s_baro._p_task);
    pif_cumulative_timer1ms += 60;
    CHECK_EQ(_doTask(s_baro._p_task), 1UL);
    CHECK_EQ(s_reads, 2);
    _end();
}

static void _testTaskTransferFails(void)
{
    CHECK(_begin());
    CHECK(pifMs5611_AttachTaskForReading(&s_baro, PIF_ID_AUTO, 50, _evtRead, FALSE, NULL));

    // A step whose transfer failed waits and is taken again, and the reading does not move on.
    s_emu.fail_in = 0;
    CHECK_EQ(_doTask(s_baro._p_task), 100000UL);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_START);
    CHECK_EQ(s_reads, 0);

    s_emu.fail_in = -1;
    CHECK_EQ(_doTask(s_baro._p_task), 2 * 1000UL);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_WAIT);

    // The read of the conversion fails this time.
    s_emu.fail_in = 0;
    CHECK_EQ(_doTask(s_baro._p_task), 100000UL);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_WAIT);
    _end();
}

static void _testTimerReading(void)
{
    CHECK(_begin());
    CHECK(pifMs5611_AttachTimer(&s_baro, &s_manager, _evtTimerRead));
    pifMs5611_SetOverSamplingRate(&s_baro, MS5611_OSR_4096);

    CHECK(pifMs5611_StartBarometric(&s_baro));
    CHECK_EQ(s_emu.d2_convs, 1);
    CHECK_EQ(s_baro._state, MS5611_STATE_TEMPERATURE_WAIT);

    // The conversion time of the ratio, 10ms, is what the timer is given, in ticks of a millisecond each.
    _tick(9);
    CHECK_EQ(s_timer_reads, 0);
    CHECK_EQ(s_emu.d1_convs, 0);
    _tick(1);
    // The pressure conversion went out from the callback, and is being waited out in its turn.
    CHECK_EQ(s_emu.d1_convs, 1);
    CHECK_EQ(s_timer_reads, 0);
    CHECK_EQ(s_baro._state, MS5611_STATE_PRESSURE_WAIT);

    _tick(10);
    CHECK_EQ(s_timer_reads, 1);
    CHECK(s_timer_result);
    CHECK_NEAR(s_timer_temperature, MS5611_EMU_TEMPERATURE);
    CHECK_NEAR(s_timer_pressure, MS5611_EMU_PRESSURE);
    // A reading of its own, so the instance is idle again and takes the next one.
    CHECK_EQ(s_baro._state, MS5611_STATE_IDLE);
    CHECK(pifMs5611_StartBarometric(&s_baro));
    _tick(20);
    CHECK_EQ(s_timer_reads, 2);
    _end();
}

static void _testTimerOneAtATime(void)
{
    CHECK(_begin());
    // Without a timer there is nothing to wait with.
    CHECK(!pifMs5611_StartBarometric(&s_baro));
    CHECK_EQ(pif_error, E_CANNOT_FOUND);

    CHECK(pifMs5611_AttachTimer(&s_baro, &s_manager, _evtTimerRead));
    // One timer, one pair of raw values: a second one has to wait for the first.
    CHECK(!pifMs5611_AttachTimer(&s_baro, &s_manager, _evtTimerRead));
    CHECK_EQ(pif_error, E_INVALID_STATE);

    CHECK(pifMs5611_StartBarometric(&s_baro));
    CHECK(!pifMs5611_StartBarometric(&s_baro));
    CHECK_EQ(pif_error, E_INVALID_STATE);

    // The reading on its way is abandoned with the timer, and its callback never comes.
    pifMs5611_DetachTimer(&s_baro);
    CHECK_EQ(s_baro._state, MS5611_STATE_IDLE);
    _tick(20);
    CHECK_EQ(s_timer_reads, 0);
    _end();
}

static void _testTimerTransferFails(void)
{
    CHECK(_begin());
    CHECK(pifMs5611_AttachTimer(&s_baro, &s_manager, _evtTimerRead));

    // The conversion command of the first step fails, so no reading starts.
    s_emu.fail_in = 0;
    CHECK(!pifMs5611_StartBarometric(&s_baro));
    CHECK_EQ(s_baro._state, MS5611_STATE_IDLE);
    CHECK_EQ(s_timer_reads, 0);

    // One that fails halfway is reported, without values, so a caller waiting on it is not left waiting.
    s_emu.fail_in = -1;
    CHECK(pifMs5611_StartBarometric(&s_baro));
    s_emu.fail_in = 0;
    _tick(2);
    CHECK_EQ(s_timer_reads, 1);
    CHECK(!s_timer_result);
    CHECK_EQ(s_baro._state, MS5611_STATE_IDLE);
    _end();
}

static void _testSecondOrderCompensation(void)
{
    static const struct { uint32_t d2; float temperature; float pressure; } table[] = {
        { MS5611_EMU_D2, MS5611_EMU_TEMPERATURE, MS5611_EMU_PRESSURE },	// 20.07 degrees C, first order only
        { 8000000UL, -0.62f, 959.89f },		// Below 20 degrees C: the second order correction
        { 6000000UL, -97.31f, 629.59f }		// Below -15 as well: the further one on top of it
    };
    size_t t;
    int i;

    for (t = 0; t < sizeof(table) / sizeof(table[0]); t++) {
        CHECK(_begin());
        s_emu.d2 = table[t].d2;
        CHECK(pifMs5611_AttachTaskForReading(&s_baro, PIF_ID_AUTO, 50, _evtRead, FALSE, NULL));
        for (i = 0; i < 5; i++) (void)_doTask(s_baro._p_task);
        CHECK_EQ(s_reads, 1);
        CHECK_NEAR(s_temperature, table[t].temperature);
        CHECK_NEAR(s_pressure, table[t].pressure);
        _end();
    }
}

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInitParams, "init parameters" },
    { _testInit, "init" },
    { _testInitChecksProm, "init checks the PROM" },
    { _testOverSamplingRate, "oversampling ratio" },
    { _testTaskReading, "reading by task" },
    { _testTaskKeepsTheReadPeriod, "the task keeps the read period" },
    { _testTaskTransferFails, "a transfer of the task fails" },
    { _testTimerReading, "reading by timer" },
    { _testTimerOneAtATime, "one reading at a time" },
    { _testTimerTransferFails, "a transfer of the timer fails" },
    { _testSecondOrderCompensation, "second order compensation" },
};

static BOOL _portSetup(void)
{
    // One task for the reading task of the driver, and one task timer for the timer manager.
    return pifTaskManager_Init(1, 1);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_ms5611: setup failed (%d)\n", pif_error);
        return 1;
    }
    if (!pifI2cPort_Init(&s_port, PIF_ID_AUTO, 1)) {
        printf("pif_ms5611: port failed (%d)\n", pif_error);
        return 1;
    }
    s_port.act_read = _actRead;
    s_port.act_write = _actWrite;
    if (!pifTimerManager_Init(&s_manager, PIF_ID_AUTO, 1000, 2)) {
        printf("pif_ms5611: timer failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_ms5611\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        // A test that gave up in the middle left its device on the port, which only takes one, and a
        // timer it gave back is only collected by a pass of the loop.
        pifMs5611_Clear(&s_baro);
        pifTaskManager_Loop();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifTimerManager_Clear(&s_manager);
    pifI2cPort_Clear(&s_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
