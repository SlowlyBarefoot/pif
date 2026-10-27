// SPDX-License-Identifier: BSD-3-Clause
// pif_can.c is included so the tests can run its static tasks directly.
#include "../../source/communication/pif_can.c"

#include <stdio.h>
#include <string.h>
#include <time.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

// A client that takes the frames of one ID and records what it is called with.
typedef struct
{
    PifCanClient client;		// First, so a PifCanClient pointer is a TestClient pointer.
    uint32_t id;
    int received;
    int rx_done_calls;
    int sending_calls;
    uint32_t rx_done_delay;		// Returned by evt_rx_done.
    uint32_t sending_delay;		// Returned by evt_sending.
    int send_count;				// Frames evt_sending queues on each call.
} TestClient;

static BOOL s_failed;

static PifCan s_can;
static PifTask s_task;				// Stands in for the tasks, which are run directly.
static TestClient s_client_1, s_client_2;

static int s_refuse;				// act_send_frame refuses this many calls.
static PifCanFrame s_sent[16];
static int s_sent_count;

static PifCanFrame s_other[16];		// Frames given to evt_receive.
static int s_other_count;

static PifCanBusState s_last_state;
static int s_state_calls;

static uint8_t s_data[64];


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static BOOL _actSendFrame(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    if (s_refuse > 0) {
        s_refuse--;
        return FALSE;
    }
    if (s_sent_count < 16) s_sent[s_sent_count++] = *p_frame;
    return TRUE;
}

static void _evtReceive(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    if (s_other_count < 16) s_other[s_other_count++] = *p_frame;
}

static void _evtBusState(PifCan *p_can, PifCanBusState state)
{
    (void)p_can;
    s_last_state = state;
    s_state_calls++;
}

static BOOL _evtClientReceive(PifCanClient *p_client, const PifCanFrame *p_frame)
{
    TestClient *p_owner = (TestClient *)p_client;

    if (p_frame->id != p_owner->id) return FALSE;
    p_owner->received++;
    return TRUE;
}

static uint32_t _evtClientRxDone(PifCanClient *p_client)
{
    TestClient *p_owner = (TestClient *)p_client;

    p_owner->rx_done_calls++;
    return p_owner->rx_done_delay;
}

static uint32_t _evtClientSending(PifCanClient *p_client)
{
    TestClient *p_owner = (TestClient *)p_client;
    PifCanFrame frame;
    int i;

    p_owner->sending_calls++;
    memset(&frame, 0, sizeof(frame));
    frame.id = p_owner->id;
    frame.length = 1;
    for (i = 0; i < p_owner->send_count; i++) {
        if (!pifCan_SendFrame(&s_can, &frame)) break;
    }
    return p_owner->sending_delay;
}

static void _initClient(TestClient *p_client, uint32_t id)
{
    memset(p_client, 0, sizeof(TestClient));
    p_client->id = id;
    p_client->client.evt_receive = _evtClientReceive;
    p_client->client.evt_rx_done = _evtClientRxDone;
    p_client->client.evt_sending = _evtClientSending;
}

static BOOL _setUp(uint32_t data_bitrate, uint16_t rx_count, uint16_t tx_count)
{
    // A failed test returns before clearing, so free what it left behind.
    s_can.__p_clients = NULL;
    pifCan_Clear(&s_can);

    if (!pifCan_Init(&s_can, PIF_ID_AUTO, 500000, data_bitrate, rx_count, tx_count)) return FALSE;
    s_can.act_send_frame = _actSendFrame;
    s_can.evt_receive = _evtReceive;
    s_can.evt_bus_state = _evtBusState;
    s_task._p_client = &s_can;

    s_refuse = 0;
    s_sent_count = 0;
    s_other_count = 0;
    s_state_calls = 0;
    _initClient(&s_client_1, 0x10);
    _initClient(&s_client_2, 0x20);
    return TRUE;
}

static BOOL _putRx(uint32_t id, uint8_t flags)
{
    PifCanFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.id = id;
    frame.flags = flags;
    frame.length = 2;
    return pifCan_PutRxFrame(&s_can, &frame);
}


static void _testDlc(void)
{
    uint8_t dlc;

    for (dlc = 0; dlc < 16; dlc++) CHECK(pifCan_LengthToDlc(pifCan_DlcToLength(dlc)) == dlc);
    CHECK(pifCan_LengthToDlc(9) == 9);
    CHECK(pifCan_LengthToDlc(13) == 10);
    CHECK(pifCan_LengthToDlc(49) == 15);
    CHECK(pifCan_LengthToDlc(64) == 15);
    CHECK(pifCan_DlcToLength(0x1F) == 64);
}

static void _testFrameTime(void)
{
    CHECK(_setUp(2000000, 4, 4));
    // 47 + 64 bits at 500 kbit/s.
    CHECK(pifCan_GetFrameTime(&s_can, CAN_F_NONE, 8) == 222);
    CHECK(pifCan_GetFrameTime(&s_can, CAN_F_EXTENDED, 0) == 134);
    CHECK(pifCan_GetFrameTime(&s_can, CAN_F_RTR, 8) == 94);
    // The data phase is faster with BRS.
    CHECK(pifCan_GetFrameTime(&s_can, CAN_F_FD | CAN_F_BRS, 64) < pifCan_GetFrameTime(&s_can, CAN_F_FD, 64));

    CHECK(pifCan_SetBitrate(&s_can, 1000000, 0));
    CHECK(pifCan_GetFrameTime(&s_can, CAN_F_NONE, 8) == 111);
    CHECK(!pifCan_SetBitrate(&s_can, 0, 0));
    pifCan_Clear(&s_can);
}

static void _testSendFrameCheck(void)
{
    PifCanFrame frame;

    CHECK(_setUp(0, 4, 2));
    memset(&frame, 0, sizeof(frame));

    frame.id = 0x800;
    CHECK(!pifCan_SendFrame(&s_can, &frame) && pif_error == E_INVALID_PARAM);
    frame.flags = CAN_F_EXTENDED;
    CHECK(pifCan_SendFrame(&s_can, &frame));

    frame.id = 0x123;
    frame.flags = CAN_F_RTR | CAN_F_FD;
    CHECK(!pifCan_SendFrame(&s_can, &frame));
    frame.flags = CAN_F_BRS;
    CHECK(!pifCan_SendFrame(&s_can, &frame));
    frame.flags = CAN_F_NONE;
    frame.length = 9;
    CHECK(!pifCan_SendFrame(&s_can, &frame));
    frame.flags = CAN_F_FD;
    frame.length = 10;
    CHECK(!pifCan_SendFrame(&s_can, &frame));
    frame.length = 12;
    CHECK(pifCan_SendFrame(&s_can, &frame));

    // The queue holds two frames.
    CHECK(pifCan_GetTxRemain(&s_can) == 0);
    CHECK(!pifCan_SendFrame(&s_can, &frame) && pif_error == E_OVERFLOW_BUFFER);
    pifCan_Clear(&s_can);
}

static void _testSendAndBusy(void)
{
    PifCanFrame frame;

    CHECK(_setUp(2000000, 4, 4));
    memset(&frame, 0, sizeof(frame));
    frame.id = 0x1ABCDEF;
    frame.flags = CAN_F_EXTENDED | CAN_F_FD | CAN_F_BRS;
    frame.length = 20;
    memcpy(frame.data, s_data, 20);
    CHECK(pifCan_SendFrame(&s_can, &frame));
    frame.id = 0x7FF;
    frame.flags = CAN_F_NONE;
    frame.length = 8;
    CHECK(pifCan_SendFrame(&s_can, &frame));

    // A refused frame stays first and is tried again after one frame.
    s_refuse = 1;
    CHECK(_doTxTask(&s_task) == pifCan_GetFrameTime(&s_can, CAN_F_EXTENDED | CAN_F_FD | CAN_F_BRS, 20));
    CHECK(s_sent_count == 0 && pifCan_GetTxRemain(&s_can) == 2);
    CHECK(_doTxTask(&s_task) == 0);
    CHECK(s_sent_count == 2);
    CHECK(s_sent[0].id == 0x1ABCDEF && s_sent[0].flags == (CAN_F_EXTENDED | CAN_F_FD | CAN_F_BRS));
    CHECK(s_sent[0].length == 20 && !memcmp(s_sent[0].data, s_data, 20));
    CHECK(s_sent[1].id == 0x7FF && s_sent[1].length == 8);
    pifCan_Clear(&s_can);
}

static void _testReceive(void)
{
    PifCanFrame frame;

    CHECK(_setUp(0, 2, 4));
    memset(&frame, 0, sizeof(frame));
    frame.id = 0x123;
    frame.length = 8;
    memcpy(frame.data, s_data, 8);
    CHECK(pifCan_PutRxFrame(&s_can, &frame));
    frame.flags = CAN_F_RTR;
    frame.length = 4;
    CHECK(pifCan_PutRxFrame(&s_can, &frame));

    // A full queue counts the frame, and a frame too long is refused without counting.
    CHECK(!pifCan_PutRxFrame(&s_can, &frame));
    CHECK(s_can._rx_lost == 1);
    frame.length = PIF_CAN_MAX_DATA_SIZE + 1;
    CHECK(!pifCan_PutRxFrame(&s_can, &frame));
    CHECK(s_can._rx_lost == 1);

    CHECK(_doRxTask(&s_task) == 0);
    CHECK(s_other_count == 2);
    CHECK(s_other[0].id == 0x123 && s_other[0].length == 8 && !memcmp(s_other[0].data, s_data, 8));
    CHECK(s_other[1].flags == CAN_F_RTR && s_other[1].length == 4);
    pifCan_Clear(&s_can);
}

static void _testClientReceive(void)
{
    CHECK(_setUp(0, 8, 4));
    CHECK(pifCan_AddClient(&s_can, &s_client_1.client));
    CHECK(pifCan_AddClient(&s_can, &s_client_2.client));
    CHECK(!pifCan_AddClient(&s_can, &s_client_1.client) && pif_error == E_ALREADY_ATTACHED);

    CHECK(_putRx(0x10, CAN_F_NONE));
    CHECK(_putRx(0x20, CAN_F_NONE));
    CHECK(_putRx(0x30, CAN_F_NONE));
    CHECK(_doRxTask(&s_task) == 0);
    CHECK(s_client_1.received == 1 && s_client_2.received == 1);
    CHECK(s_other_count == 1 && s_other[0].id == 0x30);
    CHECK(s_client_1.rx_done_calls == 1 && s_client_2.rx_done_calls == 1);

    // A client added first is offered the frame first, and one that takes it keeps it from the rest.
    s_client_2.id = 0x10;
    CHECK(_putRx(0x10, CAN_F_NONE));
    _doRxTask(&s_task);
    CHECK(s_client_1.received == 2 && s_client_2.received == 1);

    pifCan_RemoveClient(&s_can, &s_client_1.client);
    CHECK(_putRx(0x10, CAN_F_NONE));
    _doRxTask(&s_task);
    CHECK(s_client_1.received == 2 && s_client_2.received == 2);

    // Removing one that is not added changes nothing.
    pifCan_RemoveClient(&s_can, &s_client_1.client);
    CHECK(s_can.__p_clients == &s_client_2.client && s_client_2.client.__p_next == NULL);
    pifCan_RemoveClient(&s_can, &s_client_2.client);
    CHECK(s_can.__p_clients == NULL);
    pifCan_Clear(&s_can);
}

static void _testClientDelay(void)
{
    CHECK(_setUp(0, 4, 4));
    CHECK(pifCan_AddClient(&s_can, &s_client_1.client));
    CHECK(pifCan_AddClient(&s_can, &s_client_2.client));

    // The earliest delay any client asks for is taken.
    s_client_1.rx_done_delay = 500;
    s_client_2.rx_done_delay = 300;
    CHECK(_doRxTask(&s_task) == 300);
    s_client_2.rx_done_delay = 0;
    CHECK(_doRxTask(&s_task) == 500);

    s_client_1.sending_delay = 0;
    s_client_2.sending_delay = 40;
    CHECK(_doTxTask(&s_task) == 40);
    CHECK(s_client_1.sending_calls == 1 && s_client_2.sending_calls == 1);

    // Frames a client queues go out in the same run.
    s_client_1.send_count = 2;
    s_client_2.sending_delay = 0;
    CHECK(_doTxTask(&s_task) == 0);
    CHECK(s_sent_count == 2 && s_sent[0].id == 0x10);

    // While the hardware refuses, the clients wait a frame like the queue.
    s_refuse = 1;
    s_client_2.sending_delay = 1;
    CHECK(_doTxTask(&s_task) == pifCan_GetFrameTime(&s_can, CAN_F_NONE, 1));
    pifCan_Clear(&s_can);
}

static void _testBusOff(void)
{
    PifCanFrame frame;

    CHECK(_setUp(0, 4, 4));
    CHECK(pifCan_AddClient(&s_can, &s_client_1.client));
    memset(&frame, 0, sizeof(frame));
    CHECK(pifCan_SendFrame(&s_can, &frame));

    pifCan_SigBusState(&s_can, CBS_BUS_OFF);
    CHECK(s_state_calls == 1 && s_last_state == CBS_BUS_OFF && s_can._bus_state == CBS_BUS_OFF);
    CHECK(_doTxTask(&s_task) == 0);
    CHECK(s_sent_count == 0 && s_client_1.sending_calls == 0);

    // The same state again is not reported.
    pifCan_SigBusState(&s_can, CBS_BUS_OFF);
    CHECK(s_state_calls == 1);

    pifCan_SigBusState(&s_can, CBS_ERROR_PASSIVE);
    CHECK(s_state_calls == 2 && s_last_state == CBS_ERROR_PASSIVE);
    _doTxTask(&s_task);
    CHECK(s_sent_count == 1 && s_client_1.sending_calls == 1);
    pifCan_Clear(&s_can);
}

static void _testTasks(void)
{
    PifTask *p_task;

    CHECK(_setUp(0, 4, 4));
    p_task = pifCan_AttachRxTask(&s_can, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL);
    CHECK(p_task && !strcmp(p_task->name, "CanRx"));
    CHECK(!pifCan_AttachRxTask(&s_can, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL) && pif_error == E_ALREADY_ATTACHED);
    p_task = pifCan_AttachTxTask(&s_can, PIF_ID_AUTO, TM_EXTERNAL, 0, "Tx");
    CHECK(p_task && !strcmp(p_task->name, "Tx"));

    // Triggers reach the tasks, and clearing removes them.
    CHECK(_putRx(0x10, CAN_F_NONE));
    pifCan_TriggerTx(&s_can);
    pifCan_SigTxComplete(&s_can);
    pifCan_Clear(&s_can);
    CHECK(!s_can._p_rx_task && !s_can._p_tx_task);
}

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "dlc", _testDlc },
    { "frame time", _testFrameTime },
    { "send frame check", _testSendFrameCheck },
    { "send and busy", _testSendAndBusy },
    { "receive", _testReceive },
    { "client receive", _testClientReceive },
    { "client delay", _testClientDelay },
    { "bus off", _testBusOff },
    { "tasks", _testTasks },
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(4, 1);
}

int main(void)
{
    int failed = 0, i;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_can: setup failed (%d)\n", pif_error);
        return 1;
    }
    for (i = 0; i < (int)sizeof(s_data); i++) s_data[i] = (uint8_t)(i * 7 + 3);

    printf("pif_can\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifCan_Clear(&s_can);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
