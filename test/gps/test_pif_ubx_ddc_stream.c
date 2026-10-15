// SPDX-License-Identifier: BSD-3-Clause
// pif_device_stream.c and pif_ubx_ddc_stream.c are included so the tests can run the static task directly.
#include "../../source/communication/pif_device_stream.c"
#include "../../source/gps/pif_ubx_ddc_stream.c"
#include "communication/pif_i2c.h"

#include <stdio.h>
#include <time.h>


#define DATA_SIZE		600
#define DEVICE_ADDR		0x42
#define RUN_LIMIT		10000

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
    int abort_count;
} Client;

static BOOL s_failed;

static PifI2cPort s_port;
static PifUbxDdcStream s_ddc;
static Client s_client;

// The device behind the stream: what it holds for the host and what the host wrote to it.
static uint8_t s_out[DATA_SIZE];
static uint16_t s_out_size, s_out_pos;
static uint16_t s_count;			// Answered from the count registers. Follows s_out unless s_fixed_count.
static BOOL s_fixed_count;
static uint8_t s_in[DATA_SIZE];
static uint16_t s_in_size;
static int s_write_count;
static uint16_t s_largest_read, s_largest_write;

static BOOL s_async;				// act_read answers IR_WAIT and the test ends the transfer.
static int s_fail_data_read;		// Data reads that fail.

static uint8_t s_data[DATA_SIZE];


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static PifI2cReturn _actRead(PifI2cDevice *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    uint16_t count = s_fixed_count ? s_count : s_out_size - s_out_pos;

    if (p_device->addr != DEVICE_ADDR) return s_async ? IR_WAIT : IR_COMPLETE;

    if (isize == 1 && iaddr == 0xFD && size == 2) {
        p_data[0] = count >> 8;
        p_data[1] = count & 0xFF;
    }
    else if (isize == 0) {
        if (s_fail_data_read > 0) {
            s_fail_data_read--;
            s_out_pos += size;
            return IR_ERROR;
        }
        if (size > s_out_size - s_out_pos) return IR_ERROR;
        memcpy(p_data, s_out + s_out_pos, size);
        s_out_pos += size;
        if (size > s_largest_read) s_largest_read = size;
    }
    else {
        return IR_ERROR;
    }
    return s_async ? IR_WAIT : IR_COMPLETE;
}

static PifI2cReturn _actWrite(PifI2cDevice *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    if (isize != 0 || s_in_size + size > DATA_SIZE) return IR_ERROR;
    memcpy(s_in + s_in_size, p_data, size);
    s_in_size += size;
    s_write_count++;
    if (size > s_largest_write) s_largest_write = size;
    return IR_COMPLETE;
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

    // One byte at a time, as the GPS parsers read.
    length = (*act_receive)(p_owner->p_transport, p_owner->rx + p_owner->rx_size, 1);
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
    s_client.p_transport = &s_ddc.stream.parent;
    s_client.p_tx = p_tx;
    s_client.tx_size = tx_size;
    pifTransport_AttachClient(&s_ddc.stream.parent, &s_client, parsing ? _clientParsing : NULL,
            p_tx ? _clientSending : NULL, _clientAbortRx);
}

// Ends a transfer the port answered with IR_WAIT, as its interrupt would.
static void _endTransfer(BOOL result)
{
    pifI2cPort_sigEndTransfer(&s_port, result);
}

// Runs the task until the device has nothing left and the client has written everything.
static BOOL _run(void)
{
    int i;

    for (i = 0; i < RUN_LIMIT; i++) {
        if (s_async && s_port.__use_device) _endTransfer(TRUE);
        _doTask(s_ddc.stream._p_task);
        if (s_out_pos >= s_out_size && !s_ddc.stream.__running && !s_ddc.__protocol.__pending &&
                s_ddc.stream.__rx_pos >= s_ddc.stream.__rx_len && s_client.tx_pos >= s_client.tx_size &&
                !s_ddc.stream.__tx_len) {
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

static void _clearAll(void)
{
    pifUbxDdcStream_Clear(&s_ddc);
    pifI2cPort_Clear(&s_port);
}

static BOOL _setUp(uint16_t rx_size, uint16_t tx_size, uint16_t max_transfer_size)
{
    // A failed test returns before clearing, so free what it left behind. A read it left running ends at
    // the device timeout, which this brings forward.
    pif_cumulative_timer1ms += 1000;
    _clearAll();

    s_out_size = s_out_pos = 0;
    s_count = 0;
    s_fixed_count = FALSE;
    s_in_size = 0;
    s_write_count = 0;
    s_largest_read = s_largest_write = 0;
    s_async = FALSE;
    s_fail_data_read = 0;
    memset(&s_client, 0, sizeof(Client));

    if (!pifI2cPort_Init(&s_port, PIF_ID_AUTO, 2)) return FALSE;
    s_port.act_read = _actRead;
    s_port.act_write = _actWrite;
    if (!pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, &s_port, DEVICE_ADDR, rx_size, tx_size, 100)) return FALSE;
    s_ddc._p_device->parent.max_transfer_size = max_transfer_size;
    return TRUE;
}


static void _testReceive(void)
{
    CHECK(_setUp(64, 0, 32));
    CHECK(s_ddc.stream._p_task->_pause);
    _setDevice(s_data, 300);
    _attach(NULL, 0, TRUE);
    CHECK(!s_ddc.stream._p_task->_pause);

    CHECK(_run());
    CHECK(s_client.rx_size == 300 && !memcmp(s_client.rx, s_data, 300));
    // A read never asks for more than max_transfer_size.
    CHECK(s_largest_read == 32);
    CHECK(!s_client.abort_count);
}

static void _testReceiveRxSizeLimit(void)
{
    CHECK(_setUp(20, 0, 0));
    _setDevice(s_data + 3, 95);
    _attach(NULL, 0, TRUE);

    CHECK(_run());
    CHECK(s_client.rx_size == 95 && !memcmp(s_client.rx, s_data + 3, 95));
    CHECK(s_largest_read == 20);
}

static void _testReceiveAsync(void)
{
    PifI2cDevice *p_other;
    uint8_t byte;

    CHECK(_setUp(64, 0, 0));
    p_other = pifI2cPort_AddDevice(&s_port, PIF_ID_AUTO, DEVICE_ADDR + 1, NULL);
    CHECK(p_other);
    s_async = TRUE;
    _setDevice(s_data, 10);
    _attach(NULL, 0, TRUE);

    // The count read is started and the task comes back soon for it.
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    CHECK((s_ddc.stream.__running && s_ddc.__protocol.__phase == UDP_COUNT));
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);

    // The count starts the data read in the same run.
    _endTransfer(TRUE);
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    CHECK((s_ddc.stream.__running && s_ddc.__protocol.__phase == UDP_DATA));
    _endTransfer(TRUE);
    CHECK(_doTask(s_ddc.stream._p_task) == 0);
    CHECK(s_client.rx_size == 10 && !memcmp(s_client.rx, s_data, 10));

    // Between reads the port is free for another device.
    CHECK(!s_ddc.stream.__running && !s_port.__use_device);
    CHECK(pifDeviceReg_StartRead(&p_other->parent, 0, 1, &byte, 1));
    // While it holds the port the stream waits without losing anything.
    _setDevice(s_data + 10, 5);
    CHECK(_doTask(s_ddc.stream._p_task) == 0);
    CHECK(!s_ddc.stream.__running && !s_client.abort_count);
    _endTransfer(TRUE);
    CHECK(pifDeviceReg_CheckTransfer(&p_other->parent) == DRS_COMPLETE);

    CHECK(_run());
    CHECK(s_client.rx_size == 15 && !memcmp(s_client.rx + 10, s_data + 10, 5));
}

static void _testNoData(void)
{
    int i;

    CHECK(_setUp(64, 0, 0));
    _attach(NULL, 0, TRUE);

    // Nothing held, and the 0xFFFF some devices answer then, read nothing.
    s_fixed_count = TRUE;
    s_count = 0;
    for (i = 0; i < 3; i++) CHECK(_doTask(s_ddc.stream._p_task) == 0);
    s_count = 0xFFFF;
    for (i = 0; i < 3; i++) CHECK(_doTask(s_ddc.stream._p_task) == 0);
    CHECK(!s_ddc.__protocol.__pending && !s_client.rx_size && !s_client.abort_count && !s_largest_read);
}

static void _testReadError(void)
{
    CHECK(_setUp(64, 0, 16));
    _setDevice(s_data, 40);
    s_fail_data_read = 1;
    _attach(NULL, 0, TRUE);

    // The failed read drops what the device had, and the client is told.
    _doTask(s_ddc.stream._p_task);
    CHECK(s_client.abort_count == 1);
    CHECK(!s_ddc.__protocol.__pending && !s_client.rx_size);

    // The next count starts over from what the device still holds.
    CHECK(_run());
    CHECK(s_client.rx_size == 24 && !memcmp(s_client.rx, s_data + 16, 24));
    CHECK(s_client.abort_count == 1);
}

static void _testSend(void)
{
    CHECK(_setUp(16, 40, 0));
    _attach(s_data, 100, FALSE);

    CHECK(_run());
    CHECK(s_in_size == 100 && !memcmp(s_in, s_data, 100));
    CHECK(s_largest_write == 40 && s_write_count == 3);
    // A client that only sends never reads the device.
    CHECK(!s_largest_read && !s_port.__use_device);
}

static void _testSendWhilePortBusy(void)
{
    PifI2cDevice *p_other;
    uint8_t byte;

    CHECK(_setUp(16, 64, 0));
    p_other = pifI2cPort_AddDevice(&s_port, PIF_ID_AUTO, DEVICE_ADDR + 1, NULL);
    CHECK(p_other);
    s_async = TRUE;
    CHECK(pifDeviceReg_StartRead(&p_other->parent, 0, 1, &byte, 1));

    // The bytes wait for the port and go out in order once it is free.
    _attach(s_data, 50, FALSE);
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    CHECK(s_ddc.stream.__tx_len == 50 && !s_in_size);
    _endTransfer(TRUE);
    CHECK(pifDeviceReg_CheckTransfer(&p_other->parent) == DRS_COMPLETE);
    s_async = FALSE;

    CHECK(_run());
    CHECK(s_in_size == 50 && !memcmp(s_in, s_data, 50) && s_write_count == 1);
}

static void _testSendAndReceive(void)
{
    CHECK(_setUp(32, 32, 0));
    _setDevice(s_data + 7, 200);
    _attach(s_data, 150, TRUE);

    CHECK(_run());
    CHECK(s_in_size == 150 && !memcmp(s_in, s_data, 150));
    CHECK(s_client.rx_size == 200 && !memcmp(s_client.rx, s_data + 7, 200));
}

static void _testTriggerSend(void)
{
    CHECK(_setUp(16, 16, 0));
    _attach(NULL, 0, TRUE);
    pifTransport_TriggerSend(&s_ddc.stream.parent);
    CHECK(s_ddc.stream._p_task->__trigger);
}

static void _testDetachWhileReading(void)
{
    CHECK(_setUp(64, 0, 0));
    s_async = TRUE;
    _setDevice(s_data, 10);
    _attach(NULL, 0, TRUE);
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    CHECK((s_ddc.stream.__running && s_ddc.__protocol.__phase == UDP_COUNT));

    // The task runs on until the read is over, then waits for a client.
    pifTransport_DetachClient(&s_ddc.stream.parent);
    CHECK(!s_ddc.stream._p_task->_pause);
    _endTransfer(TRUE);
    CHECK(_doTask(s_ddc.stream._p_task) == 0);
    CHECK(s_ddc.stream._p_task->_pause);
    CHECK(!s_ddc.stream.__running && !s_port.__use_device);
    CHECK(!s_client.rx_size);

    // A new client gets what the device holds.
    _attach(NULL, 0, TRUE);
    s_async = FALSE;
    CHECK(_run());
    CHECK(s_client.rx_size == 10 && !memcmp(s_client.rx, s_data, 10));
}

static void _testClearWhileReading(void)
{
    CHECK(_setUp(64, 0, 0));
    s_async = TRUE;
    _attach(NULL, 0, TRUE);
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    _endTransfer(TRUE);

    pifUbxDdcStream_Clear(&s_ddc);
    // The read is over, so the device could leave the port.
    CHECK(!s_ddc._p_device && !s_ddc.stream._p_task && !s_port.__use_device);
    CHECK(pifObjArray_Count(&s_port.__devices) == 0);
}

// A protocol of the test: reads the count registers and gives them to the client as data.
static BOOL _countStart(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream)
{
    return pifDeviceReg_StartRead(p_stream->_p_device, 0xFD, 1, p_stream->__p_rx, 2);
}

static PifDeviceStreamNext _countFinish(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream, BOOL ok)
{
    if (ok) pifDeviceStream_SetRx(p_stream, 2);
    return DSN_WAIT;
}

static const PifDeviceStreamProtocolOps c_count_ops = { _countStart, _countFinish, NULL, NULL };

static void _testSetProtocol(void)
{
    PifDeviceStreamProtocol count_protocol, empty_protocol;

    CHECK(_setUp(64, 0, 0));
    pifDeviceStreamProtocol_Init(&count_protocol, &c_count_ops);
    memset(&empty_protocol, 0, sizeof(empty_protocol));
    _setDevice(s_data, 300);
    _attach(NULL, 0, TRUE);

    // Half way through, the stream is given the protocol of the test.
    _doTask(s_ddc.stream._p_task);
    // The client reads a byte a call, so it has 16 of the 64 read, and the rest go with the protocol.
    CHECK(s_ddc.__protocol.__pending == 300 - 64 && s_client.rx_size == 16);
    CHECK(!pifDeviceStream_SetProtocol(&s_ddc.stream, &empty_protocol) && pif_error == E_INVALID_PARAM);
    CHECK(pifDeviceStream_SetProtocol(&s_ddc.stream, &count_protocol) && s_ddc.stream._p_protocol == &count_protocol);
    _doTask(s_ddc.stream._p_task);
    CHECK(s_client.rx_size == 18 && s_client.rx[16] == 0x00 && s_client.rx[17] == 236);

    // A transfer belongs to the protocol that started it.
    s_async = TRUE;
    CHECK(_doTask(s_ddc.stream._p_task) == RECHECK_PERIOD1US);
    CHECK(!pifDeviceStream_SetProtocol(&s_ddc.stream, &s_ddc.__protocol.parent) && pif_error == E_INVALID_STATE);
    _endTransfer(TRUE);
    _doTask(s_ddc.stream._p_task);
    s_async = FALSE;

    // Back on the first protocol, what it knew of the device is forgotten and the count is read again.
    s_ddc.__protocol.__pending = 5;
    CHECK(pifDeviceStream_SetProtocol(&s_ddc.stream, &s_ddc.__protocol.parent) && !s_ddc.__protocol.__pending);
    s_client.rx_size = 0;
    CHECK(_run());
    CHECK(s_client.rx_size == 236 && !memcmp(s_client.rx, s_data + 64, 236));
}

static void _testInitErrors(void)
{
    _clearAll();
    CHECK(pifI2cPort_Init(&s_port, PIF_ID_AUTO, 1));

    CHECK(!pifUbxDdcStream_Init(NULL, PIF_ID_AUTO, &s_port, DEVICE_ADDR, 16, 16, 100));
    CHECK(!pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, NULL, DEVICE_ADDR, 16, 16, 100));
    CHECK(!pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, &s_port, DEVICE_ADDR, 0, 16, 100));
    CHECK(!pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, &s_port, DEVICE_ADDR, 16, 16, 0));
    // A failed init leaves nothing behind, so the port still has room.
    CHECK(pifObjArray_Count(&s_port.__devices) == 0 && !s_ddc._p_device && !s_ddc.stream._p_task);

    // A port without room for the receiver.
    CHECK(pifI2cPort_AddDevice(&s_port, PIF_ID_AUTO, DEVICE_ADDR + 1, NULL));
    CHECK(!pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, &s_port, DEVICE_ADDR, 16, 16, 100));
    CHECK(!s_ddc._p_device && !s_ddc.stream._p_task);

    // What the stream puts together.
    pifI2cPort_Clear(&s_port);
    CHECK(pifI2cPort_Init(&s_port, PIF_ID_AUTO, 1));
    CHECK(pifUbxDdcStream_Init(&s_ddc, PIF_ID_AUTO, &s_port, 0x42, 16, 16, 100));
    CHECK(s_ddc._p_device->addr == 0x42 && s_ddc.stream._p_device == &s_ddc._p_device->parent);
    CHECK(s_ddc.stream._p_protocol == &s_ddc.__protocol.parent && s_ddc.__protocol.__count_reg == 0xFD);
}

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testReceive, "receive" },
    { _testReceiveRxSizeLimit, "receive in reads of rx_size" },
    { _testReceiveAsync, "receive with reads in the background" },
    { _testNoData, "count 0 and 0xFFFF" },
    { _testReadError, "read error" },
    { _testSend, "send" },
    { _testSendWhilePortBusy, "send while the port is busy" },
    { _testSendAndReceive, "send and receive" },
    { _testTriggerSend, "trigger send" },
    { _testDetachWhileReading, "detach while reading" },
    { _testClearWhileReading, "clear while reading" },
    { _testSetProtocol, "set protocol" },
    { _testInitErrors, "init errors" },
};

int main(void)
{
    int failed = 0, i;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 1)) {
        printf("pif_ubx_ddc_stream: setup failed (%d)\n", pif_error);
        return 1;
    }
    for (i = 0; i < DATA_SIZE; i++) s_data[i] = (uint8_t)(i * 7 + (i >> 8));

    printf("pif_ubx_ddc_stream\n");
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
