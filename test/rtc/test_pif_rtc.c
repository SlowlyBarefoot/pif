// SPDX-License-Identifier: BSD-3-Clause
#include "rtc/pif_ds1302.h"
// Included to run the sync task directly.
#include "../../source/rtc/pif_rtc.c"

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

#define TASK_SIZE		2

#define REG_CONTROL		7
#define REG_TRICKLE		8
#define REG_COUNT		9

// DS1302 seen from its pins: it shifts in on the rising edges of SCLK and drives I/O after the falling edges.
static struct {
    uint8_t reg[REG_COUNT];		// Seconds to year, control, trickle charger
    uint8_t ram[DS1302_RAM_SIZE];
    SWITCH ce, sclk, io_mcu, io_chip;
    BOOL mcu_input;
    BOOL have_command, reading, ram_target, burst;
    uint8_t command, shift, bit, index, out_bit;
    int violations;		// SCLK high at the start, or the chip and the MCU driving I/O together
    int transfers;
} s_chip;

static BOOL s_failed;
static BOOL s_open_drain;
static uint32_t s_now;
static PifDs1302 s_rtc;
static int s_sync_count;
static int32_t s_sync_diff;
static PifIssuerP s_sync_issuer;


static uint32_t _timer1us(void)
{
    return s_now++;
}

static uint8_t _load(void)
{
    if (s_chip.ram_target) return s_chip.index < DS1302_RAM_SIZE ? s_chip.ram[s_chip.index] : 0;
    return s_chip.index < REG_COUNT ? s_chip.reg[s_chip.index] : 0;
}

static void _store(uint8_t value)
{
    BOOL protect = (s_chip.reg[REG_CONTROL] & 0x80) != 0;

    if (s_chip.ram_target) {
        if (!protect && s_chip.index < DS1302_RAM_SIZE) s_chip.ram[s_chip.index] = value;
    }
    else if (s_chip.index == REG_CONTROL) {
        s_chip.reg[REG_CONTROL] = value;
    }
    else if (!protect && s_chip.index < REG_COUNT) {
        s_chip.reg[s_chip.index] = value;
    }
}

static void _rising(void)
{
    if (!s_chip.have_command) {
        s_chip.shift |= s_chip.io_mcu << s_chip.bit;
        if (++s_chip.bit < 8) return;
        s_chip.command = s_chip.shift;
        s_chip.have_command = TRUE;
        s_chip.reading = s_chip.command & 1;
        s_chip.ram_target = (s_chip.command & 0x40) != 0;
        s_chip.index = (s_chip.command >> 1) & 0x1F;
        s_chip.burst = s_chip.index == 0x1F;
        if (s_chip.burst) s_chip.index = 0;
        s_chip.shift = 0;
        s_chip.bit = 0;
        s_chip.out_bit = 0;
    }
    else if (!s_chip.reading) {
        s_chip.shift |= s_chip.io_mcu << s_chip.bit;
        if (++s_chip.bit < 8) return;
        _store(s_chip.shift);
        if (s_chip.burst) s_chip.index++;
        s_chip.shift = 0;
        s_chip.bit = 0;
    }
}

static void _falling(void)
{
    if (!s_chip.have_command || !s_chip.reading) return;
    if (!s_open_drain && !s_chip.mcu_input) s_chip.violations++;
    s_chip.io_chip = (_load() >> s_chip.out_bit) & 1;
    if (++s_chip.out_bit == 8) {
        s_chip.out_bit = 0;
        if (s_chip.burst) s_chip.index++;
    }
}

static void _actWrite(PifDs1302Pin pin, SWITCH state)
{
    switch (pin) {
    case DS1302_PIN_CE:
        if (state && !s_chip.ce) {
            if (s_chip.sclk) s_chip.violations++;
            s_chip.have_command = FALSE;
            s_chip.shift = 0;
            s_chip.bit = 0;
            s_chip.transfers++;
        }
        if (!state) s_chip.io_chip = ON;		// Released
        s_chip.ce = state;
        break;

    case DS1302_PIN_SCLK:
        if (s_chip.ce && state && !s_chip.sclk) _rising();
        if (s_chip.ce && !state && s_chip.sclk) _falling();
        s_chip.sclk = state;
        break;

    case DS1302_PIN_IO:
        s_chip.io_mcu = state;
        break;
    }
}

static SWITCH _actRead(void)
{
    if (s_open_drain) return s_chip.io_chip && s_chip.io_mcu;		// Wired AND with a pull-up
    if (!s_chip.mcu_input) s_chip.violations++;
    return s_chip.io_chip;
}

static void _actIoMode(BOOL input)
{
    s_chip.mcu_input = input;
}

static void _evtSync(PifRtc *p_owner, int32_t diff, PifIssuerP p_issuer)
{
    (void)p_owner;

    s_sync_count++;
    s_sync_diff = diff;
    s_sync_issuer = p_issuer;
}

static void _reset(BOOL open_drain)
{
    memset(&s_chip, 0, sizeof(s_chip));
    s_chip.reg[0] = 0x80;		// Halted, as at the first power-on
    s_chip.reg[REG_CONTROL] = 0x80;
    s_chip.io_chip = ON;
    s_open_drain = open_drain;
    pifDs1302_Clear(&s_rtc);
    pifDs1302_Init(&s_rtc, PIF_ID_AUTO, _actWrite, _actRead, open_drain ? NULL : _actIoMode);
}

static void _set(PifDateTime *p_datetime, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
    uint8_t second)
{
    p_datetime->year = year;
    p_datetime->month = month;
    p_datetime->day = day;
    p_datetime->hour = hour;
    p_datetime->minute = minute;
    p_datetime->second = second;
    p_datetime->millisecond = 0;
}

static BOOL _same(const PifDateTime *p_a, const PifDateTime *p_b)
{
    return p_a->year == p_b->year && p_a->month == p_b->month && p_a->day == p_b->day && p_a->hour == p_b->hour &&
        p_a->minute == p_b->minute && p_a->second == p_b->second;
}

static void testCalendar(void)
{
    PifDateTime dt;

    CHECK(pifRtc_BcdToBin(0x59) == 59 && pifRtc_BinToBcd(59) == 0x59 && pifRtc_BinToBcd(0) == 0);

    _set(&dt, 0, 1, 1, 0, 0, 0);
    CHECK(pifRtc_ToSeconds(&dt) == 0);
    CHECK(pifRtc_GetWeekday(&dt) == 6);		// Saturday
    _set(&dt, 0, 3, 1, 0, 0, 0);
    CHECK(pifRtc_ToSeconds(&dt) == (31UL + 29) * 86400);
    _set(&dt, 1, 1, 1, 0, 0, 1);
    CHECK(pifRtc_ToSeconds(&dt) == 366UL * 86400 + 1);
    _set(&dt, 26, 11, 1, 12, 0, 0);
    CHECK(pifRtc_GetWeekday(&dt) == 0);		// Sunday
    _set(&dt, 100, 3, 1, 0, 0, 0);			// 2100 has no February 29
    CHECK(pifRtc_GetWeekday(&dt) == 1);		// Monday

    _set(&dt, 24, 2, 29, 23, 59, 59);
    CHECK(pifRtc_CheckDateTime(&dt));
    dt.year = 25;
    CHECK(!pifRtc_CheckDateTime(&dt));
    _set(&dt, 25, 12, 31, 0, 0, 0);
    CHECK(pifRtc_CheckDateTime(&dt));
    dt.day = 32;
    CHECK(!pifRtc_CheckDateTime(&dt));
    _set(&dt, 25, 4, 31, 0, 0, 0);
    CHECK(!pifRtc_CheckDateTime(&dt));
    _set(&dt, 25, 13, 1, 0, 0, 0);
    CHECK(!pifRtc_CheckDateTime(&dt));
    _set(&dt, 25, 1, 1, 24, 0, 0);
    CHECK(!pifRtc_CheckDateTime(&dt));
}

static void testWriteRead(void)
{
    PifDateTime dt, rd;
    BOOL running;

    _reset(FALSE);
    CHECK(pifDs1302_IsRunning(&s_rtc, &running) && !running);
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_GetDateTime(&s_rtc.parent, &rd) && pif_error == E_INVALID_STATE);

    _set(&dt, 26, 11, 1, 21, 34, 56);
    CHECK(pifRtc_SetDateTime(&s_rtc.parent, &dt));
    CHECK(s_chip.reg[0] == 0x56 && s_chip.reg[1] == 0x34 && s_chip.reg[2] == 0x21);
    CHECK(s_chip.reg[3] == 0x01 && s_chip.reg[4] == 0x11 && s_chip.reg[5] == 1 && s_chip.reg[6] == 0x26);
    CHECK(s_chip.reg[REG_CONTROL] == 0x80);		// Protected again
    CHECK(pifDs1302_IsRunning(&s_rtc, &running) && running);

    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd));
    CHECK(_same(&dt, &rd) && rd.millisecond == 0);
    CHECK(s_chip.violations == 0);
}

static void testTwelveHour(void)
{
    PifDateTime rd;

    _reset(FALSE);
    memcpy(s_chip.reg, "\x00\x15\xB1\x09\x03\x01\x26", 7);		// 11 PM
    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd) && rd.hour == 23 && rd.minute == 15);
    s_chip.reg[2] = 0x92;		// 12 AM
    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd) && rd.hour == 0);
    s_chip.reg[2] = 0xB2;		// 12 PM
    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd) && rd.hour == 12);
    s_chip.reg[2] = 0x81;		// 1 AM
    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd) && rd.hour == 1);
    s_chip.reg[2] = 0x80;		// 0 is not an hour of 12 hour mode
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_GetDateTime(&s_rtc.parent, &rd) && pif_error == E_WRONG_DATA);
    CHECK(s_chip.violations == 0);
}

static void testInvalid(void)
{
    PifDateTime dt, rd;
    uint8_t before[REG_COUNT];

    _reset(FALSE);
    memcpy(before, s_chip.reg, REG_COUNT);
    s_chip.transfers = 0;

    _set(&dt, 25, 2, 29, 0, 0, 0);
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_SetDateTime(&s_rtc.parent, &dt) && pif_error == E_INVALID_PARAM);
    _set(&dt, 100, 1, 1, 0, 0, 0);		// Beyond the two digits of the chip
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_SetDateTime(&s_rtc.parent, &dt) && pif_error == E_INVALID_PARAM);
    CHECK(s_chip.transfers == 0 && !memcmp(before, s_chip.reg, REG_COUNT));

    // Registers that are not BCD.
    memset(s_chip.reg, 0x7F, 7);
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_GetDateTime(&s_rtc.parent, &rd) && pif_error == E_WRONG_DATA);
    memcpy(s_chip.reg, "\x00\x00\x00\x31\x04\x01\x26", 7);		// April 31
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_GetDateTime(&s_rtc.parent, &rd) && pif_error == E_WRONG_DATA);
}

static void testRam(void)
{
    uint8_t data[DS1302_RAM_SIZE], rd[DS1302_RAM_SIZE];
    int i;

    _reset(FALSE);
    for (i = 0; i < DS1302_RAM_SIZE; i++) data[i] = (uint8_t)(0xA0 + i);
    CHECK(pifDs1302_WriteRam(&s_rtc, 0, data, DS1302_RAM_SIZE));
    CHECK(!memcmp(s_chip.ram, data, DS1302_RAM_SIZE) && s_chip.reg[REG_CONTROL] == 0x80);
    memset(rd, 0, sizeof(rd));
    CHECK(pifDs1302_ReadRam(&s_rtc, 5, rd, 3) && rd[0] == 0xA5 && rd[2] == 0xA7);
    CHECK(pifDs1302_WriteRam(&s_rtc, 30, (const uint8_t *)"\x5A", 1) && s_chip.ram[30] == 0x5A);

    pif_error = E_SUCCESS;
    CHECK(!pifDs1302_ReadRam(&s_rtc, 30, rd, 2) && pif_error == E_INVALID_PARAM);
    CHECK(!pifDs1302_WriteRam(&s_rtc, DS1302_RAM_SIZE, data, 0));

    CHECK(pifDs1302_SetTrickleCharger(&s_rtc, DS1302_TC_ENABLE | DS1302_TC_DIODE_1 | DS1302_TC_RES_2K));
    CHECK(s_chip.reg[REG_TRICKLE] == 0xA5 && s_chip.reg[REG_CONTROL] == 0x80);
    CHECK(s_chip.violations == 0);
}

static void testOpenDrain(void)
{
    PifDateTime dt, rd;
    uint8_t value;

    _reset(TRUE);
    _set(&dt, 99, 12, 31, 23, 59, 59);
    CHECK(pifRtc_SetDateTime(&s_rtc.parent, &dt));
    CHECK(pifRtc_GetDateTime(&s_rtc.parent, &rd) && _same(&dt, &rd));
    CHECK(pifDs1302_WriteRam(&s_rtc, 0, (const uint8_t *)"\x00", 1));
    CHECK(pifDs1302_ReadRam(&s_rtc, 0, &value, 1) && value == 0x00);
    CHECK(s_chip.violations == 0);
}

static void testSync(void)
{
    PifDateTime dt;
    PifTask *p_task;

    _reset(FALSE);
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_LoadDateTime(&s_rtc.parent) && pif_error == E_INVALID_STATE);

    _set(&dt, 26, 11, 1, 9, 0, 10);
    CHECK(pifRtc_SetDateTime(&s_rtc.parent, &dt));
    CHECK(pifRtc_LoadDateTime(&s_rtc.parent));
    CHECK(_same(&dt, (PifDateTime *)&pif_datetime) && pif_timer1ms == 0);

    CHECK(!pifRtc_StartSync(&s_rtc.parent, 0, NULL, NULL, NULL));
    CHECK(!pifRtc_StartSync(&s_rtc.parent, PIF_RTC_MAX_SYNC_PERIOD + 1, NULL, NULL, NULL));
    CHECK(pifRtc_StartSync(&s_rtc.parent, 60, _evtSync, &s_rtc, NULL));
    s_sync_count = 0;
    pif_error = E_SUCCESS;
    CHECK(!pifRtc_StartSync(&s_rtc.parent, 60, NULL, NULL, NULL) && pif_error == E_INVALID_STATE);
    p_task = s_rtc.parent._p_task;
    CHECK(p_task && !strcmp(p_task->name, "RTC"));

    // Same second: the milliseconds of pif_datetime stay.
    _set(&dt, 26, 11, 1, 9, 0, 10);
    dt.millisecond = 500;
    CHECK(pif_SetDateTime(&dt));
    _doTask(p_task);
    CHECK(pif_datetime.second == 10 && pif_timer1ms == 500 && s_sync_count == 0);

    // The chip is 1 second ahead: it has just ticked.
    s_chip.reg[0] = 0x11;
    _doTask(p_task);
    CHECK(pif_datetime.second == 11 && pif_timer1ms == 0);
    CHECK(s_sync_count == 1 && s_sync_diff == 1 && s_sync_issuer == &s_rtc);

    // The system clock is 1 second ahead: left alone.
    _set(&dt, 26, 11, 1, 9, 0, 12);
    dt.millisecond = 30;
    CHECK(pif_SetDateTime(&dt));
    _doTask(p_task);
    CHECK(pif_datetime.second == 12 && pif_timer1ms == 30 && s_sync_count == 1);

    // 2 seconds ahead: set back. Across midnight forward too.
    _set(&dt, 26, 11, 1, 9, 0, 13);
    CHECK(pif_SetDateTime(&dt));
    _doTask(p_task);
    CHECK(pif_datetime.second == 11 && pif_timer1ms == 0);
    CHECK(s_sync_count == 2 && s_sync_diff == -2);
    memcpy(s_chip.reg, "\x00\x00\x00\x01\x01\x05\x27", 7);
    _set(&dt, 26, 12, 31, 23, 59, 59);
    CHECK(pif_SetDateTime(&dt));
    _doTask(p_task);
    CHECK(pif_datetime.year == 27 && pif_datetime.month == 1 && pif_datetime.day == 1 && pif_datetime.second == 0);
    CHECK(s_sync_count == 3 && s_sync_diff == 1);

    pifRtc_StopSync(&s_rtc.parent);
    CHECK(!s_rtc.parent._p_task && pifTaskManager_Count() == 0);
}

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(TASK_SIZE, 1);
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "calendar", testCalendar },
        { "write and read", testWriteRead },
        { "12 hour mode", testTwelveHour },
        { "invalid", testInvalid },
        { "ram and trickle charger", testRam },
        { "open drain", testOpenDrain },
        { "sync", testSync },
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_rtc: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_rtc\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    pifDs1302_Clear(&s_rtc);
    pifTaskManager_Clear();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
