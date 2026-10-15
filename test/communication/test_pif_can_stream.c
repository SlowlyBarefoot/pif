// SPDX-License-Identifier: BSD-3-Clause
// pif_can.c and pif_can_stream.c are included so the tests can run the static tasks directly.
#include "../../source/communication/pif_can.c"
#include "../../source/communication/pif_can_stream.c"

#include <stdio.h>
#include <time.h>


#define DATA_SIZE		3000
#define LOG_SIZE		1024

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

typedef struct
{
    PifTransport *p_transport;
    const uint8_t *p_tx;
    uint16_t tx_size;
    uint16_t tx_pos;
    uint8_t rx[DATA_SIZE];
    uint16_t rx_size;
    BOOL rx_block;				// Reads nothing while set.
} Client;

static BOOL s_failed;

static PifCan s_can_a, s_can_b;		// Joined by act_send_frame: what one sends the other receives.
static PifTask s_task_a, s_task_b;	// Stand in for the tasks, which are run directly.
static PifCanStream s_stream_a, s_stream_b;
static Client s_client_a, s_client_b;

static int s_refuse;				// act_send_frame of s_can_a refuses this many calls.

// Frames sent by s_can_a.
static PifCanFrame s_log[LOG_SIZE];
static int s_log_count;

// Frames given to evt_receive of s_can_b.
static PifCanFrame s_other[16];
static int s_other_count;

static uint8_t s_data[DATA_SIZE];


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static BOOL _sendA(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    if (s_refuse > 0) {
        s_refuse--;
        return FALSE;
    }
    // A full queue on the other side refuses the frame, as busy hardware would.
    if (!pifCan_PutRxFrame(&s_can_b, p_frame)) return FALSE;
    if (s_log_count < LOG_SIZE) s_log[s_log_count++] = *p_frame;
    return TRUE;
}

static BOOL _sendB(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    return pifCan_PutRxFrame(&s_can_a, p_frame);
}

static void _evtReceiveB(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    if (s_other_count < 16) s_other[s_other_count++] = *p_frame;
}

static uint16_t _clientSending(void *p_client, PifActTransportSend act_send)
{
    Client *p_owner = p_client;
    uint16_t length;

    if (p_owner->tx_pos >= p_owner->tx_size) return 0;
    length = (*act_send)(p_owner->p_transport, (uint8_t *)p_owner->p_tx + p_owner->tx_pos,
            p_owner->tx_size - p_owner->tx_pos);
    p_owner->tx_pos += length;
    return length;
}

static BOOL _clientParsing(void *p_client, PifActTransportReceive act_receive)
{
    Client *p_owner = p_client;
    uint16_t length;

    if (p_owner->rx_block) return FALSE;
    length = (*act_receive)(p_owner->p_transport, p_owner->rx + p_owner->rx_size, DATA_SIZE - p_owner->rx_size);
    p_owner->rx_size += length;
    return length > 0;
}

static void _attach(Client *p_client, PifCanStream *p_stream, const uint8_t *p_tx, uint16_t tx_size)
{
    memset(p_client, 0, sizeof(Client));
    p_client->p_transport = &p_stream->parent;
    p_client->p_tx = p_tx;
    p_client->tx_size = tx_size;
    pifTransport_AttachClient(&p_stream->parent, p_client, _clientParsing, _clientSending, NULL);
}

// Runs both sides until nothing moves.
static BOOL _pump(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        _doTxTask(&s_task_a);
        _doRxTask(&s_task_b);
        _doTxTask(&s_task_b);
        _doRxTask(&s_task_a);
        if (pifRingData_IsEmpty(&s_can_a.__tx_queue) && pifRingData_IsEmpty(&s_can_b.__rx_queue) &&
                pifRingData_IsEmpty(&s_can_b.__tx_queue) && pifRingData_IsEmpty(&s_can_a.__rx_queue) &&
                s_client_a.tx_pos >= s_client_a.tx_size && s_client_b.tx_pos >= s_client_b.tx_size &&
                !s_stream_a.__tx_frame.length && !s_stream_b.__tx_frame.length) {
            return TRUE;
        }
    }
    return FALSE;
}

static void _clearAll(void)
{
    pifCanStream_Clear(&s_stream_a);
    pifCanStream_Clear(&s_stream_b);
    pifCan_Clear(&s_can_a);
    pifCan_Clear(&s_can_b);
}

static BOOL _setUp(uint32_t data_bitrate, uint16_t rx_count, uint16_t tx_count)
{
    // A failed test returns before clearing, so free what it left behind.
    _clearAll();

    if (!pifCan_Init(&s_can_a, PIF_ID_AUTO, 500000, data_bitrate, rx_count, tx_count)) return FALSE;
    if (!pifCan_Init(&s_can_b, PIF_ID_AUTO, 500000, data_bitrate, rx_count, tx_count)) return FALSE;
    s_can_a.act_send_frame = _sendA;
    s_can_b.act_send_frame = _sendB;
    s_can_b.evt_receive = _evtReceiveB;
    s_task_a._p_client = &s_can_a;
    s_task_b._p_client = &s_can_b;

    s_refuse = 0;
    s_log_count = 0;
    s_other_count = 0;
    memset(&s_client_a, 0, sizeof(Client));
    memset(&s_client_b, 0, sizeof(Client));
    return TRUE;
}

// A stream pair: s_stream_a sends with 0x100 and receives 0x101, s_stream_b the other way round.
static BOOL _setUpStreams(uint8_t flags, uint8_t frame_size, uint16_t rx_size)
{
    if (!pifCanStream_Init(&s_stream_a, PIF_ID_AUTO, &s_can_a, 0x100, 0x101, flags, frame_size, rx_size)) return FALSE;
    if (!pifCanStream_Init(&s_stream_b, PIF_ID_AUTO, &s_can_b, 0x101, 0x100, flags, frame_size, rx_size)) return FALSE;
    return TRUE;
}


static void _testStreamClassic(void)
{
    int i;

    CHECK(_setUp(0, 4, 4));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 64));
    _attach(&s_client_a, &s_stream_a, s_data, DATA_SIZE);
    _attach(&s_client_b, &s_stream_b, s_data + 1, 100);

    CHECK(_pump());
    CHECK(s_client_b.rx_size == DATA_SIZE && !memcmp(s_client_b.rx, s_data, DATA_SIZE));
    CHECK(s_client_a.rx_size == 100 && !memcmp(s_client_a.rx, s_data + 1, 100));
    for (i = 0; i < s_log_count; i++) {
        CHECK(s_log[i].id == 0x100 && s_log[i].flags == CAN_F_NONE && s_log[i].length >= 1 && s_log[i].length <= 8);
    }
    CHECK(!s_stream_a._rx_lost && !s_stream_b._rx_lost && !s_can_a._rx_lost && !s_can_b._rx_lost);
    CHECK(!s_other_count);
    _clearAll();
}

static void _testStreamFdSplit(void)
{
    CHECK(_setUp(2000000, 8, 8));
    CHECK(_setUpStreams(CAN_F_FD | CAN_F_BRS, 64, 256));
    _attach(&s_client_a, &s_stream_a, s_data, 100);
    _attach(&s_client_b, &s_stream_b, NULL, 0);

    CHECK(_pump());
    CHECK(s_client_b.rx_size == 100 && !memcmp(s_client_b.rx, s_data, 100));
    // 100 bytes in one call: a full frame, then the 36 left as lengths a CAN FD frame can have.
    CHECK(s_log_count == 3);
    CHECK(s_log[0].length == 64 && s_log[1].length == 32 && s_log[2].length == 4);
    CHECK(s_log[0].flags == (CAN_F_FD | CAN_F_BRS));
    _clearAll();
}

static void _testHardwareBusy(void)
{
    CHECK(_setUp(0, 4, 2));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 64));
    _attach(&s_client_a, &s_stream_a, s_data, 500);
    _attach(&s_client_b, &s_stream_b, NULL, 0);

    // Refused frames stay queued and are tried again after one frame.
    s_refuse = 3;
    CHECK(_doTxTask(&s_task_a) == pifCan_GetFrameTime(&s_can_a, CAN_F_NONE, 8));
    CHECK(_pump());
    CHECK(s_client_b.rx_size == 500 && !memcmp(s_client_b.rx, s_data, 500));
    _clearAll();
}

static void _testRxOverflow(void)
{
    PifCanFrame frame;

    CHECK(_setUp(0, 4, 4));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 16));
    _attach(&s_client_b, &s_stream_b, NULL, 0);
    s_client_b.rx_block = TRUE;

    memset(&frame, 0, sizeof(frame));
    frame.id = 0x100;
    frame.length = 8;
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    // The third frame does not fit in 16 bytes and is dropped whole. The client is asked again later.
    CHECK(_doRxTask(&s_task_b) > 0);
    CHECK(s_stream_b._rx_lost == 8);
    CHECK(pifRingBuffer_GetFillSize(&s_stream_b._rx_buffer) == 16);

    // A full receive queue counts the frame.
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(!pifCan_PutRxFrame(&s_can_b, &frame));
    CHECK(s_can_b._rx_lost == 1);

    // The 16 bytes kept and the 4 queued frames all arrive in one run.
    s_client_b.rx_block = FALSE;
    CHECK(_doRxTask(&s_task_b) == 0);
    CHECK(s_client_b.rx_size == 48);
    _clearAll();
}

static void _testOtherFrames(void)
{
    PifCanFrame frame;

    CHECK(_setUp(0, 4, 4));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 64));
    _attach(&s_client_b, &s_stream_b, NULL, 0);
    memset(&frame, 0, sizeof(frame));

    // Another ID, the same ID as an extended one, and a remote frame do not go to the stream.
    frame.id = 0x200;
    frame.length = 1;
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    frame.id = 0x100;
    frame.flags = CAN_F_EXTENDED;
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    frame.flags = CAN_F_RTR;
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    _doRxTask(&s_task_b);
    CHECK(s_other_count == 3 && !s_client_b.rx_size);

    // After the stream is cleared its ID goes to evt_receive too.
    pifCanStream_Clear(&s_stream_b);
    CHECK(s_can_b.__p_clients == NULL);
    frame.flags = CAN_F_NONE;
    CHECK(pifCan_PutRxFrame(&s_can_b, &frame));
    _doRxTask(&s_task_b);
    CHECK(s_other_count == 4);
    _clearAll();
}

static void _testBusOff(void)
{
    CHECK(_setUp(0, 4, 4));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 64));
    _attach(&s_client_a, &s_stream_a, s_data, 20);
    _attach(&s_client_b, &s_stream_b, NULL, 0);

    pifCan_SigBusState(&s_can_a, CBS_BUS_OFF);
    CHECK(pifTransport_GetFlowState(&s_stream_a.parent) == OFF);
    CHECK(_doTxTask(&s_task_a) == 0);
    CHECK(s_client_a.tx_pos == 0 && s_log_count == 0);

    pifCan_SigBusState(&s_can_a, CBS_ERROR_ACTIVE);
    CHECK(pifTransport_GetFlowState(&s_stream_a.parent) == ON);
    CHECK(_pump());
    CHECK(s_client_b.rx_size == 20 && !memcmp(s_client_b.rx, s_data, 20));
    _clearAll();
}

static void _testFloorLength(void)
{
    CHECK(_floorLength(64) == 64);
    CHECK(_floorLength(63) == 48);
    CHECK(_floorLength(36) == 32);
    CHECK(_floorLength(12) == 12);
    CHECK(_floorLength(11) == 8);
    CHECK(_floorLength(5) == 5);
}

static void _testStreamInit(void)
{
    PifCanStream stream;

    CHECK(_setUp(0, 4, 4));
    CHECK(_setUpStreams(CAN_F_NONE, 8, 64));
    CHECK(!pifCanStream_Init(&stream, PIF_ID_AUTO, &s_can_a, 0x300, 0x301, CAN_F_NONE, 9, 64));
    CHECK(!pifCanStream_Init(&stream, PIF_ID_AUTO, &s_can_a, 0x300, 0x301, CAN_F_FD, 10, 64));
    CHECK(!pifCanStream_Init(&stream, PIF_ID_AUTO, &s_can_a, 0x300, 0x301, CAN_F_BRS, 8, 64));
    CHECK(!pifCanStream_Init(&stream, PIF_ID_AUTO, &s_can_a, 0x800, 0x301, CAN_F_NONE, 8, 64));

    // Added after s_stream_a, so it is offered frames after it.
    CHECK(pifCanStream_Init(&stream, PIF_ID_AUTO, &s_can_a, 0x300, 0x101, CAN_F_EXTENDED | CAN_F_FD, 64, 64));
    CHECK(s_stream_a.__can_client.__p_next == &stream.__can_client);
    CHECK(pifTransport_GetFrameSize(&stream.parent) == 1);
    CHECK(pifTransport_GetByteTime(&stream.parent) > 0);
    pifCanStream_Clear(&stream);
    CHECK(s_can_a.__p_clients == &s_stream_a.__can_client && s_stream_a.__can_client.__p_next == NULL);
    _clearAll();
}

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "floor length", _testFloorLength },
    { "stream classic", _testStreamClassic },
    { "stream fd split", _testStreamFdSplit },
    { "hardware busy", _testHardwareBusy },
    { "rx overflow", _testRxOverflow },
    { "other frames", _testOtherFrames },
    { "bus off", _testBusOff },
    { "stream init", _testStreamInit },
};

int main(void)
{
    int failed = 0, i;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 1)) {
        printf("pif_can_stream: setup failed (%d)\n", pif_error);
        return 1;
    }
    for (i = 0; i < DATA_SIZE; i++) s_data[i] = (uint8_t)(i * 7 + (i >> 8));

    printf("pif_can_stream\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    _clearAll();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
