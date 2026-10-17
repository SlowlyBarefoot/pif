// SPDX-License-Identifier: BSD-3-Clause
// Runs the S.BUS, Spektrum and SUMD receivers over an in-memory wire. The UART tasks are not run:
// SendFrame puts its frame on the wire, the parser the receiver attached to its UART is called
// directly, and the 1 ms clock is set by the test.
#include "core/pif_task_manager.h"
#include "rc/pif_rc_sbus.h"
#include "rc/pif_rc_spektrum.h"
#include "rc/pif_rc_sumd.h"

#include <stdio.h>
#include <string.h>


#define WIRE_SIZE		1024

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
    uint8_t data[WIRE_SIZE];
    uint16_t head, tail;
} Wire;

static BOOL s_failed;
static uint32_t s_now;

static PifUart s_uart;
static Wire s_wire;

static int s_receives;
static uint16_t s_channel[32];


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _put(const uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size; i++) s_wire.data[s_wire.tail++ % WIRE_SIZE] = p_data[i];
}

static uint16_t _receive(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    (void)p_transport;
    for (i = 0; i < size && s_wire.head != s_wire.tail; i++) p_data[i] = s_wire.data[s_wire.head++ % WIRE_SIZE];
    return i;
}

static uint16_t _actSend(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    _put(p_data, size);
    return size;
}

static uint16_t _pending(void)
{
    return s_wire.tail - s_wire.head;
}

// Calls the parser the way the UART RX task does: again and again while it reports progress.
static void _parse(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (!(*s_uart.__evt_parsing)(s_uart.__p_client, _receive)) break;
        if (!_pending()) break;
    }
}

// Moves the clock on by ms, calling the parser every millisecond as the RX task polls an open frame.
static void _idle(uint32_t ms)
{
    while (ms--) {
        pif_cumulative_timer1ms++;
        (*s_uart.__evt_parsing)(s_uart.__p_client, _receive);
    }
}

static void _evtReceive(PifRc *p_owner, uint16_t *p_channel, PifIssuerP p_issuer)
{
    (void)p_issuer;
    memcpy(s_channel, p_channel, p_owner->_channel_count * sizeof(uint16_t));
    s_receives++;
}

static void _reset(void)
{
    memset(&s_wire, 0, sizeof(s_wire));
    memset(s_channel, 0xEE, sizeof(s_channel));
    s_receives = 0;
    pif_cumulative_timer1ms = 1000;
}


// --- S.BUS ---

static PifRcSbus s_sbus;

static BOOL _setupSbus(void)
{
    _reset();
    if (!pifRcSbus_Init(&s_sbus, PIF_ID_AUTO)) return FALSE;
    pifRc_AttachEvtReceive(&s_sbus.parent, _evtReceive, NULL);
    pifRcSbus_AttachUart(&s_sbus, &s_uart);
    return TRUE;
}

// Takes a frame off the wire that the receiver just attached sends, and clears the wire again.
static void _sbusFrame(uint8_t *p_frame)
{
    uint16_t raw[16];
    int i;

    for (i = 0; i < 16; i++) raw[i] = 172 + 100 * i;
    pifRcSbus_SendFrame(&s_sbus, raw, 16);
    _receive(NULL, p_frame, SBUS_FRAME_SIZE);
    _reset();
}

static void test_sbus_round_trip(void)
{
    uint16_t raw[16];
    int i;

    CHECK(_setupSbus());
    for (i = 0; i < 16; i++) raw[i] = 172 + 100 * i;
    CHECK(pifRcSbus_SendFrame(&s_sbus, raw, 16));
    CHECK(_pending() == SBUS_FRAME_SIZE);
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_channel[0] == 987);					// 172 -> 987.5 us, rounded away from the centre
    CHECK(s_channel[8] == 1520 + (((172 + 800 - 1024) * 5 - 4) / 8));
    CHECK(s_channel[16] == 988);				// digital channels off
    CHECK(s_channel[17] == 988);
    CHECK(s_sbus.parent._good_frames == 1);
    CHECK(!s_sbus.parent._failsafe);
}

static void test_sbus_cut_frame(void)
{
    uint8_t frame[SBUS_FRAME_SIZE];

    CHECK(_setupSbus());
    _sbusFrame(frame);
    // Half a frame, then the RX task keeps polling the open frame through the gap.
    _put(frame, 10);
    _parse();
    _idle(5);
    _put(frame, SBUS_FRAME_SIZE);
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_sbus.parent._error_frames == 0);
}

static void test_sbus_resync(void)
{
    uint8_t frame[SBUS_FRAME_SIZE];
    uint8_t junk[] = { 0x0F, 0x11, 0x22, 0x33 };

    CHECK(_setupSbus());
    _sbusFrame(frame);
    // A stray start byte right before a frame, with no pause between them.
    _put(junk, sizeof(junk));
    _put(frame, SBUS_FRAME_SIZE);
    _parse();
    CHECK(s_sbus.parent._error_frames == 1);
    CHECK(s_receives == 1);
    CHECK(s_channel[0] == 987);
}

static void test_sbus2_end_byte(void)
{
    uint8_t frame[SBUS_FRAME_SIZE];

    CHECK(_setupSbus());
    _sbusFrame(frame);
    frame[SBUS_FRAME_SIZE - 1] = 0x24;
    _put(frame, SBUS_FRAME_SIZE);
    _parse();
    CHECK(s_receives == 1);
    frame[SBUS_FRAME_SIZE - 1] = 0x05;
    pif_cumulative_timer1ms += 10;
    _put(frame, SBUS_FRAME_SIZE);
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_sbus.parent._error_frames == 1);
}

static void test_sbus_link_loss(void)
{
    uint8_t frame[SBUS_FRAME_SIZE];

    CHECK(_setupSbus());
    _sbusFrame(frame);
    _put(frame, SBUS_FRAME_SIZE);
    _parse();
    CHECK(!pifRc_CheckFailSafe(&s_sbus.parent));
    pif_cumulative_timer1ms += 101;
    CHECK(pifRc_CheckFailSafe(&s_sbus.parent));
}

static void test_sbus_no_uart(void)
{
    PifRcSbus sbus;
    uint16_t raw[16] = { 0 };

    CHECK(pifRcSbus_Init(&sbus, PIF_ID_AUTO));
    CHECK(!pifRcSbus_SendFrame(&sbus, raw, 16));
    pifRcSbus_DetachUart(&sbus);
}


// --- Spektrum ---

static PifRcSpektrum s_spektrum;

static BOOL _setupSpektrum(uint8_t protocol_id)
{
    _reset();
    if (!pifRcSpektrum_Init(&s_spektrum, PIF_ID_AUTO, protocol_id)) return FALSE;
    pifRc_AttachEvtReceive(&s_spektrum.parent, _evtReceive, NULL);
    pifRcSpektrum_AttachUart(&s_spektrum, &s_uart);
    return TRUE;
}

static void test_spektrum_round_trip(void)
{
    uint16_t raw[8] = { 0, 200, 400, 600, 800, 1000, 1200, 2047 };

    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 8));
    CHECK(_pending() == SPEKTRUM_FRAME_SIZE);
    _parse();
    // Channel 8 comes in the second frame, so nothing is handed out yet.
    CHECK(s_receives == 0);
    pif_cumulative_timer1ms += 11;
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 8));
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_channel[0] == 988);
    CHECK(s_channel[1] == 988 + 100);
    CHECK(s_channel[6] == 988 + 600);
    CHECK(s_channel[7] == 988 + 1023);
}

static void test_spektrum_short_count(void)
{
    uint16_t raw[8] = { 100, 200, 300, 400, 500, 600, 700, 800 };
    uint8_t frame[SPEKTRUM_FRAME_SIZE];

    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 4));
    CHECK(_pending() == SPEKTRUM_FRAME_SIZE);
    _receive(NULL, frame, SPEKTRUM_FRAME_SIZE);
    CHECK(frame[10] == 0xFF && frame[11] == 0xFF);
    CHECK(frame[14] == 0xFF && frame[15] == 0xFF);

    // After 8 channels moved the last slot to channel 8, 7 channels must go back to channel 7.
    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 8));
    CHECK(s_spektrum.__last_ch == 7);
    _reset();
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 7));
    _receive(NULL, frame, SPEKTRUM_FRAME_SIZE);
    CHECK(((frame[14] >> 3) & 0x0F) == 6);
}

static void test_spektrum_wrong_system(void)
{
    uint16_t raw[8] = { 100, 200, 300, 400, 500, 600, 700, 800 };
    uint8_t frame[SPEKTRUM_FRAME_SIZE];
    uint8_t junk[] = { 0x00, 0x55, 0x66 };

    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 7));
    _receive(NULL, frame, SPEKTRUM_FRAME_SIZE);

    // A frame of another protocol is not decoded with the wrong layout.
    frame[1] = PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2;
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    CHECK(s_spektrum.parent._good_frames == 0);
    CHECK(s_spektrum.parent._error_frames == 1);

    // Junk before frames, with no pause: one error, then the frames are found.
    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS));
    frame[1] = PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS;
    _put(junk, sizeof(junk));
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    _parse();
    CHECK(s_spektrum.parent._error_frames == 1);
    CHECK(s_spektrum.parent._good_frames == 2);
    CHECK(s_receives == 1);
    CHECK(s_channel[2] == 988 + 150);
}

static void test_spektrum_cut_frame(void)
{
    uint16_t raw[8] = { 100, 200, 300, 400, 500, 600, 700, 800 };
    uint8_t frame[SPEKTRUM_FRAME_SIZE];

    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 7));
    _receive(NULL, frame, SPEKTRUM_FRAME_SIZE);

    _put(frame, 7);
    _parse();
    _idle(10);
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    pif_cumulative_timer1ms += 22;
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    CHECK(s_spektrum.parent._good_frames == 2);
    CHECK(s_receives == 1);
    CHECK(s_channel[0] == 988 + 50);
}

static void test_spektrum_fades(void)
{
    uint16_t raw[8] = { 100, 200, 300, 400, 500, 600, 700, 800 };
    uint8_t frame[SPEKTRUM_FRAME_SIZE];

    CHECK(_setupSpektrum(PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS));
    CHECK(pifRcSpektrum_SendFrame(&s_spektrum, raw, 7));
    _receive(NULL, frame, SPEKTRUM_FRAME_SIZE);

    frame[0] = 250;
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    frame[0] = 3;					// the counter wrapped after 9 lost frames
    pif_cumulative_timer1ms += 22;
    _put(frame, SPEKTRUM_FRAME_SIZE);
    _parse();
    CHECK(s_spektrum.parent._lost_frames == 9);
}


// --- SUMD ---

static PifRcSumd s_sumd;

static BOOL _setupSumd(void)
{
    _reset();
    if (!pifRcSumd_Init(&s_sumd, PIF_ID_AUTO)) return FALSE;
    pifRc_AttachEvtReceive(&s_sumd.parent, _evtReceive, NULL);
    pifRcSumd_AttachUart(&s_sumd, &s_uart);
    return TRUE;
}

static void test_sumd_round_trip(void)
{
    uint16_t raw[10];
    int i;

    CHECK(_setupSumd());
    for (i = 0; i < 10; i++) raw[i] = (1000 + 100 * i) * 8;
    CHECK(pifRcSumd_SendFrame(&s_sumd, raw, 10));
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_sumd.parent._channel_count == 10);
    CHECK(s_channel[0] == 1000);
    CHECK(s_channel[9] == 1900);
    CHECK(!s_sumd.parent._failsafe);
}

static void test_sumd_count(void)
{
    uint16_t raw[PIF_SUMD_CHANNEL_COUNT + 1] = { 0 };

    CHECK(_setupSumd());
    CHECK(!pifRcSumd_SendFrame(&s_sumd, raw, PIF_SUMD_CHANNEL_COUNT + 1));
    CHECK(!pifRcSumd_SendFrame(&s_sumd, raw, 1));
    CHECK(_pending() == 0);
    CHECK(pifRcSumd_SendFrame(&s_sumd, raw, PIF_SUMD_CHANNEL_COUNT));
    _parse();
    CHECK(s_receives == 1);
}

static void test_sumd_bad_crc(void)
{
    uint16_t raw[8] = { 12000, 12000, 12000, 12000, 12000, 12000, 12000, 12000 };
    uint8_t frame[3 + 16 + 2];

    CHECK(_setupSumd());
    CHECK(pifRcSumd_SendFrame(&s_sumd, raw, 8));
    _parse();
    CHECK(s_sumd.parent._channel_count == 8);
    CHECK(!s_sumd.parent._failsafe);

    // A corrupted frame that claims failsafe and 4 channels changes nothing.
    CHECK(pifRcSumd_SendFrame(&s_sumd, raw, 8));
    _receive(NULL, frame, sizeof(frame));
    frame[1] = 0x81;
    frame[2] = 4;
    pif_cumulative_timer1ms += 10;
    _put(frame, 3 + 8 + 2);
    _parse();
    CHECK(s_sumd.parent._error_frames == 1);
    CHECK(s_sumd.parent._channel_count == 8);
    CHECK(!s_sumd.parent._failsafe);
    CHECK(s_receives == 1);
}

static void test_sumd_cut_frame(void)
{
    uint16_t raw[8] = { 12000, 12000, 12000, 12000, 12000, 12000, 12000, 12000 };
    uint8_t frame[3 + 16 + 2];

    CHECK(_setupSumd());
    CHECK(pifRcSumd_SendFrame(&s_sumd, raw, 8));
    _receive(NULL, frame, sizeof(frame));

    _put(frame, 9);
    _parse();
    _idle(6);
    _put(frame, sizeof(frame));
    _parse();
    CHECK(s_receives == 1);
    CHECK(s_sumd.parent._error_frames == 0);
    CHECK(s_channel[3] == 1500);
}


static const struct
{
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "sbus round trip", test_sbus_round_trip },
    { "sbus cut frame", test_sbus_cut_frame },
    { "sbus resync", test_sbus_resync },
    { "sbus2 end byte", test_sbus2_end_byte },
    { "sbus link loss", test_sbus_link_loss },
    { "sbus no uart", test_sbus_no_uart },
    { "spektrum round trip", test_spektrum_round_trip },
    { "spektrum short count", test_spektrum_short_count },
    { "spektrum wrong system", test_spektrum_wrong_system },
    { "spektrum cut frame", test_spektrum_cut_frame },
    { "spektrum fades", test_spektrum_fades },
    { "sumd round trip", test_sumd_round_trip },
    { "sumd count", test_sumd_count },
    { "sumd bad crc", test_sumd_bad_crc },
    { "sumd cut frame", test_sumd_cut_frame },
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us) || !pifTaskManager_Init(4, 2) ||
            !pifUart_Init(&s_uart, PIF_ID_AUTO, 115200) ||
            !pifUart_AttachRxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL) ||
            !pifUart_AttachTxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL)) {
        printf("pif_rc_serial: setup failed (%d)\n", pif_error);
        return 1;
    }
    s_uart.act_send_data = _actSend;

    printf("pif_rc_serial\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
