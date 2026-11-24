// SPDX-License-Identifier: BSD-3-Clause
// The NMEA 0183 parser of pif_gps, and pif_gps_nmea over an in-memory wire. The UART tasks are not run:
// a sentence is put on the wire and the parser the receiver attached to its UART is called directly.
#include "core/pif_task_manager.h"
#include "gps/pif_gps_nmea.h"

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
static Wire s_wire;
static PifGpsNmea s_nmea;
static PifGps *s_p_gps;

static int s_receives, s_sentences, s_errors;
static PifGpsNmeaMsgId s_last_msg_id;
static char s_frame[256];
static int s_frame_len;
static PifGpsNmeaTxt s_txt;
static int s_texts;
static int s_timeouts;


static uint32_t _timer1us(void)
{
    return pif_cumulative_timer1ms * 1000;
}

static uint16_t _receive(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
    uint16_t i;

    (void)p_transport;
    for (i = 0; i < size && s_wire.head != s_wire.tail; i++) p_data[i] = s_wire.data[s_wire.head++ % WIRE_SIZE];
    return i;
}

// Calls the parser the way the UART RX task does: again and again while it reports progress.
static void _parse(void)
{
    int i;

    for (i = 0; i < WIRE_SIZE; i++) {
        if (!(*s_uart.__evt_parsing)(s_uart.__p_client, _receive)) break;
        if (s_wire.head == s_wire.tail) break;
    }
}

// Puts a sentence on the wire with its end of line, and lets the receiver read it.
static void _send(const char *p_sentence)
{
    const char *p;

    for (p = p_sentence; *p; p++) s_wire.data[s_wire.tail++ % WIRE_SIZE] = *p;
    s_wire.data[s_wire.tail++ % WIRE_SIZE] = '\r';
    s_wire.data[s_wire.tail++ % WIRE_SIZE] = '\n';
    _parse();
}

static BOOL _evtNmeaReceive(PifGps *p_owner, PifGpsNmeaMsgId msg_id)
{
    (void)p_owner;
    s_last_msg_id = msg_id;
    if (msg_id == PIF_GPS_NMEA_MSG_ID_ERR) {
        s_errors++;
        return FALSE;
    }
    s_sentences++;
    return TRUE;
}

static void _evtReceive(PifGps *p_owner)
{
    (void)p_owner;
    s_receives++;
}

static void _evtFrame(char *p_frame)
{
    int length = (int)strlen(p_frame);

    if (s_frame_len + length < (int)sizeof(s_frame)) {
        memcpy(s_frame + s_frame_len, p_frame, length);
        s_frame_len += length;
        s_frame[s_frame_len] = 0;
    }
}

static void _evtText(PifGpsNmeaTxt *p_txt)
{
    s_txt = *p_txt;
    s_texts++;
}

static void _evtTimeout(PifGps *p_owner)
{
    (void)p_owner;
    s_timeouts++;
}

static BOOL _setup(void)
{
    memset(&s_wire, 0, sizeof(s_wire));
    s_receives = s_sentences = s_errors = s_texts = s_timeouts = 0;
    s_last_msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
    s_frame_len = 0;
    s_frame[0] = 0;
    pif_cumulative_timer1ms = 1000;

    pifGpsNmea_Clear(&s_nmea);
    if (!pifGpsNmea_Init(&s_nmea, PIF_ID_AUTO)) return FALSE;
    s_p_gps = &s_nmea._gps;
    s_p_gps->evt_nmea_receive = _evtNmeaReceive;
    s_p_gps->evt_receive = _evtReceive;
    return pifGpsNmea_AttachTransport(&s_nmea, &s_uart.parent);
}


// --- The sentences that carry a fix ---

static void test_gga(void)
{
    CHECK(_setup());
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68");
    CHECK(s_sentences == 1);
    CHECK(s_receives == 1);
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_GGA);
    CHECK(s_p_gps->_utc.hour == 12 && s_p_gps->_utc.minute == 35 && s_p_gps->_utc.second == 19);
    CHECK(s_p_gps->_utc.millisecond == 450);
    // 4807.038' N is 48 + 7.038 / 60 degrees.
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LAT], 48 + 7.038 / 60, 1e-9));
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LON], 11 + 31.0 / 60, 1e-9));
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_num_sat == 8);
    CHECK(s_p_gps->_hdop == 90);
    CHECK(CLOSE(s_p_gps->_altitude, 545.4, 1e-9));
    CHECK(s_p_gps->_connect);
}

static void test_gga_no_fix(void)
{
    CHECK(_setup());
    // The quality is 0 and the receiver sends nothing but the time, which is what it does before a fix.
    _send("$GPGGA,123519.00,,,,,0,00,,,M,,M,,*45");
    CHECK(s_sentences == 1);
    CHECK(!s_p_gps->_fix);
    CHECK(s_p_gps->_num_sat == 0);
    CHECK(s_p_gps->_utc.hour == 12);
}

static void test_southern_western(void)
{
    PifDegMin deg_min;
    PifDegMinSec deg_min_sec;

    CHECK(_setup());
    _send("$GPGGA,000000.00,3350.1234,S,15112.5678,W,1,06,1.2,10.0,M,,M,,*42");
    CHECK(s_p_gps->_coord_deg[PIF_GPS_LAT] < 0);
    CHECK(s_p_gps->_coord_deg[PIF_GPS_LON] < 0);
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LAT], -(33 + 50.1234 / 60), 1e-9));
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LON], -(151 + 12.5678 / 60), 1e-9));
    // The conversions take the magnitude, so a southern latitude is not a degree count that wrapped.
    pifGps_ConvertLatitude2DegMin(s_p_gps, &deg_min);
    CHECK(deg_min.degree == 33);
    CHECK(CLOSE(deg_min.minute, 50.1234, 1e-6));
    pifGps_ConvertLongitude2DegMin(s_p_gps, &deg_min);
    CHECK(deg_min.degree == 151);
    CHECK(CLOSE(deg_min.minute, 12.5678, 1e-6));
    pifGps_ConvertLatitude2DegMinSec(s_p_gps, &deg_min_sec);
    CHECK(deg_min_sec.degree == 33);
    CHECK(deg_min_sec.minute == 50);
    CHECK(CLOSE(deg_min_sec.second, 7.404, 1e-3));
}

static void test_rmc(void)
{
    CHECK(_setup());
    _send("$GPRMC,081836.00,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*4C");
    CHECK(s_sentences == 1);
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_RMC);
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_utc.year == 98 && s_p_gps->_utc.month == 9 && s_p_gps->_utc.day == 13);
    CHECK(CLOSE(s_p_gps->_ground_course, 360.0, 1e-9));
    CHECK(CLOSE(s_p_gps->_ground_speed, 0.0, 1e-9));
}

static void test_rmc_void(void)
{
    CHECK(_setup());
    // Status V, which says the data is not valid: a receiver that sends RMC alone reports its fix here.
    _send("$GPRMC,081836.00,A,3751.65,S,14507.36,E,010.0,360.0,130998,011.3,E*4D");
    CHECK(s_p_gps->_fix);
    // 10 knots is 10 * 1852 / 36 cm/s.
    CHECK(CLOSE(s_p_gps->_ground_speed, 10 * 1852.0 / 36.0, 1e-6));
    _send("$GPRMC,081837.00,V,,,,,,,130998,,*10");
    CHECK(!s_p_gps->_fix);
    // The position of the last fix is still there to read.
    CHECK(s_p_gps->_coord_deg[PIF_GPS_LAT] < 0);
}

static void test_vtg(void)
{
    CHECK(_setup());
    _send("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48");
    CHECK(s_sentences == 1);
    CHECK(CLOSE(s_p_gps->_ground_course, 54.7, 1e-9));
    CHECK(CLOSE(s_p_gps->_ground_speed, 5.5 * 1852.0 / 36.0, 1e-6));
}

static void test_gll_gns_zda(void)
{
    CHECK(_setup());
    _send("$GPGLL,4916.45,N,12311.12,W,225444.00,A,A*72");
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_GLL);
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_utc.hour == 22 && s_p_gps->_utc.minute == 54 && s_p_gps->_utc.second == 44);
    CHECK(CLOSE(s_p_gps->_coord_deg[PIF_GPS_LON], -(123 + 11.12 / 60), 1e-9));

    _send("$GNGNS,112257.00,3023.44,N,09950.30,W,AANN,09,1.1,152.5,-21.3,,*6B");
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_GNS);
    CHECK(s_p_gps->_fix);
    CHECK(s_p_gps->_num_sat == 9);
    CHECK(s_p_gps->_hdop == 110);
    CHECK(CLOSE(s_p_gps->_altitude, 152.5, 1e-9));
    // Every constellation reporting N is no fix at all.
    _send("$GNGNS,112258.00,3023.44,N,09950.30,W,NN,00,,,,,*5D");
    CHECK(!s_p_gps->_fix);

    _send("$GPZDA,201530.00,04,07,2002,00,00*60");
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_ZDA);
    CHECK(s_p_gps->_utc.year == 2 && s_p_gps->_utc.month == 7 && s_p_gps->_utc.day == 4);
    CHECK(s_p_gps->_utc.hour == 20 && s_p_gps->_utc.minute == 15 && s_p_gps->_utc.second == 30);
}


// --- The satellite view ---

static void test_gsv(void)
{
    uint32_t count;

    CHECK(_setup());
    count = s_p_gps->_sv_received_count;
    _send("$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,12,13,06,292,00*77");
    CHECK(s_p_gps->_sv_num_sv == 11);
    CHECK(s_p_gps->_sv_svid[0] == 3 && s_p_gps->_sv_cno[0] == 0);
    CHECK(s_p_gps->_sv_svid[2] == 6 && s_p_gps->_sv_cno[2] == 12);
    CHECK(s_p_gps->_sv_svid[3] == 13);
    CHECK(s_p_gps->_sv_chn[3] == 4);
    CHECK(s_p_gps->_sv_received_count == count + 1);

    // The second sentence of the set carries satellites five to eight.
    _send("$GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00*74");
    CHECK(s_p_gps->_sv_svid[4] == 14);
    CHECK(s_p_gps->_sv_svid[5] == 16 && s_p_gps->_sv_cno[5] == 39);
    CHECK(s_p_gps->_sv_svid[7] == 19);
}

static void test_gsv_signal_id(void)
{
    CHECK(_setup());
    // NMEA 4.10 ends GSV with a signal ID, which falls where a fifth satellite block would start. It is
    // not a satellite, and the two the sentence announced are all that is taken.
    _send("$GAGSV,1,1,02,02,01,085,,30,30,272,,7*78");
    CHECK(s_p_gps->_sv_num_sv == 2);
    CHECK(s_p_gps->_sv_svid[0] == 2);
    CHECK(s_p_gps->_sv_svid[1] == 30);
    CHECK(s_p_gps->_sv_svid[2] == 0);
}


// --- Checksums, framing and the rest of the stream ---

static void test_bad_checksum(void)
{
    CHECK(_setup());
    // The checksum is one off what the sentence works out to.
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*69");
    CHECK(s_errors == 1);
    CHECK(s_sentences == 0);
    CHECK(s_receives == 0);
    CHECK(s_last_msg_id == PIF_GPS_NMEA_MSG_ID_ERR);
    // The parser takes each field in as it arrives, so the members hold what the sentence said. What
    // the checksum decides is whether the fix is handed on, and it was not.
    CHECK(s_receives == 0);
    // The parser is ready for the next sentence.
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68");
    CHECK(s_sentences == 1);
    CHECK(s_receives == 1);
    CHECK(s_p_gps->_num_sat == 8);
}

static void test_no_checksum(void)
{
    CHECK(_setup());
    // A sentence that carries no checksum ends without being reported at all.
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    CHECK(s_sentences == 0);
    CHECK(s_errors == 0);
    CHECK(s_receives == 0);
}

static void test_lower_case_checksum(void)
{
    CHECK(_setup());
    // The standard writes the checksum in upper case, and a receiver that writes it in lower case is
    // taken all the same.
    _send("$GNGNS,112257.00,3023.44,N,09950.30,W,AANN,09,1.1,152.5,-21.3,,*6b");
    CHECK(s_sentences == 1);
}

static void test_cut_sentence(void)
{
    CHECK(_setup());
    // Half a sentence, then a whole one. The "$" starts the sentence over, whatever came before it.
    _send("$GPGGA,123519.45,4807.0");
    CHECK(s_sentences == 0);
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68");
    CHECK(s_sentences == 1);
    CHECK(s_p_gps->_num_sat == 8);
}

static void test_unknown_sentence(void)
{
    CHECK(_setup());
    // A proprietary sentence is skipped whole: none of its fields is taken for an address field.
    _send("$PUBX,00,081350.00,4717.11321,N,00833.91185,E,546.589,G3,2.1,2.0,0.007,0.00,,,,,GGA,0.0*0C");
    CHECK(s_sentences == 0);
    CHECK(s_errors == 0);
    CHECK(s_p_gps->_num_sat == 0);
    CHECK(!s_p_gps->_fix);
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68");
    CHECK(s_sentences == 1);
}

static void test_long_field(void)
{
    char sentence[PIF_GPS_NMEA_VALUE_SIZE * 3];
    uint8_t parity = 0;
    int i;

    CHECK(_setup());
    // A field longer than the buffer is cut, and what is cut still counts towards the checksum, so the
    // sentence checks out and the fields after it are taken.
    strcpy(sentence, "$GPGGA,");
    for (i = 0; i < PIF_GPS_NMEA_VALUE_SIZE + 10; i++) strcat(sentence, "9");
    strcat(sentence, ",4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*");
    for (i = 1; sentence[i] != '*'; i++) parity ^= (uint8_t)sentence[i];
    sprintf(sentence + strlen(sentence), "%02X", parity);
    _send(sentence);
    CHECK(s_sentences == 1);
    CHECK(s_p_gps->_num_sat == 8);
}

static void test_frame_event(void)
{
    CHECK(_setup());
    s_p_gps->evt_frame = _evtFrame;
    _send("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48");
    s_p_gps->evt_frame = NULL;
    // Every piece together is the sentence as it came in, the checksum and the end of line included.
    CHECK(!strcmp(s_frame, "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n"));
}

static void test_text(void)
{
    CHECK(_setup());
    CHECK(pifGps_SetEventNmeaText(s_p_gps, _evtText));
    // Calling it again only changes the callback, and does not allocate a second buffer.
    CHECK(pifGps_SetEventNmeaText(s_p_gps, _evtText));
    _send("$GPTXT,01,01,02,u-blox ag - www.u-blox.com*50");
    CHECK(s_texts == 1);
    CHECK(s_txt.total == 1 && s_txt.num == 1 && s_txt.type == 2);
    CHECK(!strcmp(s_txt.text, "u-blox ag - www.u-blox.com"));
}


// --- The timeout ---

static PifTimerManager s_timer_manager;

// One millisecond of the timer manager. The finish callback is a deferred one, so the task manager runs it.
static void _tick(void)
{
    pif_cumulative_timer1ms++;
    pifTimerManager_sigTick(&s_timer_manager);
    pifTaskManager_Loop();
}

static void test_timeout(void)
{
    int i;

    CHECK(_setup());
    CHECK(pifGps_SetTimeout(s_p_gps, &s_timer_manager, 100, _evtTimeout));
    _send("$GPGGA,123519.45,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*68");
    CHECK(s_p_gps->_connect);
    CHECK(s_p_gps->_fix);

    for (i = 0; i < 99; i++) _tick();
    CHECK(s_timeouts == 0);
    CHECK(s_p_gps->_connect);
    _tick();
    CHECK(s_timeouts == 1);
    CHECK(!s_p_gps->_connect);
    CHECK(!s_p_gps->_fix);
    CHECK(s_p_gps->_num_sat == 0);

    // Clearing gives the timer back, so an instance taken down and set up again does not keep one.
    pifGpsNmea_Clear(&s_nmea);
    _tick();
    CHECK(pifTimerManager_Count(&s_timer_manager) == 0);
    CHECK(_setup());
    CHECK(pifGps_SetTimeout(s_p_gps, &s_timer_manager, 100, _evtTimeout));
    CHECK(pifTimerManager_Count(&s_timer_manager) == 1);
}

static void test_init_errors(void)
{
    CHECK(!pifGpsNmea_Init(NULL, PIF_ID_AUTO));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(_setup());
    CHECK(!pifGpsNmea_AttachTransport(&s_nmea, NULL));
    CHECK(pif_error == E_INVALID_PARAM);
}


static struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { test_gga, "GGA" },
    { test_gga_no_fix, "GGA without a fix" },
    { test_southern_western, "southern and western" },
    { test_rmc, "RMC" },
    { test_rmc_void, "RMC status" },
    { test_vtg, "VTG" },
    { test_gll_gns_zda, "GLL, GNS and ZDA" },
    { test_gsv, "GSV" },
    { test_gsv_signal_id, "GSV with a signal ID" },
    { test_bad_checksum, "bad checksum" },
    { test_no_checksum, "no checksum" },
    { test_lower_case_checksum, "lower case checksum" },
    { test_cut_sentence, "cut sentence" },
    { test_unknown_sentence, "unknown sentence" },
    { test_long_field, "field over the buffer" },
    { test_frame_event, "frame event" },
    { test_text, "TXT" },
    { test_timeout, "timeout" },
    { test_init_errors, "init errors" },
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(4, 2) &&
            pifTimerManager_Init(&s_timer_manager, PIF_ID_AUTO, 1000, 4) &&
            pifUart_Init(&s_uart, PIF_ID_AUTO, 115200) &&
            pifUart_AttachRxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, NULL);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_gps: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_gps\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifGpsNmea_Clear(&s_nmea);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
