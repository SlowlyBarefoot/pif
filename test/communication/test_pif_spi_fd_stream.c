// SPDX-License-Identifier: BSD-3-Clause
// pif_device_stream.c and pif_spi_fd_stream.c are included so the tests can run the static task directly.
#include "../../source/communication/pif_device_stream.c"
#include "../../source/communication/pif_spi_fd_stream.c"

#include <stdio.h>
#include <time.h>


#define DATA_SIZE		600
#define RUN_LIMIT		10000
#define IDLE			0xFF

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
    uint8_t rx[DATA_SIZE * 2];
    uint16_t rx_size;
    int abort_count;
} Client;

static BOOL s_failed;

static PifSpiPort s_port;
static PifSpiFdStream s_fd;
static Client s_client;

// The device behind the stream: what it holds for the host and what came in on MOSI.
static uint8_t s_out[DATA_SIZE];
static uint16_t s_out_size, s_out_pos;
static uint8_t s_in[DATA_SIZE * 4];
static uint16_t s_in_size;
static int s_transfer_count;
static size_t s_largest_transfer;
static uint8_t s_idle;					// What the device sends when it has nothing.

static BOOL s_fail_start;				// act_start_transfer refuses the next transfer.

static uint8_t s_data[DATA_SIZE];


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static void _exchange(uint8_t *p_write, uint8_t *p_read, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        if (s_in_size < sizeof(s_in)) s_in[s_in_size++] = p_write ? p_write[i] : IDLE;
        if (p_read) p_read[i] = s_out_pos < s_out_size ? s_out[s_out_pos++] : s_idle;
    }
    s_transfer_count++;
    if (size > s_largest_transfer) s_largest_transfer = size;
}

static void _actTransfer(PifSpiDevice *p_owner, uint8_t *p_write, uint8_t *p_read, size_t size)
{
    _exchange(p_write, p_read, size);
}

static BOOL _actStartTransfer(PifSpiDevice *p_owner, uint8_t *p_write, uint8_t *p_read, size_t size)
{
    if (s_fail_start) {
        s_fail_start = FALSE;
        return FALSE;
    }
    _exchange(p_write, p_read, size);
    return TRUE;
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

    length = (*act_receive)(p_owner->p_transport, p_owner->rx + p_owner->rx_size,
            sizeof(p_owner->rx) - p_owner->rx_size);
    p_owner->rx_size += length;
    return length > 0;
}

static void _clientAbortRx(void *p_client)
{
    ((Client *)p_client)->abort_count++;
}

static void _attach(const uint8_t *p_tx, uint16_t tx_size, BOOL parsing)
{
    memset(&s_client, 0, sizeof(Client));
    s_client.p_transport = &s_fd.stream.parent;
    s_client.p_tx = p_tx;
    s_client.tx_size = tx_size;
    pifTransport_AttachClient(&s_fd.stream.parent, &s_client, parsing ? _clientParsing : NULL,
            p_tx ? _clientSending : NULL, _clientAbortRx);
}

// Runs the task until the device answers empty and the client has sent everything.
static BOOL _run(void)
{
    int i;

    for (i = 0; i < RUN_LIMIT; i++) {
        if (s_port.__use_device && s_port.act_start_transfer) pifSpiPort_sigEndTransfer(&s_port, TRUE);
        _doTask(s_fd.stream._p_task);
        if (!s_fd.stream.__running && !s_fd.stream.__more && s_out_pos >= s_out_size &&
                s_client.tx_pos >= s_client.tx_size && !s_fd.stream.__tx_len) {
            return TRUE;
        }
    }
    return FALSE;
}

static void _setDevice(const uint8_t *p_out, uint16_t size)
{
    memcpy(s_out, p_out, size);
    s_out_size = size;
    s_out_pos = 0;
}

// The bytes of the client that came in on MOSI, without the idle bytes the stream filled the transfers with.
// No byte of the data is idle.
static uint8_t s_sent[DATA_SIZE * 4];

static uint16_t _sentSize(void)
{
    uint16_t i, size = 0;

    for (i = 0; i < s_in_size; i++) {
        if (s_in[i] != s_fd.__protocol.__idle_byte) s_sent[size++] = s_in[i];
    }
    return size;
}

static void _clearAll(void)
{
    pifSpiFdStream_Clear(&s_fd);
    pifSpiPort_Clear(&s_port);
}

static BOOL _setUpSized(uint16_t rx_size, uint16_t tx_size, BOOL background, uint16_t transfer_size)
{
    // A failed test returns before clearing, so free what it left behind. A transfer it left running ends
    // at the device timeout, which this brings forward.
    pif_cumulative_timer1ms += 1000;
    _clearAll();

    s_out_size = s_out_pos = 0;
    s_in_size = 0;
    s_transfer_count = 0;
    s_largest_transfer = 0;
    s_idle = IDLE;
    s_fail_start = FALSE;
    memset(&s_client, 0, sizeof(Client));

    if (!pifSpiPort_Init(&s_port, PIF_ID_AUTO, 2)) return FALSE;
    s_port.act_transfer = _actTransfer;
    if (background) s_port.act_start_transfer = _actStartTransfer;
    return pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, transfer_size, rx_size, tx_size, 100);
}

static BOOL _setUp(uint16_t rx_size, uint16_t tx_size, BOOL background)
{
    return _setUpSized(rx_size, tx_size, background, rx_size);
}

static BOOL _isAll(const uint8_t *p_data, uint16_t size, uint8_t value)
{
    while (size--) {
        if (*p_data++ != value) return FALSE;
    }
    return TRUE;
}


static void _testReceive(void)
{
    CHECK(_setUp(32, 0, FALSE));
    CHECK(s_fd.stream._p_task->_pause && s_fd.__protocol.__idle_byte == IDLE);
    _setDevice(s_data, 100);
    _attach(NULL, 0, TRUE);

    CHECK(_run());
    // Four transfers carry the data, the last one with idle bytes after it, and a fifth answers empty.
    CHECK(s_transfer_count == 5 && s_largest_transfer == 32);
    CHECK(s_client.rx_size == 128 && !memcmp(s_client.rx, s_data, 100) && _isAll(s_client.rx + 100, 28, IDLE));
    CHECK(!_sentSize() && !s_client.abort_count);
}

static void _testEmpty(void)
{
    int i;

    CHECK(_setUp(16, 0, FALSE));
    _attach(NULL, 0, TRUE);

    // An empty device is polled once a period.
    for (i = 0; i < 3; i++) {
        CHECK(_doTask(s_fd.stream._p_task) == 0);
        CHECK(!s_fd.stream.__more && !s_fd.stream._p_task->__trigger);
    }
    CHECK(s_transfer_count == 3 && !s_client.rx_size);
}

static void _testMaxTransferSize(void)
{
    CHECK(_setUp(64, 0, FALSE));
    s_fd.stream._p_device->max_transfer_size = 16;
    _setDevice(s_data, 40);
    _attach(NULL, 0, TRUE);

    CHECK(_run());
    CHECK(s_largest_transfer == 16 && !memcmp(s_client.rx, s_data, 40));
}

static void _testSend(void)
{
    CHECK(_setUp(32, 64, FALSE));
    _attach(s_data, 50, FALSE);
    pifTransport_TriggerSend(&s_fd.stream.parent);

    CHECK(_run());
    // A client that only sends still gets its bytes out, padded with the idle byte.
    CHECK(_sentSize() == 50 && !memcmp(s_sent, s_data, 50) && !memcmp(s_in, s_data, 32));
    CHECK(s_transfer_count == 2 && !s_client.rx_size);
}

static void _testSendAndReceive(void)
{
    CHECK(_setUp(32, 16, FALSE));
    _setDevice(s_data + 9, 70);
    _attach(s_data, 100, TRUE);

    CHECK(_run());
    CHECK(_sentSize() == 100 && !memcmp(s_sent, s_data, 100));
    CHECK(s_client.rx_size >= 70 && !memcmp(s_client.rx, s_data + 9, 70));
    CHECK(_isAll(s_client.rx + 70, s_client.rx_size - 70, IDLE));
}

static void _testBackground(void)
{
    PifSpiDevice *p_other;
    uint8_t byte = 0;

    CHECK(_setUp(16, 16, TRUE));
    p_other = pifSpiPort_AddDevice(&s_port, PIF_ID_AUTO, NULL);
    CHECK(p_other);
    _setDevice(s_data, 10);
    _attach(NULL, 0, TRUE);

    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US);
    CHECK(s_fd.stream.__running && pifDeviceReg_IsBusy(&p_other->parent));
    CHECK(!pifSpiDevice_Transfer(p_other, &byte, NULL, 1) && pif_error == E_INVALID_STATE);
    // Until the port ends it, the task only looks at it again.
    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US && s_transfer_count == 1);
}

static void _testBackgroundEnd(void)
{
    CHECK(_setUp(16, 16, TRUE));
    _setDevice(s_data, 10);
    _attach(NULL, 0, TRUE);

    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US);
    pifSpiPort_sigEndTransfer(&s_port, TRUE);
    // The data is given at once, and the device is polled again in the next release.
    CHECK(_doTask(s_fd.stream._p_task) == 0);
    CHECK(s_client.rx_size == 16 && !memcmp(s_client.rx, s_data, 10) && s_fd.stream._p_task->__trigger);
    CHECK(!s_fd.stream.__running && !pifDeviceReg_IsBusy(s_fd.stream._p_device));

    CHECK(_run());
    CHECK(s_client.rx_size == 16);
}

static void _testFailedTransfer(void)
{
    CHECK(_setUp(16, 16, TRUE));
    _attach(s_data, 10, TRUE);

    // A transfer that ends in an error drops what came in and sends the bytes again.
    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US);
    pifSpiPort_sigEndTransfer(&s_port, FALSE);
    _doTask(s_fd.stream._p_task);
    CHECK(s_client.abort_count == 1 && s_fd.stream.__tx_len == 10);

    // A transfer the port does not start leaves them too.
    s_fail_start = TRUE;
    _doTask(s_fd.stream._p_task);
    CHECK(s_fd.stream.__tx_len == 10 && !s_fd.stream.__running);

    s_in_size = 0;
    CHECK(_run());
    CHECK(_sentSize() == 10 && !memcmp(s_sent, s_data, 10));
}

static void _testIdleByte(void)
{
    CHECK(_setUp(8, 8, FALSE));
    pifSpiFdStream_SetIdleByte(&s_fd, 0x00);
    s_idle = 0x00;
    _setDevice(s_data + 1, 5);
    _attach(s_data, 3, TRUE);

    CHECK(_run());
    CHECK(s_in_size >= 8 && !memcmp(s_in, s_data, 3) && _isAll(s_in + 3, 5, 0x00));
    CHECK(!memcmp(s_client.rx, s_data + 1, 5) && _isAll(s_client.rx + 5, 3, 0x00));
}

static void _testDetachWhileRunning(void)
{
    CHECK(_setUp(16, 16, TRUE));
    _setDevice(s_data, 10);
    _attach(NULL, 0, TRUE);
    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US);

    // The task runs on until the transfer is over, then waits for a client.
    pifTransport_DetachClient(&s_fd.stream.parent);
    CHECK(!s_fd.stream._p_task->_pause);
    pifSpiPort_sigEndTransfer(&s_port, TRUE);
    CHECK(_doTask(s_fd.stream._p_task) == 0);
    CHECK(s_fd.stream._p_task->_pause && !s_fd.stream.__running && !s_client.rx_size);
}

static void _testClearWhileRunning(void)
{
    CHECK(_setUp(16, 16, TRUE));
    _attach(NULL, 0, TRUE);
    CHECK(_doTask(s_fd.stream._p_task) == RECHECK_PERIOD1US);

    pif_cumulative_timer1ms += 1000;
    pifSpiFdStream_Clear(&s_fd);
    // The transfer is over, so the device could leave the port.
    CHECK(!s_fd._p_device && !s_fd.stream._p_task && !s_fd.__protocol.__p_out && !s_port.__use_device);
    CHECK(pifObjArray_Count(&s_port.__devices) == 0);
}

static void _testTransferSize(void)
{
    // Transfers shorter than the receive buffer.
    CHECK(_setUpSized(64, 16, FALSE, 8));
    _setDevice(s_data, 20);
    _attach(s_data, 12, TRUE);

    CHECK(_run());
    CHECK(s_largest_transfer == 8 && _sentSize() == 12 && !memcmp(s_sent, s_data, 12));
    CHECK(s_client.rx_size == 24 && !memcmp(s_client.rx, s_data, 20) && _isAll(s_client.rx + 20, 4, IDLE));
}

static void _testInitErrors(void)
{
    _clearAll();
    CHECK(pifSpiPort_Init(&s_port, PIF_ID_AUTO, 1));

    CHECK(!pifSpiFdStream_Init(NULL, PIF_ID_AUTO, &s_port, 16, 16, 16, 100) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, NULL, 16, 16, 16, 100) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, 0, 16, 16, 100) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, 16, 0, 16, 100));
    CHECK(!pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, 16, 16, 16, 0));
    // A failed init leaves nothing behind, so the port still has room.
    CHECK(pifObjArray_Count(&s_port.__devices) == 0 && !s_fd._p_device && !s_fd.__protocol.__p_out);

    // A port without room for the device.
    CHECK(pifSpiPort_AddDevice(&s_port, PIF_ID_AUTO, NULL));
    CHECK(!pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, 16, 16, 16, 100));
    CHECK(!s_fd._p_device && !s_fd.__protocol.__p_out && !s_fd.stream._p_task);

    // What the stream puts together.
    pifSpiPort_Clear(&s_port);
    CHECK(pifSpiPort_Init(&s_port, PIF_ID_AUTO, 1));
    CHECK(pifSpiFdStream_Init(&s_fd, PIF_ID_AUTO, &s_port, 16, 32, 16, 100));
    CHECK(s_fd.stream._p_device == &s_fd._p_device->parent && s_fd.stream._p_protocol == &s_fd.__protocol.parent);
    CHECK(s_fd.__protocol.__out_size == 16 && s_fd.__protocol.__idle_byte == IDLE);
    pifSpiFdStream_SetIdleByte(&s_fd, 0x5A);
    CHECK(s_fd.__protocol.__idle_byte == 0x5A);
}

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testReceive, "receive" },
    { _testEmpty, "empty device" },
    { _testMaxTransferSize, "max_transfer_size" },
    { _testSend, "send" },
    { _testSendAndReceive, "send and receive" },
    { _testBackground, "background transfer holds the port" },
    { _testBackgroundEnd, "background transfer ended by signal" },
    { _testFailedTransfer, "failed transfer" },
    { _testIdleByte, "idle byte" },
    { _testDetachWhileRunning, "detach while running" },
    { _testClearWhileRunning, "clear while running" },
    { _testTransferSize, "transfer size" },
    { _testInitErrors, "init errors" },
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
        printf("pif_spi_fd_stream: setup failed (%d)\n", pif_error);
        return 1;
    }
    // No byte of the data is idle, so it is told apart from the padding.
    for (i = 0; i < DATA_SIZE; i++) s_data[i] = (uint8_t)(1 + (i * 7 + (i >> 8)) % 250);

    printf("pif_spi_fd_stream\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pif_cumulative_timer1ms += 1000;
    _clearAll();
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
