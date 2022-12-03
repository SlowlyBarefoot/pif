#include <ctype.h>
#include <string.h>

#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "gps/pif_gps.h"


#define DIGIT_TO_VAL(_x)        (_x - '0')


static void _convertString2Date(char* str, PifDateTime* p_date_time)
{
	p_date_time->day = DIGIT_TO_VAL(str[0]) * 10 + DIGIT_TO_VAL(str[1]);
	p_date_time->month = DIGIT_TO_VAL(str[2]) * 10 + DIGIT_TO_VAL(str[3]);
	p_date_time->year = DIGIT_TO_VAL(str[4]) * 10 + DIGIT_TO_VAL(str[5]);
}

static void _convertString2Time(char* str, PifDateTime* p_date_time)
{
	int i, digit;

	p_date_time->hour = DIGIT_TO_VAL(str[0]) * 10 + DIGIT_TO_VAL(str[1]);
	p_date_time->minute = DIGIT_TO_VAL(str[2]) * 10 + DIGIT_TO_VAL(str[3]);
	p_date_time->second = DIGIT_TO_VAL(str[4]) * 10 + DIGIT_TO_VAL(str[5]);
	if (str[6] == '.') {
		p_date_time->millisecond = 0;
		digit = 100;
		for (i = 7; i < 10; i++) {
			if (!isdigit((int)str[i])) break;
			p_date_time->millisecond += DIGIT_TO_VAL(str[i]) * digit;
			digit /= 10;
		}
	}
}

static double _convertString2Float(char* str)
{
	char* p;
	double value = 0.0, unit;

	p = str;
	if (*p == '-') p++;
	while (isdigit((int)*p)) {
		value *= 10;
		value += DIGIT_TO_VAL(*p++);
	}
	if (*str == '-') value = -value;

	if (*p == '.') {
		p++;
		unit = 10;
		while (isdigit((int)*p)) {
			value += DIGIT_TO_VAL(*p++) / unit;
			unit *= 10;
		}
	}
	return value;
}

static int _convertString2Interger(char* str)
{
	char* p;
	int value = 0.0;

	p = str;
	if (*p == '-') p++;
	while (isdigit((int)*p)) {
		value *= 10;
		value += DIGIT_TO_VAL(*p++);
	}
	if (*str == '-') value = -value;
	return value;
}

/*
 * EOS increased the precision here, even if we think that the gps is not precise enough, with 10e5 precision it has 76cm resolution
 * with 10e7 it's around 1 cm now. Increasing it further is irrelevant, since even 1cm resolution is unrealistic, however increased
 * resolution also increased precision of nav calculations
*/

static double _convertString2Degrees(char* s)
{
	char* p;
	char* q;
	uint8_t deg = 0, min = 0;
	unsigned int frac_min = 0;
	uint8_t i;

	// scan for decimal point or end of field
	for (p = s; isdigit((int)*p); p++);
	q = s;

	// convert degrees
	while ((p - q) > 2) {
		if (deg) deg *= 10;
		deg += DIGIT_TO_VAL(*q++);
	}
	// convert minutes
	while (p > q) {
		if (min) min *= 10;
		min += DIGIT_TO_VAL(*q++);
	}
	// convert fractional minutes
	// expect up to four digits, result is in
	// ten-thousandths of a minute
	if (*p == '.') {
		q = p + 1;
		for (i = 0; i < 4; i++) {
			frac_min *= 10;
			if (isdigit((int)*q)) frac_min += *q++ - '0';
		}
	}
	return deg + (min * 10000UL + frac_min) / 600000.0;
}

static uint8_t _convertAscii2Hex(char n)    // convert '0'..'9','A'..'F' to 0..15
{
	n -= '0';
	if (n > 9) n -= 7;
	n &= 0x0F;
	return n;
}

static void _evtTimerFinish(PifIssuerP p_issuer)
{
    PifGps* p_owner = (PifGps*)p_issuer;

	p_owner->_connect = FALSE;
    p_owner->_fix = FALSE;
    p_owner->_num_sat = 0;
	if (p_owner->__evt_timeout) (*p_owner->__evt_timeout)(p_owner);
}

BOOL pifGps_Init(PifGps* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifGps));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	return TRUE;
}

void pifGps_Clear(PifGps* p_owner)
{
	if (p_owner->__p_txt) {
		free(p_owner->__p_txt);
		p_owner->__p_txt = NULL;
	}
	p_owner->__evt_text = NULL;
}

BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout)
{
	if (timeout > 0) {
		if (!p_owner->__p_timer) {
			p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
			if (!p_owner->__p_timer) return FALSE;
			pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner);
		    p_owner->__evt_timeout = evt_timeout;
		}
	    if (!pifTimer_Start(p_owner->__p_timer, timeout)) return FALSE;
	}
	else {
		if (p_owner->__p_timer) pifTimer_Stop(p_owner->__p_timer);
	}
    return TRUE;
}

void pifGps_SendEvent(PifGps* p_owner)
{
	p_owner->_connect = TRUE;
	if (p_owner->evt_receive) (*p_owner->evt_receive)(p_owner);
	if (p_owner->__p_timer) pifTimer_Reset(p_owner->__p_timer);
}

BOOL pifGps_SetEventNmeaText(PifGps* p_owner, PifEvtGpsNmeaText evt_text)
{
	p_owner->__p_txt = (PifGpsNmeaTxt*)calloc(sizeof(PifGpsNmeaTxt), 1);
	if (!p_owner->__p_txt) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_owner->__evt_text = evt_text;
	return TRUE;
}

BOOL pifGps_ParsingNmea(PifGps* p_owner, uint8_t c)
{
	BOOL rtn = FALSE;
	uint8_t frame_ok = 0;
	static uint8_t param = 0, offset = 0, parity = 0;
	static char string[PIF_GPS_NMEA_VALUE_SIZE];
	static uint8_t checksum_param = 0;
	static uint16_t msg_id = PIF_GPS_NMEA_MSG_ID_NONE;

	if (c == '$') {
		param = 0;
		offset = 0;
		parity = 0;
		if (p_owner->evt_frame) {
			string[0] = c;
			string[1] = 0;
			(p_owner->evt_frame)(string);
		}
		return TRUE;
	}
	else if (c == ',' || c == '*') {
		string[offset] = 0;
		if (param == 0) { //frame identification
			msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
			if (string[2] == 'D' && string[3] == 'T' && string[4] == 'M') msg_id = PIF_GPS_NMEA_MSG_ID_DTM;
			else if (string[2] == 'G' && string[3] == 'B' && string[4] == 'S') msg_id = PIF_GPS_NMEA_MSG_ID_GBS;
			else if (string[2] == 'G' && string[3] == 'G' && string[4] == 'A') msg_id = PIF_GPS_NMEA_MSG_ID_GGA;
			else if (string[2] == 'G' && string[3] == 'L' && string[4] == 'L') msg_id = PIF_GPS_NMEA_MSG_ID_GLL;
			else if (string[2] == 'G' && string[3] == 'N' && string[4] == 'S') msg_id = PIF_GPS_NMEA_MSG_ID_GNS;
			else if (string[2] == 'G' && string[3] == 'R' && string[4] == 'S') msg_id = PIF_GPS_NMEA_MSG_ID_GRS;
			else if (string[2] == 'G' && string[3] == 'S' && string[4] == 'A') msg_id = PIF_GPS_NMEA_MSG_ID_GSA;
			else if (string[2] == 'G' && string[3] == 'S' && string[4] == 'T') msg_id = PIF_GPS_NMEA_MSG_ID_GST;
			else if (string[2] == 'G' && string[3] == 'S' && string[4] == 'V') msg_id = PIF_GPS_NMEA_MSG_ID_GSV;
			else if (string[2] == 'R' && string[3] == 'M' && string[4] == 'C') msg_id = PIF_GPS_NMEA_MSG_ID_RMC;
			else if (string[2] == 'T' && string[3] == 'H' && string[4] == 'S') msg_id = PIF_GPS_NMEA_MSG_ID_THS;
			else if (string[2] == 'T' && string[3] == 'X' && string[4] == 'T') msg_id = PIF_GPS_NMEA_MSG_ID_TXT;
			else if (string[2] == 'V' && string[3] == 'L' && string[4] == 'W') msg_id = PIF_GPS_NMEA_MSG_ID_VLW;
			else if (string[2] == 'V' && string[3] == 'T' && string[4] == 'G') msg_id = PIF_GPS_NMEA_MSG_ID_VTG;
			else if (string[2] == 'Z' && string[3] == 'D' && string[4] == 'A') msg_id = PIF_GPS_NMEA_MSG_ID_ZDA;
		}
		else if (offset) {
			switch (msg_id) {
			case PIF_GPS_NMEA_MSG_ID_DTM:
				break;

			case PIF_GPS_NMEA_MSG_ID_GBS:
				break;

			case PIF_GPS_NMEA_MSG_ID_GGA:
				if (param == 1) _convertString2Time(string, &p_owner->_utc);
				else if (param == 2) p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(string);
				else if (param == 3 && string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
				else if (param == 4) p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(string);
				else if (param == 5 && string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
				else if (param == 6) p_owner->_fix = (string[0]  > '0');
				else if (param == 7) p_owner->_num_sat = _convertString2Interger(string);
				else if (param == 9) p_owner->_altitude = _convertString2Float(string);
				break;

			case PIF_GPS_NMEA_MSG_ID_GLL:
				if (param == 1) p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(string);
				else if (param == 2 && string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
				else if (param == 3) p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(string);
				else if (param == 4 && string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
				else if (param == 5) _convertString2Time(string, &p_owner->_utc);
				break;

			case PIF_GPS_NMEA_MSG_ID_GNS:
				if (param == 1) _convertString2Time(string, &p_owner->_utc);
				else if (param == 2) p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(string);
				else if (param == 3 && string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
				else if (param == 4) p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(string);
				else if (param == 5 && string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
				else if (param == 7) p_owner->_num_sat = _convertString2Interger(string);
				else if (param == 9) p_owner->_altitude = _convertString2Float(string);
				break;

			case PIF_GPS_NMEA_MSG_ID_GRS:
				break;

			case PIF_GPS_NMEA_MSG_ID_GSA:
				break;

			case PIF_GPS_NMEA_MSG_ID_GST:
				break;

			case PIF_GPS_NMEA_MSG_ID_GSV:
				break;

			case PIF_GPS_NMEA_MSG_ID_RMC:
				if (param == 1) _convertString2Time(string, &p_owner->_utc);
				else if (param == 3) p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(string);
				else if (param == 4 && string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
				else if (param == 5) p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(string);
				else if (param == 6 && string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
				else if (param == 7) p_owner->_ground_speed = _convertString2Float(string) * 51444L;	// knots -> cm/s
				else if (param == 8) p_owner->_ground_course = _convertString2Float(string);
				else if (param == 9) _convertString2Date(string, &p_owner->_utc);
				break;

			case PIF_GPS_NMEA_MSG_ID_THS:
				break;

			case PIF_GPS_NMEA_MSG_ID_TXT:
				if (p_owner->__evt_text) {
					if (param == 1) p_owner->__p_txt->total = _convertString2Interger(string);
					else if (param == 2) p_owner->__p_txt->num = _convertString2Interger(string);
					else if (param == 3) p_owner->__p_txt->type = _convertString2Interger(string);
					else if (param == 4) strncpy(p_owner->__p_txt->text, string, PIF_GPS_NMEA_TEXT_SIZE - 1);
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_VLW:
				break;

			case PIF_GPS_NMEA_MSG_ID_VTG:
				if (param == 1) p_owner->_ground_course = _convertString2Float(string);
				else if (param == 5) p_owner->_ground_speed = _convertString2Float(string) * 51444L;	// knots -> cm/s
				break;

			case PIF_GPS_NMEA_MSG_ID_ZDA:
				if (param == 1) _convertString2Time(string, &p_owner->_utc);
				else if (param == 2) p_owner->_utc.day = _convertString2Interger(string);
				else if (param == 3) p_owner->_utc.month = _convertString2Interger(string);
				else if (param == 4) p_owner->_utc.year = _convertString2Interger(string) - 2000;
				break;
			}
		}
		param++;
		if (c == '*') checksum_param = 1;
		else parity ^= c;
		if (p_owner->evt_frame) {
			string[offset++] = c;
			string[offset++] = 0;
			(p_owner->evt_frame)(string);
		}
		offset = 0;
	}
	else if (c == '\r' || c == '\n') {
		if (msg_id && checksum_param) { //parity checksum
			uint8_t checksum = _convertAscii2Hex(string[0]);
			checksum <<= 4;
			checksum += _convertAscii2Hex(string[1]);
			if (checksum == parity) {
				if (msg_id == p_owner->evt_nmea_msg_id) frame_ok = 1;
				if (msg_id == PIF_GPS_NMEA_MSG_ID_TXT && p_owner->__evt_text) {
					(p_owner->__evt_text)(p_owner->__p_txt);
				}
			}
#ifndef __PIF_NO_LOG__
			else {
				pifLog_Printf(LT_ERROR, "GN(%u): MsgId=%u CS=%x:%x", __LINE__, msg_id, checksum, parity);
			}
#endif
			rtn = TRUE;
		}
		checksum_param = 0;
		if (p_owner->evt_frame) {
			string[offset++] = c;
			string[offset++] = 0;
			(p_owner->evt_frame)(string);
		}
		offset = 0;
		if (frame_ok) {
			pifGps_SendEvent(p_owner);
		}
		return rtn;
	}
	else {
		if (offset < PIF_GPS_NMEA_VALUE_SIZE) string[offset++] = c;
		if (!checksum_param) parity ^= c;
	}
	return FALSE;
}

void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[PIF_GPS_LAT];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[PIF_GPS_LON];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec)
{
	double degree, minute, second;

	degree = p_owner->_coord_deg[PIF_GPS_LAT];
	p_deg_min_sec->degree = degree;
	minute = (degree - p_deg_min_sec->degree) * 60;
	p_deg_min_sec->minute = minute;
	second = (minute - p_deg_min_sec->minute) * 60;
	p_deg_min_sec->second = second;
}

void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec)
{
	double degree, minute, second;

	degree = p_owner->_coord_deg[PIF_GPS_LON];
	p_deg_min_sec->degree = degree;
	minute = (degree - p_deg_min_sec->degree) * 60;
	p_deg_min_sec->minute = minute;
	second = (minute - p_deg_min_sec->minute) * 60;
	p_deg_min_sec->second = second;
}
