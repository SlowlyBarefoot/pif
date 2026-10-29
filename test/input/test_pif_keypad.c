// SPDX-License-Identifier: BSD-3-Clause
// Includes pif_keypad.c itself to run the state machine of a key without the scheduler.
#include "../../source/input/pif_keypad.c"

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

#define SCAN_MS     20      // PIF_KEYPAD_CONTROL_PERIOD

static BOOL s_failed;
static PifKeypad s_keypad;
static char s_log[512];     // the events in order: P, R<ms>, L, D, r<count>@<ms>
static int s_len;


static void _log(const char* p_text)
{
    s_len += sprintf(s_log + s_len, "%s%s", s_len ? " " : "", p_text);
}

static void _evtPressed(char ch)
{
    _log("P");
}

static void _evtReleased(char ch, uint32_t on_time)
{
    char text[16];

    sprintf(text, "R%lu", (unsigned long)on_time);
    _log(text);
}

static void _evtLongPressed(char ch, uint32_t on_time)
{
    _log("L");
}

static void _evtDoublePressed(char ch)
{
    _log("D");
}

static void _evtRepeated(char ch, uint16_t count)
{
    char text[24];

    sprintf(text, "r%u@%lu", count, (unsigned long)pif_cumulative_timer1ms);
    _log(text);
}

// Holds the key for hold ms and releases it for 400 ms, a scan every SCAN_MS from time 0, and gives the events.
static const char* _press(uint32_t hold)
{
    uint32_t t;

    s_len = 0;
    s_log[0] = 0;
    pif_cumulative_timer1ms = 0;
    for (t = 0; t < hold; t += SCAN_MS) {
        _checkKeyState(&s_keypad, 0, ON);
        pif_cumulative_timer1ms += SCAN_MS;
    }
    for (t = 0; t < 400; t += SCAN_MS) {
        _checkKeyState(&s_keypad, 0, OFF);
        pif_cumulative_timer1ms += SCAN_MS;
    }
    return s_log;
}

static void _setUp(void)
{
    static const char kKeymap[] = "U";

    pifKeypad_Clear(&s_keypad);
    pifKeypad_Init(&s_keypad, PIF_ID_AUTO, NULL);
    pifKeypad_SetKeymap(&s_keypad, 1, kKeymap);
    s_keypad.evt_pressed = _evtPressed;
    s_keypad.evt_released = _evtReleased;
    s_keypad.evt_long_pressed = _evtLongPressed;
}

// Without evt_repeated a held key raises what it did before.
static void _testNoRepeat(void)
{
    _setUp();
    CHECK(s_keypad._repeat_delay1ms == PIF_KEYPAD_DEFAULT_REPEAT_DELAY);
    CHECK(s_keypad._repeat_period1ms == PIF_KEYPAD_DEFAULT_REPEAT_PERIOD);
    CHECK(!strcmp(_press(1200), "P L"));
    CHECK(!strcmp(_press(200), "P R220"));
}

// From the delay on, every period, counting from 1, and next to the long press.
static void _testRepeat(void)
{
    _setUp();
    s_keypad.evt_repeated = _evtRepeated;
    CHECK(!strcmp(_press(200), "P R220"));
    CHECK(!strcmp(_press(480), "P R500"));       // released before the delay
    CHECK(!strcmp(_press(1200), "P r1@500 r2@600 r3@700 r4@800 r5@900 L r6@1000 r7@1100"));
    CHECK(!strcmp(_press(620), "P r1@500 r2@600 R640"));     // the count starts again with a press
}

// A period shorter than the scan gives one repeat a scan, without a burst.
static void _testShortPeriod(void)
{
    _setUp();
    s_keypad.evt_repeated = _evtRepeated;
    CHECK(pifKeypad_SetRepeatTime(&s_keypad, 300, 10));
    CHECK(!strcmp(_press(400), "P r1@300 r2@320 r3@340 r4@360 r5@380 R420"));
}

// With evt_double_pressed a held key repeats as well, and a double press does not.
static void _testDoublePress(void)
{
    _setUp();
    s_keypad.evt_repeated = _evtRepeated;
    s_keypad.evt_double_pressed = _evtDoublePressed;
    CHECK(!strcmp(_press(800), "P r1@500 r2@600 r3@700 R820"));
}

// The delay is not less than the double press time, so a key that repeats has had its evt_pressed.
static void _testSetRepeatTime(void)
{
    _setUp();
    CHECK(!pifKeypad_SetRepeatTime(&s_keypad, PIF_KEYPAD_DEFAULT_DOUBLE_TIME - 1, 100));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(!pifKeypad_SetRepeatTime(&s_keypad, 500, 0));
    CHECK(s_keypad._repeat_delay1ms == PIF_KEYPAD_DEFAULT_REPEAT_DELAY);
    CHECK(pifKeypad_SetRepeatTime(&s_keypad, PIF_KEYPAD_DEFAULT_DOUBLE_TIME, 50));
    CHECK(s_keypad._repeat_delay1ms == PIF_KEYPAD_DEFAULT_DOUBLE_TIME);
    CHECK(s_keypad._repeat_period1ms == 50);
    CHECK(!pifKeypad_SetDoubleTime(&s_keypad, PIF_KEYPAD_DEFAULT_DOUBLE_TIME + 1));    // past the delay
    CHECK(pifKeypad_SetDoubleTime(&s_keypad, PIF_KEYPAD_DEFAULT_DOUBLE_TIME - 1));
}

typedef struct
{
    const char* p_name;
    void (*test)(void);
} TestCase;

int main(void)
{
    static const TestCase kTests[] = {
        { "no repeat", _testNoRepeat },
        { "repeat", _testRepeat },
        { "short period", _testShortPeriod },
        { "double press", _testDoublePress },
        { "set repeat time", _testSetRepeatTime },
    };
    unsigned i, failed = 0;

    printf("test_pif_keypad\n");
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    pifKeypad_Clear(&s_keypad);
    return failed ? 1 : 0;
}
