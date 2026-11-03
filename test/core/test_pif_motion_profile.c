// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_motion_profile.h"

#include <math.h>
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

#define NEAR(A, B)	(fabsf((A) - (B)) <= 1e-4f * (1.0f + fabsf(B)))

static BOOL s_failed;


// Samples a move densely and checks that it is continuous and within the limits, and that it ends at the target
// at rest.
static BOOL _isSmooth(PifMotionProfile *p_profile, float v_max, float a_max, float d_max, float target)
{
    const float dt = 1e-4f;
    float duration = pifMotionProfile_GetDuration(p_profile);
    float t, position, velocity;
    float limit = (a_max > d_max ? a_max : d_max) * dt;

    pifMotionProfile_Sample(p_profile, 0.0f);
    position = p_profile->_position;
    velocity = p_profile->_velocity;
    for (t = dt; t < duration + 2 * dt; t += dt) {
        pifMotionProfile_Sample(p_profile, t);
        if (fabsf(p_profile->_velocity - velocity) > limit * 1.01f + 1e-5f) return FALSE;
        if (fabsf(p_profile->_position - position) > (fabsf(velocity) + limit) * dt * 1.01f + 1e-5f) return FALSE;
        if (fabsf(p_profile->_velocity) > v_max * 1.0001f && fabsf(p_profile->_velocity) > fabsf(velocity)) return FALSE;
        position = p_profile->_position;
        velocity = p_profile->_velocity;
    }
    return p_profile->_done && p_profile->_position == target && p_profile->_velocity == 0.0f;
}

// 0 to 10 at 10/s, 20/s^2: 0.5 s up over 2.5, 0.5 s at 10 over 5 and 0.5 s down over 2.5.
static void _testTrapezoid(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    CHECK(profile._done && profile._position == 0.0f);
    pifMotionProfile_Move(&profile, 10.0f);
    CHECK(!profile._done);
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 1.5f));

    pifMotionProfile_Sample(&profile, 0.25f);
    CHECK(NEAR(profile._position, 0.625f) && NEAR(profile._velocity, 5.0f) && profile._acceleration == 20.0f);
    pifMotionProfile_Sample(&profile, 0.5f);
    CHECK(NEAR(profile._position, 2.5f) && NEAR(profile._velocity, 10.0f));
    pifMotionProfile_Sample(&profile, 0.75f);
    CHECK(NEAR(profile._position, 5.0f) && NEAR(profile._velocity, 10.0f) && profile._acceleration == 0.0f);
    pifMotionProfile_Sample(&profile, 1.25f);
    CHECK(NEAR(profile._position, 9.375f) && NEAR(profile._velocity, 5.0f) && profile._acceleration == -20.0f);
    pifMotionProfile_Sample(&profile, 1.5f);
    CHECK(profile._done && profile._position == 10.0f && profile._velocity == 0.0f);
    pifMotionProfile_Sample(&profile, 9.0f);
    CHECK(profile._done && profile._position == 10.0f);

    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 10.0f));
}

// 0 to 1 at 20/s^2 never reaches 10/s: up to sqrt(20) and down, sqrt(0.05) s each.
static void _testTriangle(void)
{
    PifMotionProfile profile;
    float half = sqrtf(0.05f);

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 1.0f);
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 2.0f * half));
    pifMotionProfile_Sample(&profile, half);
    CHECK(NEAR(profile._position, 0.5f) && NEAR(profile._velocity, sqrtf(20.0f)));
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 1.0f));
}

// Down at 40/s^2 and up at 10/s^2, toward negative positions.
static void _testAsymmetricNegative(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 10.0f, 40.0f));
    pifMotionProfile_SetPosition(&profile, 5.0f);
    pifMotionProfile_Move(&profile, -15.0f);
    // 1 s up over 5, 0.25 s down over 1.25 and 13.75 at 10/s.
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 1.0f + 1.375f + 0.25f));
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(NEAR(profile._position, 0.0f) && NEAR(profile._velocity, -10.0f));
    CHECK(_isSmooth(&profile, 10.0f, 10.0f, 40.0f, -15.0f));
}

// A move to where it is stays at rest.
static void _testNoMove(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_SetPosition(&profile, 3.0f);
    pifMotionProfile_Move(&profile, 3.0f);
    CHECK(profile._done && pifMotionProfile_GetDuration(&profile) == 0.0f);
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(profile._position == 3.0f && profile._velocity == 0.0f);
    CHECK(pifMotionProfile_TimeAtPosition(&profile, 3.0f) == 0.0f);
    CHECK(pifMotionProfile_TimeAtPosition(&profile, 4.0f) < 0.0f);
}

// At 5 and 10/s toward 10, turned back to 0: 0.5 s of braking to 7.5, then from rest down to 0.
static void _testReverse(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 10.0f);
    pifMotionProfile_Sample(&profile, 0.75f);
    pifMotionProfile_Move(&profile, 0.0f);
    pifMotionProfile_Sample(&profile, 0.0f);
    CHECK(NEAR(profile._position, 5.0f) && NEAR(profile._velocity, 10.0f) && profile._acceleration == -20.0f);
    pifMotionProfile_Sample(&profile, 0.5f);
    CHECK(NEAR(profile._position, 7.5f) && NEAR(profile._velocity, 0.0f));
    // 7.5 from rest: 0.5 s up, 0.25 s at 10/s, 0.5 s down.
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 0.5f + 1.25f));
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 0.0f));
}

// At 5 and 10/s, a target 1 ahead is within the 2.5 it takes to stop: past it to 7.5 and back.
static void _testOvershoot(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 10.0f);
    pifMotionProfile_Sample(&profile, 0.75f);
    pifMotionProfile_Move(&profile, 6.0f);
    pifMotionProfile_Sample(&profile, 0.5f);
    CHECK(NEAR(profile._position, 7.5f) && NEAR(profile._velocity, 0.0f));
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 6.0f));
}

// A target further on keeps going without slowing down.
static void _testExtend(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 10.0f);
    pifMotionProfile_Sample(&profile, 0.75f);
    pifMotionProfile_Move(&profile, 20.0f);
    // 15 at 10/s: 1.25 s at it and 0.5 s down.
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 1.75f));
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(NEAR(profile._position, 15.0f) && NEAR(profile._velocity, 10.0f));
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 20.0f));
}

// A lower v_max set during a move slows the next move down to it at d_max.
static void _testLowerLimit(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 100.0f);
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(pifMotionProfile_SetLimits(&profile, 5.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 100.0f);
    pifMotionProfile_Sample(&profile, 0.25f);
    CHECK(NEAR(profile._velocity, 5.0f));
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(NEAR(profile._velocity, 5.0f) && profile._acceleration == 0.0f);
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, 100.0f));
}

// A stop at 10/s brakes over 2.5 in 0.5 s.
static void _testStop(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, -100.0f);
    pifMotionProfile_Sample(&profile, 1.0f);
    CHECK(NEAR(profile._position, -7.5f));
    pifMotionProfile_Stop(&profile);
    CHECK(NEAR(pifMotionProfile_GetDuration(&profile), 0.5f));
    CHECK(_isSmooth(&profile, 10.0f, 20.0f, 20.0f, profile.__target));
    CHECK(NEAR(profile._position, -10.0f));

    // At rest, a stop is no move.
    pifMotionProfile_Stop(&profile);
    CHECK(profile._done && pifMotionProfile_GetDuration(&profile) == 0.0f);
}

// The time of each position is the inverse of the sample, as for the pulses of a step motor.
static void _testTimeAtPosition(void)
{
    PifMotionProfile profile;
    float t;
    int n;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pifMotionProfile_Move(&profile, 10.0f);
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 0.0f), 0.0f));
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 0.625f), 0.25f));
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 2.5f), 0.5f));
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 5.0f), 0.75f));
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 10.0f), 1.5f));
    CHECK(pifMotionProfile_TimeAtPosition(&profile, 10.5f) < 0.0f);
    CHECK(pifMotionProfile_TimeAtPosition(&profile, -0.5f) < 0.0f);

    // The times of steps of 0.01 rise and land on their positions.
    t = 0.0f;
    for (n = 1; n <= 1000; n++) {
        float next = pifMotionProfile_TimeAtPosition(&profile, n * 0.01f);

        CHECK(next > t);
        pifMotionProfile_Sample(&profile, next);
        CHECK(fabsf(profile._position - n * 0.01f) < 1e-4f);
        t = next;
    }

    // After a reverse, the first time at a position on the way out comes before the one on the way back.
    pifMotionProfile_Sample(&profile, 0.75f);
    pifMotionProfile_Move(&profile, 0.0f);
    t = pifMotionProfile_TimeAtPosition(&profile, 6.0f);
    CHECK(t > 0.0f && t < 0.5f);
    CHECK(NEAR(pifMotionProfile_TimeAtPosition(&profile, 7.5f), 0.5f));
    CHECK(pifMotionProfile_TimeAtPosition(&profile, 8.0f) < 0.0f);
}

static void _testInvalid(void)
{
    PifMotionProfile profile;

    CHECK(pifMotionProfile_Init(&profile, 10.0f, 20.0f, 20.0f));
    pif_error = E_SUCCESS;
    CHECK(!pifMotionProfile_Init(&profile, 0.0f, 20.0f, 20.0f) && pif_error == E_INVALID_PARAM);
    CHECK(!pifMotionProfile_SetLimits(&profile, 10.0f, -1.0f, 20.0f));
    CHECK(!pifMotionProfile_SetLimits(&profile, 10.0f, 20.0f, NAN));
}

int main(void)
{
    static const struct {
        const char* p_name;
        void (*test)(void);
    } kTests[] = {
        { "trapezoid", _testTrapezoid },
        { "triangle", _testTriangle },
        { "asymmetric negative", _testAsymmetricNegative },
        { "no move", _testNoMove },
        { "reverse", _testReverse },
        { "overshoot", _testOvershoot },
        { "extend", _testExtend },
        { "lower limit", _testLowerLimit },
        { "stop", _testStop },
        { "time at position", _testTimeAtPosition },
        { "invalid", _testInvalid },
    };
    unsigned i, failed = 0;

    printf("test_pif_motion_profile\n");
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%u failed\n", failed);
    return failed ? 1 : 0;
}
