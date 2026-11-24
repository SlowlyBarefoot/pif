// SPDX-License-Identifier: BSD-3-Clause
// pif_gps_ublox over an in-memory wire. The UART tasks are not run: a packet is put on the wire and the
// parser the driver attached to its UART is called directly, and the transmit callback is called the way
// the TX task does so the queued commands come back off the wire.
#include "core/pif_task_manager.h"
#include "gps/pif_gps_ublox.h"

#include <math.h>
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

#define CLOSE(VALUE, TARGET, TOLERANCE)		(fabs((VALUE) - (TARGET)) <= (TOLERANCE))

typedef struct
{
    uint8_t data[WIRE_SIZE];
    uint16_t head, tail;
} Wire;

static BOOL s_failed;

static PifUart s_uart;
static Wire s_rx, s_tx;			// Towards the host, and towards the receiver
static PifGpsUblox s_ublox;
static PifGps *s_p_gps;

static int s_receives, s_packets, s_errors, s_nmea_sentences;
static PifGpsUbxError s_last_error;
static uint8_t s_last_class, s_last_msg;


static uint32_t _timer1us(void)
{
    return pif_cumulative_timer1ms * 1000;
}

static uint16_t _actReceive(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    (void)p_transport;
    for (i = 0; i < size && s_rx.head != s_rx.tail; i++) p_data[i] = s_rx.data[s_rx.head++ % WIRE_SIZE];
    return i;
}

static uint16_t _actSend(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    (void)p_transport;
    for (i = 0; i < size; i++) s_tx.data[s_tx.tail++ % WIRE_SIZE] = p_data[i];
    return size;
}

// Hands the bytes on the wire to the parser, as the UART RX task does.
static void _parse(void)
{
    int i;

    for (i = 0; i < WIRE_SIZE; i++) {
        if (!(*s_uart.__evt_parsing)(s_uart.__p_client, _actReceive)) break;
        if (s_rx.head == s_rx.tail) break;
    }
}

// Calls the transmit callback the way the UART TX task does, until it has nothing more to hand over.
static void _send(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        if (!(*s_uart.__evt_sending)(s_uart.__p_client, _actSend)) break;
    }
}

static void _put(const uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    for (i = 0; i < size; i++) s_rx.data[s_rx.tail++ % WIRE_SIZE] = p_data[i];
}

static uint16_t _sent(void)
{
    return s_tx.tail - s_tx.head;
}

// Builds a UBX packet with its checksum and puts it on the wire towards the host.
static void _putUbx(uint8_t class_id, uint8_t msg_id, const uint8_t *p_payload, uint16_t length)
{
    uint8_t head[6] = { 0xB5, 0x62 };
    uint8_t ck[2] = { 0, 0 };
    uint16_t i;

    head[2] = class_id;
    head[3] = msg_id;
    head[4] = length & 0xFF;
    head[5] = length >> 8;
    for (i = 2; i < 6; i++) {
        ck[0] += head[i];
        ck[1] += ck[0];
    }
    for (i = 0; i < length; i++) {
        ck[0] += p_payload[i];
        ck[1] += ck[0];
    }
    _put(head, 6);
    if (length) _put(p_payload, length);
    _put(ck, 2);
}

static BOOL _evtUbxReceive(PifGpsUblox *p_owner, PifGpsUbxPacket *p_packet)
{
    (void)p_owner;
    s_packets++;
    s_last_class = p_packet->class_id;
    s_last_msg = p_packet->msg_id;
    return TRUE;
}

static void _evtUbxError(PifGpsUblox *p_owner, PifGpsUbxError error)
{
    (void)p_owner;
    s_errors++;
    s_last_error = error;
}

static void _evtReceive(PifGps *p_owner)
{
    (void)p_owner;
    s_receives++;
}

static BOOL _evtNmeaReceive(PifGps *p_owner, PifGpsNmeaMsgId msg_id)
{
    (void)p_owner;
    if (msg_id != PIF_GPS_NMEA_MSG_ID_ERR) s_nmea_sentences++;
    return FALSE;
}

static BOOL _setup(void)
{
    memset(&s_rx, 0, sizeof(s_rx));
    memset(&s_tx, 0, sizeof(s_tx));
    s_receives = s_packets = s_errors = s_nmea_sentences = 0;
    s_last_class = s_last_msg = 0;
    pif_cumulative_timer1ms = 1000;

    pifGpsUblox_Clear(&s_ublox);
    if (!pifGpsUblox_Init(&s_ublox, PIF_ID_AUTO)) return FALSE;
    s_p_gps = &s_ublox._gps;
    s_ublox.evt_ubx_receive = _evtUbxReceive;
    s_ublox.evt_ubx_error = _evtUbxError;
    s_p_gps->evt_receive = _evtReceive;
    s_p_gps->evt_nmea_receive = _evtNmeaReceive;
    return pifGpsUblox_AttachTransport(&s_ublox, &s_uart.parent);
}


// --- The payload structures ---

static void test_payload_layout(void)
{
    // The payload is read in place, so each structure has to be the size the protocol gives its message,
    // with nothing the compiler padded in between.
    CHECK(sizeof(PifGpsUbxNavPosllh) == 28);
    CHECK(sizeof(PifGpsUbxNavPvt) == 92);
    CHECK(sizeof(PifGpsUbxNavSol) == 52);
    CHECK(sizeof(PifGpsUbxNavStatus) == 16);
    CHECK(sizeof(PifGpsUbxNavSvInfoChannel) == 12);
    CHECK(sizeof(PifGpsUbxNavSvInfo) == 8 + 32 * 12);
    CHECK(sizeof(PifGpsUbxNavTimeUtc) == 20);
    CHECK(sizeof(PifGpsUbxNavVelned) == 36);
}


// --- The NAV messages ---

static void test_nav_posllh(void)
{
    PifGpsUbxNavPosllh posllh;
    PifGpsUbxNavStatus status;

    CHECK(_setup());
    memset(&status, 0, sizeof(status));
    status.gps_fix = 3;			// 3D
    status.flags = 0x01;		// The fix is valid
    _putUbx(GUCI_NAV, GUMI_NAV_STATUS, (uint8_t *)&status, sizeof(status));
    _parse();
    CHECK(s_packets == 1);

    memset(&posllh, 0, sizeof(posllh));
    posllh.lon = 1131000000;	// 113.1 deg
    posllh.lat = 374500000;		// 37.45 deg
    posllh.h_msl = 123456;		// mm
    posllh.h_acc = 2500;
    posllh.v_acc = 3500;
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    _parse();
    CHECK(s_packets == 2);
    CHECK(s_receives == 2);
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LON], 113.1, 1e-7));
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LAT], 37.45, 1e-7));
    CHECK(CLOSE(s_p_gps->_altitude, 123.456, 1e-9));
    CHECK(s_p_gps->_horizontal_acc == 2500);
    CHECK(s_p_gps->_vertical_acc == 3500);
    // POSLLH says nothing of the fix: NAV-STATUS of the same epoch did.
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_update_rate[1] == pif_cumulative_timer1ms);
}

static void test_nav_status_no_fix(void)
{
    PifGpsUbxNavStatus status;

    CHECK(_setup());
    memset(&status, 0, sizeof(status));
    status.gps_fix = 3;
    status.flags = 0x01;
    _putUbx(GUCI_NAV, GUMI_NAV_STATUS, (uint8_t *)&status, sizeof(status));
    _parse();
    s_p_gps->_fix = TRUE;

    // A 2D fix is not one, and the valid bit alone is not either.
    status.gps_fix = 2;
    _putUbx(GUCI_NAV, GUMI_NAV_STATUS, (uint8_t *)&status, sizeof(status));
    _parse();
    CHECK(!s_p_gps->_fix);

    s_p_gps->_fix = TRUE;
    status.gps_fix = 3;
    status.flags = 0x00;
    _putUbx(GUCI_NAV, GUMI_NAV_STATUS, (uint8_t *)&status, sizeof(status));
    _parse();
    CHECK(!s_p_gps->_fix);
}

static void test_nav_pvt(void)
{
    PifGpsUbxNavPvt pvt;

    CHECK(_setup());
    memset(&pvt, 0, sizeof(pvt));
    pvt.year = 2026;
    pvt.month = 11;
    pvt.day = 23;
    pvt.hour = 14;
    pvt.min = 25;
    pvt.sec = 36;
    pvt.nano = 250000000;		// 250 ms
    pvt.fix_type = 3;
    pvt.flags = 0x01;
    pvt.num_sv = 12;
    pvt.lon = 1270123456;		// 127.0123456 deg
    pvt.lat = 374512345;
    pvt.h_msl = 56789;			// mm
    pvt.h_acc = 1200;
    pvt.v_acc = 1800;
    pvt.g_speed = 12345;		// mm/s
    pvt.head_mot = 18012345;	// 180.12345 deg
    pvt.p_dop = 140;			// 1.40
    _putUbx(GUCI_NAV, GUMI_NAV_PVT, (uint8_t *)&pvt, sizeof(pvt));
    _parse();

    CHECK(s_packets == 1);
    // The year of PifDateTime counts from 2000.
    CHECK(s_p_gps->_utc.year == 26);
    CHECK(s_p_gps->_utc.month == 11 && s_p_gps->_utc.day == 23);
    CHECK(s_p_gps->_utc.hour == 14 && s_p_gps->_utc.minute == 25 && s_p_gps->_utc.second == 36);
    CHECK(s_p_gps->_utc.millisecond == 250);
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_num_sat == 12);
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LON], 127.0123456, 1e-7));
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LAT], 37.4512345, 1e-7));
    CHECK(CLOSE(s_p_gps->_altitude, 56.789, 1e-9));
    CHECK(CLOSE(s_p_gps->_ground_speed, 1234.5, 1e-9));		// mm/s to cm/s
    CHECK(CLOSE(s_p_gps->_ground_course, 180.12345, 1e-6));
    CHECK(s_p_gps->_hdop == 140);
}

static void test_nav_pvt_negative_nano(void)
{
    PifGpsUbxNavPvt pvt;

    CHECK(_setup());
    memset(&pvt, 0, sizeof(pvt));
    pvt.year = 2026;
    pvt.month = 1;
    pvt.day = 1;
    pvt.sec = 30;
    // The fraction of the second runs from -1e9 to 1e9: the second is the nearest one, so a negative
    // fraction leaves nothing to add to it.
    pvt.nano = -125000000;
    _putUbx(GUCI_NAV, GUMI_NAV_PVT, (uint8_t *)&pvt, sizeof(pvt));
    _parse();
    CHECK(s_p_gps->_utc.second == 30);
    CHECK(s_p_gps->_utc.millisecond == 0);
}

static void test_nav_velned(void)
{
    PifGpsUbxNavVelned velned;

    CHECK(_setup());
    memset(&velned, 0, sizeof(velned));
    velned.speed = 500;		// 3-D, which climbing adds to
    velned.g_speed = 400;	// 2-D, over the ground
    velned.heading = 9012345;	// 90.12345 deg
    _putUbx(GUCI_NAV, GUMI_NAV_VELNED, (uint8_t *)&velned, sizeof(velned));
    _parse();
    CHECK(CLOSE(s_p_gps->_ground_speed, 400, 1e-9));
    CHECK(CLOSE(s_p_gps->_ground_course, 90.12345, 1e-6));
}

static void test_nav_timeutc(void)
{
    PifGpsUbxNavTimeUtc time_utc;

    CHECK(_setup());
    memset(&time_utc, 0, sizeof(time_utc));
    time_utc.year = 2026;
    time_utc.month = 3;
    time_utc.day = 4;
    time_utc.hour = 5;
    time_utc.min = 6;
    time_utc.sec = 7;
    time_utc.nano = 8000000;
    // Without the valid bit the time is not taken.
    _putUbx(GUCI_NAV, GUMI_NAV_TIMEUTC, (uint8_t *)&time_utc, sizeof(time_utc));
    _parse();
    CHECK(s_p_gps->_utc.year == 0);

    time_utc.valid = 0x04;
    _putUbx(GUCI_NAV, GUMI_NAV_TIMEUTC, (uint8_t *)&time_utc, sizeof(time_utc));
    _parse();
    CHECK(s_p_gps->_utc.year == 26 && s_p_gps->_utc.month == 3 && s_p_gps->_utc.day == 4);
    CHECK(s_p_gps->_utc.hour == 5 && s_p_gps->_utc.minute == 6 && s_p_gps->_utc.second == 7);
    CHECK(s_p_gps->_utc.millisecond == 8);
}

static void test_nav_svinfo(void)
{
    PifGpsUbxNavSvInfo sv_info;
    uint16_t length;
    int i;

    CHECK(_setup());
    memset(&sv_info, 0, sizeof(sv_info));
    sv_info.num_ch = 20;
    for (i = 0; i < 20; i++) {
        sv_info.channel[i].chn = i;
        sv_info.channel[i].svid = 10 + i;
        sv_info.channel[i].quality = 4;
        sv_info.channel[i].cno = 30 + i;
    }
    length = 8 + 20 * sizeof(PifGpsUbxNavSvInfoChannel);
    _putUbx(GUCI_NAV, GUMI_NAV_SVINFO, (uint8_t *)&sv_info, length);
    _parse();

    CHECK(s_packets == 1);
    // The satellite view goes where the NMEA GSV sentences put it, and only as many as there is room for.
    CHECK(s_p_gps->_sv_num_sv == PIF_GPS_SV_MAXSATS);
    CHECK(s_p_gps->_sv_chn[0] == 0 && s_p_gps->_sv_svid[0] == 10 && s_p_gps->_sv_cno[0] == 30);
    CHECK(s_p_gps->_sv_quality[0] == 4);
    CHECK(s_p_gps->_sv_svid[PIF_GPS_SV_MAXSATS - 1] == 10 + PIF_GPS_SV_MAXSATS - 1);
    CHECK(s_p_gps->_sv_received_count == 1);
}

static void test_other_class(void)
{
    uint8_t payload[4] = { 1, 2, 3, 4 };

    CHECK(_setup());
    // A class the driver does not decode is no error: the client reads it through evt_ubx_receive.
    _putUbx(GUCI_MON, GUMI_MON_VER, payload, sizeof(payload));
    _parse();
    CHECK(s_packets == 1);
    CHECK(s_errors == 0);
    CHECK(s_last_class == GUCI_MON && s_last_msg == GUMI_MON_VER);
}


// --- Framing ---

static void test_wrong_checksum(void)
{
    PifGpsUbxNavPosllh posllh;
    uint16_t i;

    CHECK(_setup());
    memset(&posllh, 0, sizeof(posllh));
    posllh.lat = 100000000;
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    // Break the last byte of the checksum.
    i = (s_rx.tail - 1) % WIRE_SIZE;
    s_rx.data[i] ^= 0xFF;
    _parse();
    CHECK(s_packets == 0);
    CHECK(s_errors == 1);
    CHECK(s_last_error == GUE_WRONG_CRC);

    // The parser is ready for the next packet.
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    _parse();
    CHECK(s_packets == 1);
}

static void test_big_length(void)
{
    uint8_t head[6] = { 0xB5, 0x62, GUCI_NAV, GUMI_NAV_SAT, 0xFF, 0xFF };

    CHECK(_setup());
    _put(head, sizeof(head));
    _parse();
    CHECK(s_errors == 1);
    CHECK(s_last_error == GUE_BIG_LENGTH);
    CHECK(s_packets == 0);
}

static void test_resync(void)
{
    PifGpsUbxNavPosllh posllh;
    uint8_t stray[1] = { 0xB5 };

    CHECK(_setup());
    memset(&posllh, 0, sizeof(posllh));
    // A stray first sync character right before a packet: the 0xB5 that follows it starts the packet.
    _put(stray, 1);
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    _parse();
    CHECK(s_errors == 1);
    CHECK(s_last_error == GUE_INVALID_DATA);
    CHECK(s_packets == 1);
}

static void test_nmea_between_packets(void)
{
    PifGpsUbxNavPosllh posllh;
    const char *p_sentence = "$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68\r\n";

    CHECK(_setup());
    memset(&posllh, 0, sizeof(posllh));
    _put((const uint8_t *)p_sentence, (uint16_t)strlen(p_sentence));
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    _parse();
    // One driver takes a receiver however it is set to talk.
    CHECK(s_nmea_sentences == 1);
    CHECK(s_packets == 1);
    CHECK(s_errors == 0);
    CHECK(s_p_gps->_num_sat == 8);
}

static void test_abort_rx(void)
{
    uint8_t head[4] = { 0xB5, 0x62, GUCI_NAV, GUMI_NAV_POSLLH };
    PifGpsUbxNavPosllh posllh;

    CHECK(_setup());
    _put(head, sizeof(head));
    _parse();
    // The transport lost bytes, so the packet being parsed is broken and the parser starts over.
    (*s_uart.__evt_abort_rx)(s_uart.__p_client);
    memset(&posllh, 0, sizeof(posllh));
    _putUbx(GUCI_NAV, GUMI_NAV_POSLLH, (uint8_t *)&posllh, sizeof(posllh));
    _parse();
    CHECK(s_packets == 1);
    CHECK(s_errors == 0);
}


// --- Commands ---

static void test_send_ubx(void)
{
    uint8_t payload[6] = { 200, 0, 1, 0, 1, 0 };	// CFG-RATE: 200 ms, one measurement, GPS time
    uint8_t ack[2] = { GUCI_CFG, GUMI_CFG_RATE };
    uint8_t sent[32];
    uint16_t i, length;
    uint8_t ck_a = 0, ck_b = 0;

    CHECK(_setup());
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_RATE, sizeof(payload), payload, 1000));
    // Queued is not sent: the request is on its way until pifGpsUblox_CheckRequest() says otherwise.
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);
    _send();

    length = _sent();
    CHECK(length == 8 + sizeof(payload));
    for (i = 0; i < length; i++) sent[i] = s_tx.data[(s_tx.head + i) % WIRE_SIZE];
    CHECK(sent[0] == 0xB5 && sent[1] == 0x62);
    CHECK(sent[2] == GUCI_CFG && sent[3] == GUMI_CFG_RATE);
    CHECK(sent[4] == sizeof(payload) && sent[5] == 0);
    CHECK(!memcmp(sent + 6, payload, sizeof(payload)));
    for (i = 2; i < length - 2; i++) {
        ck_a += sent[i];
        ck_b += ck_a;
    }
    CHECK(sent[length - 2] == ck_a && sent[length - 1] == ck_b);

    // A configuration message is answered, so the request waits for the ACK that names it.
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);
    _putUbx(GUCI_ACK, GUMI_ACK_ACK, ack, sizeof(ack));
    _parse();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_ACK);
}

static void test_nak_and_other_ack(void)
{
    uint8_t payload[1] = { 0 };
    uint8_t other[2] = { GUCI_CFG, GUMI_CFG_SBAS };
    uint8_t ack[2] = { GUCI_CFG, GUMI_CFG_NAV5 };

    CHECK(_setup());
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_NAV5, sizeof(payload), payload, 1000));
    _send();
    // An acknowledgement of another message is not this request answered.
    _putUbx(GUCI_ACK, GUMI_ACK_ACK, other, sizeof(other));
    _parse();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);

    _putUbx(GUCI_ACK, GUMI_ACK_NAK, ack, sizeof(ack));
    _parse();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_NAK);
}

static void test_request_timeout(void)
{
    uint8_t payload[1] = { 0 };

    CHECK(_setup());
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_NAV5, sizeof(payload), payload, 100));
    _send();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);
    pif_cumulative_timer1ms += 100;
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_TIMEOUT);
    // A request that is over leaves room for the next one.
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_NAV5, sizeof(payload), payload, 100));
}

static void test_one_request_at_a_time(void)
{
    uint8_t payload[1] = { 0 };

    CHECK(_setup());
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_NAV5, sizeof(payload), payload, 1000));
    // There is one transmit buffer and one request state, so the second is refused and the first keeps
    // its state.
    CHECK(!pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_SBAS, sizeof(payload), payload, 1000));
    CHECK(pif_error == E_INVALID_STATE);
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);
}

static void test_unanswered_message(void)
{
    uint8_t payload[1] = { 0 };

    CHECK(_setup());
    // Only a configuration message is acknowledged. For the others the request is over once the last
    // byte has left.
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_NAV, GUMI_NAV_PVT, sizeof(payload), payload, 1000));
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_SEND);
    _send();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_TIMEOUT);
    CHECK(_sent() == 8 + sizeof(payload));
}

static void test_poll_request(void)
{
    char sentence[32];
    uint16_t i, length;

    CHECK(_setup());
    CHECK(pifGpsUblox_PollRequestGPQ(&s_ublox, "RMC", 1000));
    _send();
    length = _sent();
    for (i = 0; i < length; i++) sentence[i] = (char)s_tx.data[(s_tx.head + i) % WIRE_SIZE];
    sentence[length] = 0;
    // The checksum of "GPGPQ,RMC" is 0x21.
    CHECK(!strcmp(sentence, "$GPGPQ,RMC*21\r\n"));

    CHECK(_setup());
    CHECK(pifGpsUblox_PollRequestGLQ(&s_ublox, "GSV", 1000));
    _send();
    length = _sent();
    for (i = 0; i < length; i++) sentence[i] = (char)s_tx.data[(s_tx.head + i) % WIRE_SIZE];
    sentence[length] = 0;
    CHECK(!strcmp(sentence, "$GLGLQ,GSV*3F\r\n"));

    // A message ID that would not leave room for the checksum and the end of line is refused rather
    // than written past the end of the sentence.
    CHECK(_setup());
    CHECK(!pifGpsUblox_PollRequestGPQ(&s_ublox, "ABCDEFGHIJKLMNOP", 1000));
    CHECK(pif_error == E_OVERFLOW_BUFFER);
}

static void test_pubx_rate(void)
{
    char sentence[48];
    uint16_t i, length;
    uint8_t parity = 0;

    CHECK(_setup());
    CHECK(pifGpsUblox_SetPubxRate(&s_ublox, "GGA", 0, 1, 0, 0, 0, 1000));
    _send();
    length = _sent();
    CHECK(length < sizeof(sentence));
    for (i = 0; i < length; i++) sentence[i] = (char)s_tx.data[(s_tx.head + i) % WIRE_SIZE];
    sentence[length] = 0;
    CHECK(!strncmp(sentence, "$PUBX,40,GGA,0,1,0,0,0,0*", 25));
    CHECK(sentence[length - 2] == '\r' && sentence[length - 1] == '\n');
    for (i = 1; sentence[i] != '*'; i++) parity ^= (uint8_t)sentence[i];
    CHECK(sentence[length - 4] == kPifHexUpperChar[parity >> 4]);
    CHECK(sentence[length - 3] == kPifHexUpperChar[parity & 0x0F]);
}

static void test_two_messages_queued(void)
{
    uint8_t payload[2] = { 1, 2 };
    uint16_t length;

    CHECK(_setup());
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_NAV, GUMI_NAV_PVT, sizeof(payload), payload, 1000));
    _send();
    CHECK(pifGpsUblox_CheckRequest(&s_ublox) == GURS_TIMEOUT);
    length = _sent();
    // The second message goes out whole after the first one, each headed by its own length.
    CHECK(pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_NAV, GUMI_NAV_SOL, sizeof(payload), payload, 1000));
    _send();
    CHECK(_sent() == length + 8 + sizeof(payload));
    CHECK(s_tx.data[(s_tx.head + length + 3) % WIRE_SIZE] == GUMI_NAV_SOL);
}

static void test_no_transport(void)
{
    uint8_t payload[1] = { 0 };

    pifGpsUblox_Clear(&s_ublox);
    CHECK(pifGpsUblox_Init(&s_ublox, PIF_ID_AUTO));
    // Nothing to send through, so the command fails and says why.
    CHECK(!pifGpsUblox_SendUbxMsg(&s_ublox, GUCI_CFG, GUMI_CFG_NAV5, sizeof(payload), payload, 1000));
    CHECK(pif_error == E_CANNOT_FOUND);
    CHECK(s_ublox._request_state == GURS_FAILURE);
}

static void test_init_errors(void)
{
    CHECK(!pifGpsUblox_Init(NULL, PIF_ID_AUTO));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(_setup());
    CHECK(!pifGpsUblox_AttachTransport(&s_ublox, NULL));
    CHECK(pif_error == E_INVALID_PARAM);
}


static struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { test_payload_layout, "payload layout" },
    { test_nav_posllh, "NAV-POSLLH" },
    { test_nav_status_no_fix, "NAV-STATUS without a 3D fix" },
    { test_nav_pvt, "NAV-PVT" },
    { test_nav_pvt_negative_nano, "NAV-PVT with a negative fraction" },
    { test_nav_velned, "NAV-VELNED" },
    { test_nav_timeutc, "NAV-TIMEUTC" },
    { test_nav_svinfo, "NAV-SVINFO" },
    { test_other_class, "a class not decoded" },
    { test_wrong_checksum, "wrong checksum" },
    { test_big_length, "payload over the buffer" },
    { test_resync, "resync on a stray sync character" },
    { test_nmea_between_packets, "NMEA between the packets" },
    { test_abort_rx, "lost bytes" },
    { test_send_ubx, "send a UBX command" },
    { test_nak_and_other_ack, "NAK, and an ACK of another message" },
    { test_request_timeout, "request timeout" },
    { test_one_request_at_a_time, "one request at a time" },
    { test_unanswered_message, "a message that is not answered" },
    { test_poll_request, "NMEA poll request" },
    { test_pubx_rate, "PUBX rate" },
    { test_two_messages_queued, "two messages queued" },
    { test_no_transport, "no transport" },
    { test_init_errors, "init errors" },
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(4, 2) &&
            pifUart_Init(&s_uart, PIF_ID_AUTO, 115200) &&
            pifUart_AttachRxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL) &&
            pifUart_AttachTxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_gps_ublox: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_gps_ublox\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifGpsUblox_Clear(&s_ublox);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
