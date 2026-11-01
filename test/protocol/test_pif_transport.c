// SPDX-License-Identifier: BSD-3-Clause
// pif_xmodem over each PifTransport: a pair of PifUart and a pair of PifCanStream.
// The tasks of the transports are run directly, the way the task manager would.
#include "communication/pif_can_stream.h"
#include "communication/pif_uart.h"
#include "protocol/pif_xmodem.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


#define X_PACKETS		3
#define X_DATA_SIZE		128	// Data of an XMODEM packet.

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

typedef enum
{
    MEDIUM_UART,
    MEDIUM_CAN,
    MEDIUM_CAN_FD
} Medium;

static BOOL s_failed;


// The transports. a and b are joined: what one sends the other receives.
static PifUart s_uart_a, s_uart_b;
static PifCan s_can_a, s_can_b;
static PifCanStream s_stream_a, s_stream_b;
static PifTransport *s_p_a, *s_p_b;
static PifTask *s_tasks[4];
static int s_task_count;

static uint8_t s_data[X_PACKETS * X_DATA_SIZE];

// pif_xmodem
static PifXmodem s_x_tx, s_x_rx;
static uint8_t s_x_data[X_PACKETS * X_DATA_SIZE];
static int s_x_received, s_x_nak;
static BOOL s_x_eot, s_x_tx_finish, s_x_rx_finish, s_x_cancel;


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static uint16_t _uartSendA(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return pifUart_PutRxData(&s_uart_b, p_data, size) ? size : 0;
}

static uint16_t _uartSendB(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return pifUart_PutRxData(&s_uart_a, p_data, size) ? size : 0;
}

static BOOL _canSendA(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    return pifCan_PutRxFrame(&s_can_b, p_frame);
}

static BOOL _canSendB(PifCan *p_can, const PifCanFrame *p_frame)
{
    (void)p_can;
    return pifCan_PutRxFrame(&s_can_a, p_frame);
}

static BOOL _addTask(PifTask *p_task)
{
    if (!p_task) return FALSE;
    s_tasks[s_task_count++] = p_task;
    return TRUE;
}

static void _runTasks(void)
{
    int i;

    for (i = 0; i < s_task_count; i++) (*s_tasks[i]->__evt_loop)(s_tasks[i]);
}

static void _tearDownMedium(void)
{
    pifCanStream_Clear(&s_stream_a);
    pifCanStream_Clear(&s_stream_b);
    pifCan_Clear(&s_can_a);
    pifCan_Clear(&s_can_b);
    pifUart_Clear(&s_uart_a);
    pifUart_Clear(&s_uart_b);
    s_task_count = 0;
}

static BOOL _setUpUart(PifUart *p_uart, PifActUartSendData act_send_data)
{
    if (!pifUart_Init(p_uart, PIF_ID_AUTO, 115200) || !pifUart_AllocRxBuffer(p_uart, 1024)) return FALSE;
    p_uart->act_send_data = act_send_data;
    // The tasks come before any client, which the UART takes only while it has them.
    if (!_addTask(pifUart_AttachRxTask(p_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL))) return FALSE;
    return _addTask(pifUart_AttachTxTask(p_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL));
}

static BOOL _setUpCan(PifCan *p_can, PifActCanSendFrame act_send_frame)
{
    if (!pifCan_Init(p_can, PIF_ID_AUTO, 500000, 2000000, 8, 8)) return FALSE;
    p_can->act_send_frame = act_send_frame;
    if (!_addTask(pifCan_AttachRxTask(p_can, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL))) return FALSE;
    return _addTask(pifCan_AttachTxTask(p_can, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL));
}

static BOOL _setUpMedium(Medium medium)
{
    uint8_t flags = medium == MEDIUM_CAN_FD ? CAN_F_FD | CAN_F_BRS : CAN_F_NONE;
    uint8_t frame_size = medium == MEDIUM_CAN_FD ? 64 : 8;

    _tearDownMedium();

    if (medium == MEDIUM_UART) {
        if (!_setUpUart(&s_uart_a, _uartSendA) || !_setUpUart(&s_uart_b, _uartSendB)) return FALSE;
        s_p_a = &s_uart_a.parent;
        s_p_b = &s_uart_b.parent;
        return TRUE;
    }

    if (!_setUpCan(&s_can_a, _canSendA) || !_setUpCan(&s_can_b, _canSendB)) return FALSE;
    // The receive buffer holds a whole XMODEM packet with room to spare.
    if (!pifCanStream_Init(&s_stream_a, PIF_ID_AUTO, &s_can_a, 0x100, 0x101, flags, frame_size, 1024)) return FALSE;
    if (!pifCanStream_Init(&s_stream_b, PIF_ID_AUTO, &s_can_b, 0x101, 0x100, flags, frame_size, 1024)) return FALSE;
    s_p_a = &s_stream_a.parent;
    s_p_b = &s_stream_b.parent;
    return TRUE;
}


static void _evtXTxReceive(uint8_t code, uint8_t packet_no)
{
    switch (code) {
    case 'C':
        pifXmodem_SendData(&s_x_tx, 1, s_data, X_DATA_SIZE);
        break;

    case ASCII_ACK:
        if (packet_no < X_PACKETS) {
            pifXmodem_SendData(&s_x_tx, packet_no + 1, s_data + packet_no * X_DATA_SIZE, X_DATA_SIZE);
        }
        else {
            pifXmodem_SendEot(&s_x_tx);
        }
        break;

    case ASCII_NAK:
        s_x_nak++;
        pifXmodem_SendData(&s_x_tx, packet_no, s_data + (packet_no - 1) * X_DATA_SIZE, X_DATA_SIZE);
        break;

    default:
        s_x_cancel = TRUE;
        break;
    }
}

static void _evtXRxReceive(uint8_t code, PifXmodemPacket *p_packet)
{
    uint8_t no;

    switch (code) {
    case ASCII_SOH:
        no = p_packet->packet_no[0];
        if (no >= 1 && no <= X_PACKETS) memcpy(s_x_data + (no - 1) * X_DATA_SIZE, p_packet->p_data, X_DATA_SIZE);
        s_x_received++;
        break;

    case ASCII_EOT:
        s_x_eot = TRUE;
        break;

    default:
        s_x_cancel = TRUE;
        break;
    }
}

static void _evtXTxFinish(uint16_t delay)
{
    (void)delay;
    s_x_tx_finish = TRUE;
}

static void _evtXRxFinish(uint16_t delay)
{
    (void)delay;
    s_x_rx_finish = TRUE;
}

static void _testXmodem(Medium medium)
{
    int n;

    CHECK(_setUpMedium(medium));
    CHECK(pifXmodem_Init(&s_x_tx, PIF_ID_AUTO, XT_CRC) && pifXmodem_Init(&s_x_rx, PIF_ID_AUTO, XT_CRC));
    pifXmodem_AttachEvtTx(&s_x_tx, _evtXTxReceive, _evtXTxFinish);
    pifXmodem_AttachEvtRx(&s_x_rx, _evtXRxReceive, _evtXRxFinish);
    CHECK(pifXmodem_AttachTransport(&s_x_tx, s_p_a) && pifXmodem_AttachTransport(&s_x_rx, s_p_b));
    memset(s_x_data, 0, sizeof(s_x_data));
    s_x_received = s_x_nak = 0;
    s_x_eot = s_x_tx_finish = s_x_rx_finish = s_x_cancel = FALSE;

    CHECK(pifXmodem_ReadyReceive(&s_x_rx));
    for (n = 0; n < 20000 && !(s_x_tx_finish && s_x_rx_finish) && !s_x_cancel; n++) _runTasks();
    CHECK(s_x_tx_finish && s_x_rx_finish && s_x_eot && !s_x_cancel);
    CHECK(s_x_received == X_PACKETS && !s_x_nak);
    CHECK(!memcmp(s_x_data, s_data, sizeof(s_x_data)));

    pifXmodem_Clear(&s_x_tx);
    pifXmodem_Clear(&s_x_rx);
    _tearDownMedium();
}

static void _testXmodemUart(void) { _testXmodem(MEDIUM_UART); }
static void _testXmodemCan(void) { _testXmodem(MEDIUM_CAN); }
static void _testXmodemCanFd(void) { _testXmodem(MEDIUM_CAN_FD); }


static void _evtAbortRxNone(void *p_client)
{
    (void)p_client;
}

static void _testUartTransport(void)
{
    CHECK(_setUpMedium(MEDIUM_UART));

    // A client attached later replaces the one before it, together with its evt_abort_rx, and detaching
    // it leaves the UART without a client.
    pifTransport_AttachClient(&s_uart_a.parent, &s_x_rx, NULL, NULL, _evtAbortRxNone);
    CHECK(pifXmodem_Init(&s_x_tx, PIF_ID_AUTO, XT_CRC));
    CHECK(!pifXmodem_AttachTransport(&s_x_tx, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(pifXmodem_AttachTransport(&s_x_tx, &s_uart_a.parent));
    CHECK(s_x_tx.__p_transport == &s_uart_a.parent && s_uart_a.__p_client == &s_x_tx);
    CHECK(s_uart_a.__evt_parsing && s_uart_a.__evt_sending && !s_uart_a.__evt_abort_rx);
    pifXmodem_DetachTransport(&s_x_tx);
    CHECK(!s_x_tx.__p_transport && !s_uart_a.__p_client && !s_uart_a.__evt_parsing && !s_uart_a.__evt_sending);
    pifXmodem_Clear(&s_x_tx);

    // The UART reports its own state through the transport.
    pifUart_SetFrameSize(&s_uart_a, 4);
    CHECK(pifTransport_GetFrameSize(&s_uart_a.parent) == 4);
    CHECK(pifTransport_GetByteTime(&s_uart_a.parent) == s_uart_a._transfer_time);
    CHECK(pifTransport_GetFlowState(&s_uart_a.parent) == ON);
    _tearDownMedium();
}

static int s_starts;
static BOOL s_send_at_once;

static uint16_t _evtSendingNone(void *p_client, PifActTransportSend act_send_data)
{
    (void)p_client;
    (void)act_send_data;
    return 0;
}

// Starts a transfer; with s_send_at_once the transfer also ends before this returns, the way a short one can
// end in the interrupt.
static BOOL _actStartTransferCount(PifUart *p_uart)
{
    uint8_t *p_data;
    uint16_t length = 0;

    if (!(pifUart_StartGetTxData(p_uart, &p_data, &length) & PIF_UART_SEND_DATA_STATE_DATA)) return FALSE;
    s_starts++;
    if (s_send_at_once) pifUart_EndGetTxData(p_uart, length);
    return TRUE;
}

static void _testUartTxBufferShared(void)
{
    static uint8_t frame[] = { 1, 2, 3 };
    PifUart uart;
    PifTask *p_task;

    CHECK(pifUart_Init(&uart, PIF_ID_AUTO, 115200) && pifUart_AllocTxBuffer(&uart, 64));
    uart.act_start_transfer = _actStartTransferCount;
    p_task = pifUart_AttachTxTask(&uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL);
    CHECK(p_task);
    pifTransport_AttachClient(&uart.parent, NULL, NULL, _evtSendingNone, NULL);

    // Bytes put beside a client that has nothing to send still start the transfer.
    s_starts = 0;
    s_send_at_once = FALSE;
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame));
    CHECK((*p_task->__evt_loop)(p_task) == sizeof(frame) * uart._transfer_time);
    CHECK(s_starts == 1 && uart.__tx_state == UTS_SENDING);
    CHECK(pifUart_EndGetTxData(&uart, sizeof(frame)) & PIF_UART_SEND_DATA_STATE_EMPTY);
    CHECK(uart.__tx_state == UTS_IDLE);

    // A transfer that ends before act_start_transfer() returns leaves the UART idle, so the next bytes go out.
    s_send_at_once = TRUE;
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame));
    (*p_task->__evt_loop)(p_task);
    CHECK(s_starts == 2 && uart.__tx_state == UTS_IDLE);
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame));
    (*p_task->__evt_loop)(p_task);
    CHECK(s_starts == 3 && uart.__tx_state == UTS_IDLE);

    pifUart_Clear(&uart);
}

#ifdef PIF_USE_UART_DIRECTION

static uint32_t s_fake_time;
static PifUartDirection s_dir;
static int s_dir_calls;

static uint32_t _fakeTimer1us(void)
{
    return s_fake_time;
}

static void _actDirection(PifUartDirection direction)
{
    s_dir = direction;
    s_dir_calls++;
}

static BOOL _actStartTransfer(PifUart *p_uart)
{
    (void)p_uart;
    return TRUE;
}

// What the TX interrupt does: takes the whole TX buffer out.
static void _drainTx(PifUart *p_uart)
{
    uint8_t data;

    while (!(pifUart_GetTxByte(p_uart, &data) & PIF_UART_SEND_DATA_STATE_EMPTY));
}

static void _testUartDirection(void)
{
    static uint8_t frame[] = { 1, 2, 3 };
    PifActTimer1us act_timer = pif_act_timer1us;
    PifUart uart;
    PifTask *p_task;
    uint32_t byte_time, period;

    s_fake_time = 1000;
    pif_act_timer1us = _fakeTimer1us;

    CHECK(pifUart_Init(&uart, PIF_ID_AUTO, 115200) && pifUart_AllocTxBuffer(&uart, 64));
    uart.act_start_transfer = _actStartTransfer;
    p_task = pifUart_AttachTxTask(&uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL);
    CHECK(p_task);
    byte_time = uart._transfer_time;
    CHECK(uart.dir_delay == 2);

    s_dir_calls = 0;
    pifUart_AttachActDirection(&uart, _actDirection, UD_RX);
    CHECK(s_dir == UD_RX && s_dir_calls == 1 && uart._direction == UD_RX);

    // The line is driven before the bytes go to the TX buffer, and kept while they are in it.
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame));
    CHECK(s_dir == UD_TX && s_dir_calls == 2);
    CHECK((*p_task->__evt_loop)(p_task) && s_dir == UD_TX);

    // Given back dir_delay byte times after the buffer gets empty, not before.
    _drainTx(&uart);
    period = (*p_task->__evt_loop)(p_task);
    CHECK(period == 2 * byte_time && s_dir == UD_TX);
    s_fake_time += byte_time;
    CHECK((*p_task->__evt_loop)(p_task) == byte_time && s_dir == UD_TX);
    s_fake_time += byte_time;
    CHECK((*p_task->__evt_loop)(p_task) == 0 && s_dir == UD_RX && s_dir_calls == 3);

    // More bytes during the wait keep the line and start the wait over.
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame));
    _drainTx(&uart);
    (*p_task->__evt_loop)(p_task);
    s_fake_time += byte_time;
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame) && s_dir_calls == 4);
    (*p_task->__evt_loop)(p_task);
    _drainTx(&uart);
    s_fake_time += byte_time;
    CHECK((*p_task->__evt_loop)(p_task) == 2 * byte_time && s_dir == UD_TX);
    s_fake_time += 2 * byte_time;
    CHECK((*p_task->__evt_loop)(p_task) == 0 && s_dir == UD_RX && s_dir_calls == 5);

    // A line held from outside stays driven through a transfer.
    pifUart_SetDirection(&uart, UD_TX);
    CHECK(pifUart_SendTxData(&uart, frame, sizeof(frame)) == sizeof(frame) && s_dir_calls == 6);
    _drainTx(&uart);
    (*p_task->__evt_loop)(p_task);
    s_fake_time += 10 * byte_time;
    CHECK((*p_task->__evt_loop)(p_task) == 0 && s_dir == UD_TX && s_dir_calls == 6);
    pifUart_SetDirection(&uart, UD_RX);

    pifUart_Clear(&uart);
    pif_act_timer1us = act_timer;
}

#endif	// PIF_USE_UART_DIRECTION

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "xmodem over uart", _testXmodemUart },
    { "xmodem over can", _testXmodemCan },
    { "xmodem over can fd", _testXmodemCanFd },
    { "uart transport", _testUartTransport },
    { "uart tx buffer shared with a client", _testUartTxBufferShared },
#ifdef PIF_USE_UART_DIRECTION
    { "uart rs-485 direction", _testUartDirection },
#endif
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(16, 2);
}

int main(void)
{
    int failed = 0, i;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_transport: setup failed (%d)\n", pif_error);
        return 1;
    }
    for (i = 0; i < (int)sizeof(s_data); i++) s_data[i] = (uint8_t)(i * 7 + (i >> 3));

    printf("pif_transport\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
