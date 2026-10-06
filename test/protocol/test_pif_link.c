// SPDX-License-Identifier: BSD-3-Clause
// pif_link.c is included so the tests can drive its static parser and sender directly.
#include "../../source/protocol/pif_link.c"
#include "protocol/pif_link_fragment.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#define WIRE_SIZE		8192
#define MAX_DATA		65535

#define CMD_ECHO		0x41	// Answered with 'O', 'K', low byte of the data size.
#define CMD_FAIL		0x42	// Answered with LINK_E_FAILED.
#define CMD_NO_HANDLER	0x43	// In the question table without a handler.
#define CMD_UNKNOWN		0x44	// Not in the question table.

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

static PifTimerManager s_timer_manager;
static PifLink s_a, s_b;			// s_a sends the requests, s_b answers them.
static PifUart s_uart_a, s_uart_b;
static PifRingBuffer s_wire_ab, s_wire_ba;

// Frames sent by s_b are counted by their ETX, which appears nowhere else in a frame.
static int s_b_frames;
static int s_b_drop_frame;		// Index of the frame of s_b that is lost, or -1.
static BOOL s_b_mute;			// Every frame of s_b is lost.
static uint8_t s_b_stage[WIRE_SIZE];
static int s_b_stage_len;

// Receiver side.
static uint8_t s_rx_data[MAX_DATA];
static int s_rx_calls, s_rx_len, s_rx_bad;
static int s_stream_fail_offset;

// Sender side.
static int s_resp_calls, s_err_calls;
static PifLinkError s_last_error;
static uint8_t s_resp_data[8];

static uint8_t s_data[MAX_DATA];


static uint32_t _timer1us(void)
{
    return (uint32_t)clock() * (1000000 / CLOCKS_PER_SEC);
}

static uint16_t _sendA(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return pifRingBuffer_PutData(&s_wire_ab, p_data, size) ? size : 0;
}

static uint16_t _sendB(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    (void)p_uart;
    for (i = 0; i < size; i++) {
        s_b_stage[s_b_stage_len++] = p_data[i];
        if (p_data[i] == ASCII_ETX) {
            if (!s_b_mute && s_b_frames != s_b_drop_frame) pifRingBuffer_PutData(&s_wire_ba, s_b_stage, s_b_stage_len);
            s_b_frames++;
            s_b_stage_len = 0;
        }
        else if (p_data[i] == ASCII_NAK && s_b_stage_len == 1) {
            pifRingBuffer_PutData(&s_wire_ba, s_b_stage, 1);
            s_b_stage_len = 0;
        }
    }
    return size;
}

static uint16_t _receiveA(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return pifRingBuffer_GetBytes(&s_wire_ba, p_data, size);
}

static uint16_t _receiveB(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    return pifRingBuffer_GetBytes(&s_wire_ab, p_data, size);
}

static BOOL _isQuiet(void)
{
    return pifRingBuffer_IsEmpty(&s_wire_ab) && pifRingBuffer_IsEmpty(&s_wire_ba) &&
            pifRingBuffer_IsEmpty(&s_b.__tx.answer_buffer) && !s_b.__tx.answer_length;
}

// Runs both links until s_a has nothing left to send. A response timeout is simulated whenever s_a waits
// and nothing is on the way, since the timer task does not run here.
static BOOL _pump(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        _evtSending(&s_a, _sendA);
        // A real UART takes time to send, so the response never arrives before s_a waits for it.
        if (s_a.__tx.state == LINK_TS_WAIT_SENDED) _evtSending(&s_a, _sendA);
        _evtParsing(&s_b, _receiveB);
        _evtSending(&s_b, _sendB);
        _evtParsing(&s_a, _receiveA);
        if (s_a.__tx.state == LINK_TS_WAIT_RESPONSE && _isQuiet()) _evtTimerTxTimeout(&s_a);
        if (s_a.__tx.state == LINK_TS_IDLE && pifRingBuffer_IsEmpty(&s_a.__tx.request_buffer) && _isQuiet()) {
            return TRUE;
        }
    }
    return FALSE;
}

static void _evtQuestion(PifLink *p_link, PifLinkPacket *p_packet)
{
    uint8_t answer[3];

    s_rx_calls++;
    if (p_packet->command == CMD_FAIL) {
        pifLink_MakeError(p_link, p_packet, LINK_F_DEFAULT, LINK_E_FAILED);
        return;
    }

    if ((p_packet->flags & LINK_F_FRAGMENT) && s_b.__p_fragment->rx_mode == LINK_FM_STREAM) {
        if (p_packet->offset != s_rx_len) s_rx_bad++;
        if (s_stream_fail_offset >= 0 && p_packet->offset >= s_stream_fail_offset) {
            pifLink_MakeError(p_link, p_packet, LINK_F_DEFAULT, LINK_E_FAILED);
            return;
        }
        memcpy(s_rx_data + p_packet->offset, p_packet->p_data, p_packet->data_count);
        s_rx_len += p_packet->data_count;
        if (p_packet->more) return;
    }
    else {
        if (p_packet->more || p_packet->offset) s_rx_bad++;
        if (p_packet->data_count) memcpy(s_rx_data, p_packet->p_data, p_packet->data_count);
        s_rx_len = p_packet->data_count;
    }

    answer[0] = 'O';
    answer[1] = 'K';
    answer[2] = (uint8_t)s_rx_len;
    pifLink_MakeAnswer(p_link, p_packet, LINK_F_DEFAULT, answer, sizeof(answer));
}

static void _evtResponse(PifLink *p_link, PifLinkPacket *p_packet)
{
    (void)p_link;
    s_resp_calls++;
    if (p_packet && p_packet->data_count <= sizeof(s_resp_data)) {
        memcpy(s_resp_data, p_packet->p_data, p_packet->data_count);
    }
}

static void _evtError(PifLink *p_link, const PifLinkRequest *p_request, PifLinkError error)
{
    (void)p_link;
    (void)p_request;
    s_err_calls++;
    s_last_error = error;
}

static const PifLinkQuestion s_questions_a[] = { { 0 } };
static const PifLinkQuestion s_questions_b[] = {
    { CMD_ECHO, 0, _evtQuestion },
    { CMD_FAIL, 0, _evtQuestion },
    { CMD_NO_HANDLER, 0, NULL },
    { 0 }
};

static const PifLinkRequest s_req_echo = { CMD_ECHO, LINK_F_DEFAULT, 3, 100, _evtResponse };
static const PifLinkRequest s_req_echo_rle = { CMD_ECHO, LINK_F_RLE_YES, 3, 100, _evtResponse };
static const PifLinkRequest s_req_fail = { CMD_FAIL, LINK_F_DEFAULT, 3, 100, _evtResponse };
static const PifLinkRequest s_req_no_handler = { CMD_NO_HANDLER, LINK_F_DEFAULT, 3, 100, _evtResponse };
static const PifLinkRequest s_req_unknown = { CMD_UNKNOWN, LINK_F_DEFAULT, 3, 100, _evtResponse };

// Makes a pair of links joined by in-memory wires.
static BOOL _setUp(void)
{
    // A failed test returns before _tearDown(), so free what it left behind.
    pifLink_Clear(&s_a);
    pifLink_Clear(&s_b);
    // Removed timers are freed on the next tick.
    pifTimerManager_sigTick(&s_timer_manager);

    memset(&s_uart_a, 0, sizeof(s_uart_a));
    memset(&s_uart_b, 0, sizeof(s_uart_b));
    s_uart_a._fc_state = ON;
    s_uart_a._frame_size = 1;
    s_uart_b._fc_state = ON;
    s_uart_b._frame_size = 1;

    if (!pifLink_Init(&s_a, PIF_ID_AUTO, &s_timer_manager, LINK_T_SINGLE, 0, s_questions_a)) return FALSE;
    if (!pifLink_Init(&s_b, PIF_ID_AUTO, &s_timer_manager, LINK_T_SINGLE, 0, s_questions_b)) return FALSE;
    s_a.__p_uart = &s_uart_a;
    s_b.__p_uart = &s_uart_b;
    s_a.evt_error = _evtError;
    // s_a also parses the frames of the encoding tests, which carry up to 1000 bytes.
    if (!pifLink_ResizeTxRequest(&s_a, 256) || !pifLink_ResizeRxPacket(&s_a, 1000) ||
            !pifLink_ResizeTxResponse(&s_b, 64)) return FALSE;

    pifRingBuffer_Empty(&s_wire_ab);
    pifRingBuffer_Empty(&s_wire_ba);
    s_b_frames = 0;
    s_b_drop_frame = -1;
    s_b_mute = FALSE;
    s_b_stage_len = 0;
    s_rx_calls = s_rx_len = s_rx_bad = 0;
    s_stream_fail_offset = -1;
    s_resp_calls = s_err_calls = 0;
    s_last_error = LINK_E_NONE;
    memset(s_resp_data, 0, sizeof(s_resp_data));
    memset(s_rx_data, 0, sizeof(s_rx_data));
    return TRUE;
}

static void _tearDown(void)
{
    pifLink_Clear(&s_a);
    pifLink_Clear(&s_b);
}

// Encodes data as one request frame of s_a, optionally damages it, and parses it with s_a.
// Returns the frame length, -1 when the parser rejects it, or -2 when the decoded data differs.
#define DAMAGE_NONE			0
#define DAMAGE_RAW_STX		1	// The first DLE sequence is replaced by 'A' and a raw STX.
#define DAMAGE_NO_RLE_FLAG	2	// LINK_F_RLE is cleared from the header and the CRC is fixed up.

static int _roundTrip(uint8_t flags, uint8_t *p_data, uint16_t size, int damage)
{
    PifLink *p_link = &s_a;
    uint8_t crc_bytes[CRC_SIZE];
    uint16_t length, crc_pos, crc, fill, i;
    uint8_t *p_byte;

    pifRingBuffer_Empty(&s_wire_ba);
    length = pifLink_PutFrame(p_link, &s_wire_ba, flags, CMD_ECHO, 0x21, 0, NULL, p_data, size, &crc_pos);
    if (!length) return -3;

    fill = pifRingBuffer_GetFillSize(&s_wire_ba);
    if (damage == DAMAGE_RAW_STX) {
        for (i = p_link->__header_size; i < fill; i++) {
            p_byte = pifRingBuffer_GetTailPointer(&s_wire_ba, i);
            if (*p_byte == ASCII_DLE) {
                *p_byte = 'A';
                *pifRingBuffer_GetTailPointer(&s_wire_ba, i + 1) = ASCII_STX;
                break;
            }
        }
    }
    else if (damage == DAMAGE_NO_RLE_FLAG) {
        *pifRingBuffer_GetTailPointer(&s_wire_ba, 1) &= ~LINK_F_RLE_MASK;
        crc = CRC_INIT;
        for (i = 1; i < crc_pos; i++) crc = pifCrc16_Add(crc, *pifRingBuffer_GetTailPointer(&s_wire_ba, i));
        _encodeCrc(crc, crc_bytes);
        for (i = 0; i < CRC_SIZE; i++) *pifRingBuffer_GetTailPointer(&s_wire_ba, crc_pos + i) = crc_bytes[i];
    }

    p_link->__rx.state = LINK_RS_IDLE;
    _parsingPacket(p_link, _receiveA);
    if (p_link->__rx.state != LINK_RS_DONE) return -1;
    p_link->__rx.state = LINK_RS_IDLE;
    if (p_link->__rx.packet.length != size) return -2;
    if (size && memcmp(p_link->__rx.packet.p_data, p_data, size)) return -2;
    return length;
}


// ---- Encoding ----

// Header 6 bytes, CRC 3 bytes and ETX around the data.
#define FRAME_EXTRA		10

static void testEscape(void)
{
    uint8_t data[256];
    int i;

    CHECK(_setUp());
    for (i = 0; i < 256; i++) data[i] = (uint8_t)i;
    // Only STX, ETX, ACK, DLE, XON, XOFF and NAK take an extra byte.
    CHECK(_roundTrip(LINK_F_DEFAULT, data, 256, DAMAGE_NONE) == FRAME_EXTRA + 256 + 7);

    memset(data, 0, 64);
    CHECK(_roundTrip(LINK_F_DEFAULT, data, 64, DAMAGE_NONE) == FRAME_EXTRA + 64);

    memset(data, ASCII_XON, 10);
    CHECK(_roundTrip(LINK_F_DEFAULT, data, 10, DAMAGE_NONE) == FRAME_EXTRA + 20);

    CHECK(_roundTrip(LINK_F_DEFAULT, NULL, 0, DAMAGE_NONE) == FRAME_EXTRA);
    _tearDown();
}

static void testRunLength(void)
{
    uint8_t data[256];
    int i;

    CHECK(_setUp());
    for (i = 0; i < 256; i++) data[i] = (uint8_t)i;
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 256, DAMAGE_NONE) == FRAME_EXTRA + 256 + 7);

    memset(data, 0, 64);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 64, DAMAGE_NONE) == FRAME_EXTRA + 3);

    memset(data, ASCII_XON, 10);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 10, DAMAGE_NONE) == FRAME_EXTRA + 4);

    // A run holds 3 to 130 bytes.
    memset(data, 0x55, 133);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 130, DAMAGE_NONE) == FRAME_EXTRA + 3);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 131, DAMAGE_NONE) == FRAME_EXTRA + 4);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 133, DAMAGE_NONE) == FRAME_EXTRA + 6);

    data[0] = 'a';
    data[1] = 'a';
    data[2] = 'b';
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 3, DAMAGE_NONE) == FRAME_EXTRA + 3);

    memset(data, ASCII_DLE, 3);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 3, DAMAGE_NONE) == FRAME_EXTRA + 4);
    _tearDown();
}

static void testRandomFrames(void)
{
    static uint8_t data[1000];
    int frame, size, k, n;
    uint8_t value;

    CHECK(_setUp());
    srand(1);
    for (frame = 0; frame < 2000; frame++) {
        size = rand() % (int)sizeof(data);
        k = 0;
        while (k < size) {
            n = 1 + (rand() % 4 ? 0 : rand() % 200);
            value = rand() % 3 ? (uint8_t)rand() : (uint8_t)(ASCII_STX + rand() % 0x14);
            while (n-- && k < size) data[k++] = value;
        }
        CHECK(_roundTrip((frame & 1) ? LINK_F_RLE_YES : LINK_F_DEFAULT, data, (uint16_t)size, DAMAGE_NONE) > 0);
    }
    _tearDown();
}

static void testInvalidFrames(void)
{
    uint8_t data[256];
    int i;

    CHECK(_setUp());
    for (i = 0; i < 256; i++) data[i] = (uint8_t)i;
    CHECK(_roundTrip(LINK_F_DEFAULT, data, 256, DAMAGE_RAW_STX) == -1);

    memset(data, 0, 64);
    CHECK(_roundTrip(LINK_F_RLE_YES, data, 64, DAMAGE_NO_RLE_FLAG) == -1);
    _tearDown();
}


// ---- Requests ----

static void testRequestAnswer(void)
{
    CHECK(_setUp());
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_echo, s_data, 20));
    CHECK(_pump());
    CHECK(s_rx_calls == 1 && s_rx_len == 20 && !memcmp(s_rx_data, s_data, 20));
    CHECK(s_resp_calls == 1 && s_err_calls == 0);
    CHECK(s_resp_data[0] == 'O' && s_resp_data[2] == 20);
    _tearDown();
}

static void testErrorAnswers(void)
{
    CHECK(_setUp());
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_fail, s_data, 4));
    CHECK(_pump());
    CHECK(s_err_calls == 1 && s_last_error == LINK_E_FAILED && s_resp_calls == 0);

    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_no_handler, s_data, 4));
    CHECK(_pump());
    CHECK(s_err_calls == 2 && s_last_error == LINK_E_UNSUPPORTED);

    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_unknown, s_data, 4));
    CHECK(_pump());
    CHECK(s_err_calls == 3 && s_last_error == LINK_E_UNSUPPORTED);
    _tearDown();
}

static void testLostAnswer(void)
{
    CHECK(_setUp());
    s_b_drop_frame = 0;
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_echo, s_data, 20));
    CHECK(_pump());
    // The retry carries LINK_F_RETRY, so the kept answer is sent again without running the handler twice.
    CHECK(s_rx_calls == 1);
    CHECK(s_resp_calls == 1 && s_err_calls == 0);
    _tearDown();
}

static void testTimeout(void)
{
    CHECK(_setUp());
    s_b_mute = TRUE;
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_echo, s_data, 4));
    CHECK(_pump());
    // The first attempt and its 3 retries all reach s_b, which handles the question only once.
    CHECK(s_b_frames == 4 && s_rx_calls == 1);
    CHECK(s_err_calls == 1 && s_last_error == LINK_E_TIMEOUT && s_resp_calls == 0);
    _tearDown();
}


// ---- Fragments ----

static void testFragmentNotEnabled(void)
{
    CHECK(_setUp());
    CHECK(!pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(pif_error == E_INVALID_STATE);
    CHECK(!pifLink_SetTxFragmentSize(&s_a, 10));
    CHECK(s_a.__p_fragment == NULL && s_a.__p_fragment_ops == NULL);

    // The receiver has not enabled fragmentation either.
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(s_a.__p_fragment->tx_size == PIF_LINK_TX_FRAGMENT_SIZE);
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_err_calls == 1 && s_last_error == LINK_E_UNSUPPORTED);
    CHECK(s_rx_calls == 0 && s_b_frames == 1);
    _tearDown();
}

static void testFragmentReassemble(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, 1000));
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_rx_calls == 1 && s_rx_len == 300 && !memcmp(s_rx_data, s_data, 300) && !s_rx_bad);
    CHECK(s_resp_calls == 1 && s_err_calls == 0);
    CHECK(s_resp_data[0] == 'O' && s_resp_data[2] == (uint8_t)300);
    CHECK(s_b_frames == (300 + PIF_LINK_TX_FRAGMENT_SIZE - 1) / PIF_LINK_TX_FRAGMENT_SIZE);
    _tearDown();
}

static void testFragmentStream(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_STREAM, 0));
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_rx_calls == (300 + PIF_LINK_TX_FRAGMENT_SIZE - 1) / PIF_LINK_TX_FRAGMENT_SIZE);
    CHECK(s_rx_len == 300 && !memcmp(s_rx_data, s_data, 300) && !s_rx_bad);
    CHECK(s_resp_calls == 1 && s_err_calls == 0 && s_resp_data[0] == 'O');
    _tearDown();
}

static void testFragmentRejected(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));

    // Receive mode LINK_FM_NONE.
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_NONE, 0));
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_err_calls == 1 && s_last_error == LINK_E_UNSUPPORTED && s_rx_calls == 0);

    // Larger than the reassembly buffer.
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, 100));
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_err_calls == 2 && s_last_error == LINK_E_INVALID_PARAM && s_rx_calls == 0);

    // Command without a handler.
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_no_handler, s_data, 300));
    CHECK(_pump());
    CHECK(s_err_calls == 3 && s_last_error == LINK_E_UNSUPPORTED);
    _tearDown();
}

static void testFragmentLostAnswer(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, 1000));
    s_b_drop_frame = 3;
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_rx_calls == 1 && s_rx_len == 300 && !memcmp(s_rx_data, s_data, 300));
    CHECK(s_resp_calls == 1 && s_err_calls == 0);

    // In stream mode the handler is not called again for the retried fragment.
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_STREAM, 0));
    s_rx_calls = s_rx_len = 0;
    s_b_frames = 0;
    s_b_drop_frame = 5;
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_rx_calls == (300 + PIF_LINK_TX_FRAGMENT_SIZE - 1) / PIF_LINK_TX_FRAGMENT_SIZE);
    CHECK(s_rx_len == 300 && !memcmp(s_rx_data, s_data, 300) && !s_rx_bad);
    CHECK(s_resp_calls == 2 && s_err_calls == 0);
    _tearDown();
}

static void testFragmentStreamError(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_STREAM, 0));
    s_stream_fail_offset = 124;
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 300));
    CHECK(_pump());
    CHECK(s_err_calls == 1 && s_last_error == LINK_E_FAILED && s_resp_calls == 0);
    _tearDown();
}

static void testFragmentLargeRle(void)
{
    uint16_t size = 199;

    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, MAX_DATA));
    CHECK(pifLink_ResizeRxPacket(&s_b, size + 1));
    CHECK(pifLink_SetTxFragmentSize(&s_a, size));
    memset(s_data + 1000, 0, 5000);
    memset(s_data + 7000, ASCII_DLE, 300);
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo_rle, s_data, MAX_DATA));
    CHECK(_pump());
    CHECK(s_rx_calls == 1 && s_rx_len == MAX_DATA && !memcmp(s_rx_data, s_data, MAX_DATA));
    CHECK(s_resp_calls == 1 && s_err_calls == 0);
    // More than 127 fragments, so seq wraps from 127 back to 1.
    CHECK(s_b_frames == (MAX_DATA + size - 1) / size && s_b_frames > 127);
    _tearDown();
}

static void testFragmentMixedQueue(void)
{
    CHECK(_setUp());
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, 1000));
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_echo, s_data, 20));
    CHECK(pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 500));
    CHECK(pifLink_MakeRequest(&s_a, 0, &s_req_echo, s_data, 10));
    CHECK(_pump());
    CHECK(s_resp_calls == 3 && s_err_calls == 0);
    CHECK(s_rx_calls == 3 && s_rx_len == 10);
    _tearDown();
}

static void testFragmentParameters(void)
{
    CHECK(_setUp());
    CHECK(!pifLink_EnableFragment(&s_b, LINK_FM_REASSEMBLE, 0));
    CHECK(s_b.__p_fragment == NULL);
    CHECK(pifLink_EnableFragment(&s_a, LINK_FM_NONE, 0));
    CHECK(!pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, s_data, 0));
    CHECK(!pifLink_MakeLargeRequest(&s_a, 0, &s_req_echo, NULL, 10));
    CHECK(!pifLink_MakeLargeRequest(&s_a, 0, NULL, s_data, 10));
    CHECK(!pifLink_SetTxFragmentSize(&s_a, 0));
    CHECK(!pifLink_SetTxFragmentSize(&s_a, PIF_LINK_MAX_DATA_SIZE));
    CHECK(pifLink_SetTxFragmentSize(&s_a, PIF_LINK_MAX_DATA_SIZE - 1));
    _tearDown();
    CHECK(s_a.__p_fragment == NULL && s_a.__p_fragment_ops == NULL);
}


typedef struct StTestCase
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "escape", testEscape },
    { "run-length", testRunLength },
    { "random frames", testRandomFrames },
    { "invalid frames", testInvalidFrames },
    { "request and answer", testRequestAnswer },
    { "error answers", testErrorAnswers },
    { "lost answer", testLostAnswer },
    { "timeout", testTimeout },
    { "fragment not enabled", testFragmentNotEnabled },
    { "fragment reassemble", testFragmentReassemble },
    { "fragment stream", testFragmentStream },
    { "fragment rejected", testFragmentRejected },
    { "fragment lost answer", testFragmentLostAnswer },
    { "fragment stream error", testFragmentStreamError },
    { "fragment large with rle", testFragmentLargeRle },
    { "fragment mixed queue", testFragmentMixedQueue },
    { "fragment parameters", testFragmentParameters }
};

int main(void)
{
    int failed = 0, i;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 2) ||
            !pifTimerManager_Init(&s_timer_manager, PIF_ID_AUTO, 1000, 8) ||
            !pifRingBuffer_InitHeap(&s_wire_ab, PIF_ID_AUTO, WIRE_SIZE) ||
            !pifRingBuffer_InitHeap(&s_wire_ba, PIF_ID_AUTO, WIRE_SIZE)) {
        printf("pif_link: setup failed (%d)\n", pif_error);
        return 1;
    }
    for (i = 0; i < MAX_DATA; i++) s_data[i] = (uint8_t)(i * 7 + (i >> 8));

#ifdef PIF_NO_LOG
    printf("pif_link (PIF_NO_LOG)\n");
#else
    printf("pif_link\n");
#endif
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
