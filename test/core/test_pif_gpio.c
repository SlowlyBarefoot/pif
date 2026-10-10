// SPDX-License-Identifier: BSD-3-Clause
// Includes pif_gpio.c itself to run the polling task without the scheduler.
#include "../../source/core/pif_gpio.c"

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

#define MAX_EVENTS		8

typedef struct {
    PifGpio* p_owner;
    uint8_t index;
    SWITCH state;
} Event;

static BOOL s_failed;
static PifGpioState s_in;
static PifGpioState s_out;
static int s_out_count;
static Event s_event[MAX_EVENTS];
static int s_event_count;


static uint32_t _timer1us(void)
{
    return 0;
}

static PifGpioState _actIn(PifId id)
{
    return s_in;
}

static void _actOut(PifId id, PifGpioState state)
{
    s_out = state;
    s_out_count++;
}

static void _evtIn(PifGpio* p_owner, uint8_t index, SWITCH state)
{
    if (s_event_count < MAX_EVENTS) {
        s_event[s_event_count].p_owner = p_owner;
        s_event[s_event_count].index = index;
        s_event[s_event_count].state = state;
    }
    s_event_count++;
}

static void _setUp(void)
{
    s_in = 0;
    s_out = 0;
    s_out_count = 0;
    s_event_count = 0;
    pif_error = E_SUCCESS;
}

static void testInit(void)
{
    PifGpio gpio;

    _setUp();
    CHECK(!pifGpio_Init(&gpio, PIF_ID_AUTO, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifGpio_Init(&gpio, PIF_ID_AUTO, PIF_GPIO_MAX_COUNT + 1));
    // Init clears whatever an earlier use left behind.
    memset(&gpio, 0xA5, sizeof(gpio));
    CHECK(pifGpio_Init(&gpio, 1, PIF_GPIO_MAX_COUNT));
    CHECK(gpio._id == 1 && !gpio.evt_in && !gpio.__act_in && !gpio.__act_out);
    CHECK(gpio.__read_state == 0 && gpio.__write_state == 0);
}

// Input and output callbacks used to share a union, so a read called the output callback.
static void testInAndOut(void)
{
    PifGpio gpio;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, 3));
    pifGpio_AttachActOut(&gpio, _actOut);
    CHECK(pifGpio_ReadAll(&gpio) == 0 && pif_error == E_CANNOT_USE);
    CHECK(!pifGpio_AttachTaskIn(&gpio, PIF_ID_AUTO, TM_PERIOD, 10, FALSE));
    CHECK(s_out_count == 0);

    pifGpio_AttachActIn(&gpio, _actIn);
    s_in = 0xFF;
    CHECK(pifGpio_ReadAll(&gpio) == 0x07);
    CHECK(pifGpio_ReadCell(&gpio, 2) == ON);
    s_in = 0x03;
    CHECK(pifGpio_ReadCell(&gpio, 2) == OFF);
    CHECK(pifGpio_WriteAll(&gpio, 0xFF) && s_out == 0x07);
    CHECK(s_out_count == 1);
}

static void testWriteCell(void)
{
    PifGpio gpio;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, 4));
    CHECK(!pifGpio_WriteCell(&gpio, 0, ON) && pif_error == E_CANNOT_USE);
    pifGpio_AttachActOut(&gpio, _actOut);
    CHECK(pifGpio_WriteCell(&gpio, 1, ON) && s_out == 0x02);
    CHECK(pifGpio_WriteCell(&gpio, 3, ON) && s_out == 0x0A);
    CHECK(pifGpio_WriteCell(&gpio, 1, OFF) && s_out == 0x08);
    CHECK(!pifGpio_WriteCell(&gpio, 4, ON) && pif_error == E_INVALID_PARAM);
    CHECK(s_out == 0x08 && s_out_count == 3);
}

// Every pin of PIF_GPIO_WIDTH, where the mask cannot be made by shifting.
static void testFullWidth(void)
{
    PifGpio gpio;
    PifTask* p_task;
    const PifGpioState top = (PifGpioState)1 << (PIF_GPIO_WIDTH - 1);
    const PifGpioState all = (PifGpioState)~(PifGpioState)0;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, PIF_GPIO_WIDTH));
    pifGpio_AttachActIn(&gpio, _actIn);
    pifGpio_AttachActOut(&gpio, _actOut);
    gpio.evt_in = _evtIn;
    s_in = all;
    CHECK(pifGpio_ReadAll(&gpio) == all);
    s_in = top;
    CHECK(pifGpio_ReadAll(&gpio) == top && pifGpio_ReadCell(&gpio, PIF_GPIO_WIDTH - 1) == ON);
    CHECK(pifGpio_ReadCell(&gpio, PIF_GPIO_WIDTH - 2) == OFF);
    CHECK(pifGpio_WriteCell(&gpio, PIF_GPIO_WIDTH - 1, ON) && s_out == top);
    CHECK(pifGpio_WriteAll(&gpio, all) && s_out == all);
    CHECK(!pifGpio_WriteCell(&gpio, PIF_GPIO_WIDTH, ON) && pif_error == E_INVALID_PARAM);

    s_in = 0;
    p_task = pifGpio_AttachTaskIn(&gpio, PIF_ID_AUTO, TM_PERIOD, 10, FALSE);
    CHECK(p_task);
    s_in = top;
    _doTask(p_task);
    pifTaskManager_Remove(p_task);
    CHECK(s_event_count == 1 && s_event[0].index == PIF_GPIO_WIDTH - 1 && s_event[0].state == ON);

    pifGpio_sigData(&gpio, PIF_GPIO_WIDTH - 1, OFF);
    CHECK(s_event_count == 2 && gpio.__read_state == 0);
}

#ifdef PIF_COLLECT_SIGNAL

// A collect signal channel holds 32 bits at most.
static void testCollectSignal(void)
{
    PifGpio gpio;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, PIF_GPIO_WIDTH < 32 ? PIF_GPIO_WIDTH : 32));
    CHECK(pifGpio_SetCsFlag(&gpio, GP_CSF_ALL_BIT));
    pifGpio_Clear(&gpio);
#if PIF_GPIO_WIDTH > 32
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, 33));
    CHECK(!pifGpio_SetCsFlag(&gpio, GP_CSF_ALL_BIT) && pif_error == E_INVALID_PARAM);
#endif
}

#endif

static void testTaskIn(void)
{
    PifGpio gpio;
    PifTask* p_task;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, 3));
    pifGpio_AttachActIn(&gpio, _actIn);
    gpio.evt_in = _evtIn;

    // A pin that is already set is the reference, not an event.
    s_in = 0x01;
    p_task = pifGpio_AttachTaskIn(&gpio, PIF_ID_AUTO, TM_PERIOD, 10, FALSE);
    CHECK(p_task);
    _doTask(p_task);
    CHECK(s_event_count == 0);

    // Reading a pin must not swallow the change the task reports next.
    s_in = 0x06;
    CHECK(pifGpio_ReadCell(&gpio, 1) == ON);
    _doTask(p_task);
    CHECK(s_event_count == 3);
    CHECK(s_event[0].p_owner == &gpio && s_event[0].index == 0 && s_event[0].state == OFF);
    CHECK(s_event[1].index == 1 && s_event[1].state == ON);
    CHECK(s_event[2].index == 2 && s_event[2].state == ON);

    // Bits above count are not pins.
    s_in = 0xFE;
    _doTask(p_task);
    CHECK(s_event_count == 3);
    pifTaskManager_Remove(p_task);
}

static void testSigData(void)
{
    PifGpio gpio;

    _setUp();
    CHECK(pifGpio_Init(&gpio, PIF_ID_AUTO, 2));
    gpio.evt_in = _evtIn;
    pifGpio_sigData(&gpio, 1, 5);
    CHECK(s_event_count == 1 && s_event[0].p_owner == &gpio && s_event[0].index == 1 && s_event[0].state == ON);
    pifGpio_sigData(&gpio, 1, ON);
    CHECK(s_event_count == 1);
    pifGpio_sigData(&gpio, 1, OFF);
    CHECK(s_event_count == 2 && s_event[1].state == OFF);
    pifGpio_sigData(&gpio, 2, ON);
    CHECK(s_event_count == 2 && pif_error == E_INVALID_PARAM && gpio.__read_state == 0);
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
        { "in and out", testInAndOut },
        { "write cell", testWriteCell },
        { "full width", testFullWidth },
        { "task in", testTaskIn },
        { "sig data", testSigData },
#ifdef PIF_COLLECT_SIGNAL
        { "collect signal", testCollectSignal },
#endif
    };

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(2, 0)) {
        printf("pif_gpio: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_gpio (%d bit)\n", PIF_GPIO_WIDTH);
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
