// SPDX-License-Identifier: BSD-3-Clause
// Runs pifJoystick_Process() directly, without the scheduler.
#include "input/pif_joystick.h"
#include "filter/pif_noise_filter_int16.h"

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

static BOOL s_failed;
static PifJoystick s_joystick;
static int16_t s_raw_x, s_raw_y;
static int s_move_count, s_direction_count;
static uint8_t s_direction;


static void _actRead(PifJoystick *p_owner, int16_t *p_x, int16_t *p_y)
{
    *p_x = s_raw_x;
    *p_y = s_raw_y;
}

static void _evtMove(PifJoystick *p_owner, int16_t x, int16_t y)
{
    s_move_count++;
}

static void _evtDirection(PifJoystick *p_owner, uint8_t direction)
{
    s_direction = direction;
    s_direction_count++;
}

// A 10-bit thumbstick that rests at 500 rather than half-way.
static void _setUp(void)
{
    pifJoystick_Init(&s_joystick, PIF_ID_AUTO, 0, 1023, _actRead);
    s_joystick.evt_move = _evtMove;
    s_joystick.evt_direction = _evtDirection;
    s_raw_x = s_raw_y = 500;
    pifJoystick_CalibrateCenter(&s_joystick);
    s_move_count = s_direction_count = 0;
    s_direction = JD_NONE;
}

static void _read(int16_t x, int16_t y)
{
    s_raw_x = x;
    s_raw_y = y;
    pifJoystick_Process(&s_joystick);
}

static void test_init_rejects_bad_params(void)
{
    PifJoystick joystick;

    CHECK(!pifJoystick_Init(NULL, PIF_ID_AUTO, 0, 1023, _actRead));
    CHECK(!pifJoystick_Init(&joystick, PIF_ID_AUTO, 10, 11, _actRead));
    CHECK(pifJoystick_Init(&joystick, PIF_ID_AUTO, 0, 1023, _actRead));
    CHECK(joystick._cal[JA_X]._center == 511);
    CHECK(!pifJoystick_SetAxis(&joystick, JA_X, 100, 100, 200));
    CHECK(!pifJoystick_SetAxis(&joystick, JA_COUNT, 0, 100, 200));
    CHECK(!pifJoystick_SetDeadZone(&joystick, PIF_JOYSTICK_RANGE));
}

static void test_scales_each_side_of_center(void)
{
    _setUp();
    _read(500, 500);
    CHECK(s_joystick._position[JA_X] == 0 && s_joystick._position[JA_Y] == 0);
    CHECK(s_move_count == 0);

    _read(1023, 0);
    CHECK(s_joystick._position[JA_X] == PIF_JOYSTICK_RANGE);
    CHECK(s_joystick._position[JA_Y] == -PIF_JOYSTICK_RANGE);
    CHECK(s_move_count == 1);

    _read(250, 500);    // half-way to the left of a centre at 500
    CHECK(s_joystick._position[JA_X] == -500);
}

static void test_dead_zone_and_invert(void)
{
    _setUp();
    pifJoystick_SetDeadZone(&s_joystick, 100);
    pifJoystick_SetInvert(&s_joystick, JA_Y, TRUE);

    _read(540, 460);    // 76 and -80 of 1000: inside the dead zone
    CHECK(s_joystick._position[JA_X] == 0 && s_joystick._position[JA_Y] == 0);

    _read(1023, 0);
    CHECK(s_joystick._position[JA_X] == PIF_JOYSTICK_RANGE);
    CHECK(s_joystick._position[JA_Y] == PIF_JOYSTICK_RANGE);
}

static void test_direction_with_hysteresis(void)
{
    _setUp();
    _read(1023, 1000);
    CHECK(s_direction == (JD_RIGHT | JD_UP));
    CHECK(s_direction_count == 1);

    // 400 of 1000: under DIRECTION_ON but over DIRECTION_OFF, so it keeps pointing right.
    _read(709, 500);
    CHECK(s_direction == JD_RIGHT);
    CHECK(s_direction_count == 2);

    _read(600, 500);    // 190 of 1000
    CHECK(s_direction == JD_NONE);

    _read(709, 500);    // 400 again, but from the middle it is not enough
    CHECK(s_direction == JD_NONE);
    CHECK(s_direction_count == 3);

    _read(0, 0);
    CHECK(s_direction == (JD_LEFT | JD_DOWN));
}

static void test_calibrate_center_rejects_ends(void)
{
    _setUp();
    s_raw_x = 0;
    s_raw_y = 500;
    CHECK(!pifJoystick_CalibrateCenter(&s_joystick));
    CHECK(s_joystick._cal[JA_X]._center == 500);
}

// Readings given with pifJoystick_sigData() rather than through act_read.
static void test_sig_data(void)
{
    PifJoystick joystick;

    CHECK(pifJoystick_Init(&joystick, PIF_ID_AUTO, 0, 1023, NULL));
    CHECK(!pifJoystick_CalibrateCenter(&joystick));    // nothing given yet

    pifJoystick_sigData(&joystick, 500, 500);
    CHECK(pifJoystick_CalibrateCenter(&joystick));
    CHECK(joystick._cal[JA_X]._center == 500);

    pifJoystick_sigData(&joystick, 0, 1023);
    pifJoystick_sigData(&joystick, 1023, 0);           // the last one counts
    pifJoystick_Process(&joystick);
    CHECK(joystick._position[JA_X] == PIF_JOYSTICK_RANGE);
    CHECK(joystick._position[JA_Y] == -PIF_JOYSTICK_RANGE);

    joystick._position[JA_X] = 0;                       // Process leaves it alone without a new reading
    pifJoystick_Process(&joystick);
    CHECK(joystick._position[JA_X] == 0);
}

// The switches of an arcade stick as 0, 1 and 2, debounced by a median of three.
static void test_digital_with_median(void)
{
    PifNfInt16Median filter_x, filter_y;
    int16_t buffer_x[PIF_NF_MEDIAN_BUFFER_SIZE(3)], buffer_y[PIF_NF_MEDIAN_BUFFER_SIZE(3)];
    static const int16_t kBounce[] = { 1, 2, 1, 2, 2, 2, 1, 2, 2 };   // a press of RIGHT that bounces
    unsigned i;

    CHECK(pifJoystick_Init(&s_joystick, PIF_ID_AUTO, 0, 2, _actRead));
    CHECK(s_joystick._cal[JA_X]._center == 1);
    CHECK(pifNoiseFilterInt16_InitMedian(&filter_x, buffer_x, 3));
    CHECK(pifNoiseFilterInt16_InitMedian(&filter_y, buffer_y, 3));
    CHECK(pifJoystick_AttachFilter(&s_joystick, &filter_x.parent, &filter_y.parent));
    s_joystick.evt_direction = _evtDirection;
    s_direction_count = 0;

    for (i = 0; i < sizeof(kBounce) / sizeof(kBounce[0]); i++) _read(kBounce[i], 1);
    CHECK(s_direction == JD_RIGHT);
    CHECK(s_direction_count == 1);
}

typedef struct
{
    const char *p_name;
    void (*test)(void);
} TestCase;

int main(void)
{
    static const TestCase kTests[] = {
        { "init rejects bad params", test_init_rejects_bad_params },
        { "scales each side of center", test_scales_each_side_of_center },
        { "dead zone and invert", test_dead_zone_and_invert },
        { "direction with hysteresis", test_direction_with_hysteresis },
        { "calibrate center rejects ends", test_calibrate_center_rejects_ends },
        { "sig data", test_sig_data },
        { "digital with median", test_digital_with_median },
    };
    unsigned i, failed = 0;

    printf("test_pif_joystick\n");
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    pifJoystick_Clear(&s_joystick);
    return failed ? 1 : 0;
}
