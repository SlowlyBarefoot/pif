// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_quad_encoder.h"

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

#define NEAR(A, B)	(fabsf((A) - (B)) < 0.01f)

static BOOL s_failed;
static uint32_t s_now;
static int s_phase;			// Index into kForward of the state given last
static int s_step_events;
static int s_last_direction;

// A leading B.
static const uint8_t kForward[4] = { 0, QUAD_ENCODER_PHASE_A, QUAD_ENCODER_PHASE_A | QUAD_ENCODER_PHASE_B, QUAD_ENCODER_PHASE_B };


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _evtStep(int8_t direction, PifIssuerP p_issuer)
{
    (void)p_issuer;
    s_step_events++;
    s_last_direction = direction;
}

// Moves `steps` quarter steps (negative to go back), each `interval` us after the one before.
static void _move(PifQuadEncoder *p_encoder, int steps, uint32_t interval)
{
    int dir = steps < 0 ? -1 : 1;

    while (steps) {
        s_phase = (s_phase + dir) & 3;
        s_now += interval;
        pifQuadEncoder_sigState(p_encoder, kForward[s_phase], s_now);
        steps -= dir;
    }
}

static void _setUp(PifQuadEncoder *p_encoder, PifQuadEncoderResolution resolution)
{
    s_now = 1000;
    s_phase = 0;
    s_step_events = 0;
    s_last_direction = 0;
    pif_error = E_SUCCESS;
    pifQuadEncoder_Init(p_encoder, PIF_ID_AUTO, resolution);
    // The first state only sets where the encoder stands.
    pifQuadEncoder_sigState(p_encoder, kForward[0], s_now);
}

static void testInit(void)
{
    PifQuadEncoder encoder;
    float speed = 1.0f;

    CHECK(!pifQuadEncoder_Init(NULL, PIF_ID_AUTO, QUAD_ENCODER_RES_X4) && pif_error == E_INVALID_PARAM);
    CHECK(!pifQuadEncoder_Init(&encoder, PIF_ID_AUTO, (PifQuadEncoderResolution)3));
    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 0 && pifQuadEncoder_GetDirection(&encoder) == 0);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, &speed) == QUAD_ENCODER_R_NO_DATA && speed == 0.0f);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, NULL) == QUAD_ENCODER_R_NO_DATA);
}

static void testPositionX4(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    pifQuadEncoder_AttachEvtStep(&encoder, _evtStep, NULL);
    _move(&encoder, 10, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 10 && pifQuadEncoder_GetDirection(&encoder) == 1);
    CHECK(s_step_events == 10 && s_last_direction == 1);
    _move(&encoder, -13, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -3 && pifQuadEncoder_GetDirection(&encoder) == -1);
    CHECK(s_step_events == 23 && s_last_direction == -1);
    // The same state again is no step.
    CHECK(!pifQuadEncoder_sigState(&encoder, kForward[s_phase], s_now));
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -3 && pifQuadEncoder_GetErrorCount(&encoder) == 0);
}

static void testResolution(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X2);
    _move(&encoder, 9, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 4);

    _setUp(&encoder, QUAD_ENCODER_RES_X1);
    pifQuadEncoder_AttachEvtStep(&encoder, _evtStep, NULL);
    _move(&encoder, 7, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 1 && s_step_events == 1);
    _move(&encoder, 1, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 2 && s_step_events == 2);

    // Counts change at the same place in both directions, also below 0.
    _setUp(&encoder, QUAD_ENCODER_RES_X1);
    pifQuadEncoder_AttachEvtStep(&encoder, _evtStep, NULL);
    _move(&encoder, -1, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -1 && s_step_events == 1 && s_last_direction == -1);
    _move(&encoder, -3, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -1 && s_step_events == 1);
    _move(&encoder, -1, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -2);
    _move(&encoder, 5, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 0 && s_last_direction == 1);
}

static void testSetPosition(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X1);
    _move(&encoder, 6, 100);
    pifQuadEncoder_SetPosition(&encoder, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 100);
    // The count still changes at the same place of the cycle.
    _move(&encoder, 2, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 101);
    _move(&encoder, -3, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 100);
    pifQuadEncoder_SetPosition(&encoder, -50);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -50);
}

static void testReverse(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    pifQuadEncoder_SetReverse(&encoder, TRUE);
    _move(&encoder, 5, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == -5 && pifQuadEncoder_GetDirection(&encoder) == -1);
    CHECK(pifQuadEncoder_GetSpeed(&encoder) < 0.0f);
}

static void testError(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    _move(&encoder, 6, 100);
    // Skipping a state changes both phases at once.
    s_phase = (s_phase + 2) & 3;
    s_now += 100;
    CHECK(!pifQuadEncoder_sigState(&encoder, kForward[s_phase], s_now));
    CHECK(pifQuadEncoder_GetErrorCount(&encoder) == 1 && pifQuadEncoder_GetPosition(&encoder) == 6);
    // The speed starts over after the lost step.
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, NULL) == QUAD_ENCODER_R_NO_DATA);
    _move(&encoder, 2, 100);
    CHECK(pifQuadEncoder_GetPosition(&encoder) == 8 && pifQuadEncoder_ReadSpeed(&encoder, NULL) == QUAD_ENCODER_R_OK);
}

static void testSpeed(void)
{
    PifQuadEncoder encoder;
    float speed;
    int i;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    _move(&encoder, 1, 250);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, &speed) == QUAD_ENCODER_R_NO_DATA);
    _move(&encoder, 1, 250);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, &speed) == QUAD_ENCODER_R_OK && NEAR(speed, 4000.0f));

    // Uneven quarter steps average out over whole cycles.
    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    for (i = 0; i < 4; i++) {
        _move(&encoder, 1, 150);
        _move(&encoder, 1, 350);
    }
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 4000.0f));

    _setUp(&encoder, QUAD_ENCODER_RES_X1);
    _move(&encoder, -12, 250);
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), -1000.0f));
}

static void testSlowDown(void)
{
    PifQuadEncoder encoder;
    float speed;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    pifQuadEncoder_SetTimeout(&encoder, 100000UL);
    _move(&encoder, 8, 250);
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 4000.0f));

    // Until the next step the speed is at most what the time since the last one allows.
    s_now += 200;
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 4000.0f));
    s_now += 800;
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 1000.0f));
    s_now += 99000UL;
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, &speed) == QUAD_ENCODER_R_OK && NEAR(speed, 10.0f));
    s_now += 1;
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, &speed) == QUAD_ENCODER_R_TIMEOUT && speed == 0.0f);

    // The first step after the stop does not span the gap.
    _move(&encoder, 1, 1000000UL);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, NULL) == QUAD_ENCODER_R_NO_DATA);
    _move(&encoder, 1, 500);
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 2000.0f));
}

static void testDirectionChange(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    _move(&encoder, 8, 250);
    _move(&encoder, -1, 250);
    CHECK(pifQuadEncoder_ReadSpeed(&encoder, NULL) == QUAD_ENCODER_R_NO_DATA);
    _move(&encoder, -1, 500);
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), -2000.0f));
}

static void testWrapAround(void)
{
    PifQuadEncoder encoder;

    _setUp(&encoder, QUAD_ENCODER_RES_X4);
    s_now = 0xFFFFFF00UL;
    _move(&encoder, 8, 100);
    CHECK(s_now < 0x1000);
    CHECK(NEAR(pifQuadEncoder_GetSpeed(&encoder), 10000.0f));
}

static BOOL _portSetup(void)
{
    return TRUE;
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "init", testInit },
        { "position x4", testPositionX4 },
        { "resolution", testResolution },
        { "set position", testSetPosition },
        { "reverse", testReverse },
        { "error", testError },
        { "speed", testSpeed },
        { "slow down", testSlowDown },
        { "direction change", testDirectionChange },
        { "wrap around", testWrapAround },
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_quad_encoder: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_quad_encoder\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
