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

void pifGps_ParsingNmea(PifGps* p_owner, uint8_t c)
{
    uint8_t sv_sat_num, sv_packet_idx, sv_sat_param;

	if (c == '$') {
		p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
		p_owner->__param = 0;
		p_owner->__offset = 0;
		p_owner->__parity = 0;
		if (p_owner->evt_frame) {
			p_owner->__string[0] = c;
			p_owner->__string[1] = 0;
			(p_owner->evt_frame)(p_owner->__string);
		}
	}
	else if (c == ',' || c == '*') {
		p_owner->__string[p_owner->__offset] = 0;
		if (p_owner->__param == 0) {
			if (p_owner->__offset >= 5) {
				if (p_owner->__string[2] == 'D' && p_owner->__string[3] == 'T' && p_owner->__string[4] == 'M') c = PIF_GPS_NMEA_MSG_ID_DTM;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'B' && p_owner->__string[4] == 'S') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GBS;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'G' && p_owner->__string[4] == 'A') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GGA;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'L' && p_owner->__string[4] == 'L') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GLL;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'N' && p_owner->__string[4] == 'S') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GNS;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'R' && p_owner->__string[4] == 'S') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GRS;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'S' && p_owner->__string[4] == 'A') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GSA;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'S' && p_owner->__string[4] == 'T') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GST;
				else if (p_owner->__string[2] == 'G' && p_owner->__string[3] == 'S' && p_owner->__string[4] == 'V') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_GSV;
				else if (p_owner->__string[2] == 'R' && p_owner->__string[3] == 'M' && p_owner->__string[4] == 'C') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_RMC;
				else if (p_owner->__string[2] == 'T' && p_owner->__string[3] == 'H' && p_owner->__string[4] == 'S') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_THS;
				else if (p_owner->__string[2] == 'T' && p_owner->__string[3] == 'X' && p_owner->__string[4] == 'T') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_TXT;
				else if (p_owner->__string[2] == 'V' && p_owner->__string[3] == 'L' && p_owner->__string[4] == 'W') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_VLW;
				else if (p_owner->__string[2] == 'V' && p_owner->__string[3] == 'T' && p_owner->__string[4] == 'G') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_VTG;
				else if (p_owner->__string[2] == 'Z' && p_owner->__string[3] == 'D' && p_owner->__string[4] == 'A') p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_ZDA;
			}
		}
		else if (p_owner->__offset) {
			switch (p_owner->__msg_id) {
			case PIF_GPS_NMEA_MSG_ID_DTM:
				break;

			case PIF_GPS_NMEA_MSG_ID_GBS:
				break;

			case PIF_GPS_NMEA_MSG_ID_GGA:
				switch (p_owner->__param) {
				case 1: 
					_convertString2Time(p_owner->__string, &p_owner->_utc);
					break;
				case 2: 
					p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(p_owner->__string);
					break;
				case 3: 
					if (p_owner->__string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
					break;
				case 4: 
					p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(p_owner->__string);
					break;
				case 5: 
					if (p_owner->__string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
					break;
				case 6: 
					p_owner->_fix = (p_owner->__string[0]  > '0');
					break;
				case 7: 
					p_owner->_num_sat = _convertString2Interger(p_owner->__string);
					break;
				case 9: 
					p_owner->_altitude = _convertString2Float(p_owner->__string);
					break;
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_GLL:
				switch (p_owner->__param) {
				case 1: 
					p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(p_owner->__string);
					break;
				case 2:
					if (p_owner->__string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
					break;
				case 3:
					p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(p_owner->__string);
					break;
				case 4:
					if (p_owner->__string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
					break;
				case 5:
					 _convertString2Time(p_owner->__string, &p_owner->_utc);
					break;
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_GNS:
				switch (p_owner->__param) {
				case 1:
					_convertString2Time(p_owner->__string, &p_owner->_utc);
					break;
				case 2:
					p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(p_owner->__string);
					break;
				case 3:
					if (p_owner->__string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
					break;
				case 4:
					p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(p_owner->__string);
					break;
				case 5:
					if (p_owner->__string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
					break;
				case 7:
					p_owner->_num_sat = _convertString2Interger(p_owner->__string);
					break;
				case 9:
					p_owner->_altitude = _convertString2Float(p_owner->__string);
					break;
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_GRS:
				break;

			case PIF_GPS_NMEA_MSG_ID_GSA:
				break;

			case PIF_GPS_NMEA_MSG_ID_GST:
				break;

			case PIF_GPS_NMEA_MSG_ID_GSV:
				switch (p_owner->__param) {
				case 1:
                    // Total number of messages of this type in this cycle
					break;
				case 2:
                    // Message number
					p_owner->__sv_msg_num = _convertString2Interger(p_owner->__string);
					break;
				case 3:
                    // Total number of SVs visible
					p_owner->_sv_num_sv = _convertString2Interger(p_owner->__string);
					break;
				}
				if (p_owner->__param < 4) break;

				sv_packet_idx = (p_owner->__param - 4) / 4 + 1; // satellite number in packet, 1-4
				sv_sat_num    = sv_packet_idx + (4 * (p_owner->__sv_msg_num - 1)); // global satellite number
				sv_sat_param  = p_owner->__param - 3 - (4 * (sv_packet_idx - 1)); // parameter number for satellite

				if (sv_sat_num > PIF_GPS_SV_MAXSATS) break;

				switch (sv_sat_param) {
				case 1:
					// SV PRN number
					p_owner->_sv_chn[sv_sat_num - 1]  = sv_sat_num;
					p_owner->_sv_svid[sv_sat_num - 1] = _convertString2Interger(p_owner->__string);
					break;
				case 2:
					// Elevation, in degrees, 90 maximum
					break;
				case 3:
					// Azimuth, degrees from True North, 000 through 359
					break;
				case 4:
					// SNR, 00 through 99 dB (null when not tracking)
					p_owner->_sv_cno[sv_sat_num - 1] = _convertString2Interger(p_owner->__string);
					p_owner->_sv_quality[sv_sat_num - 1] = 0; // only used by ublox
					break;
				}

				p_owner->_sv_received_count++;
				break;

			case PIF_GPS_NMEA_MSG_ID_RMC:
				switch (p_owner->__param) {
				case 1:
					_convertString2Time(p_owner->__string, &p_owner->_utc);
					break;
				case 3:
					p_owner->_coord_deg[PIF_GPS_LAT] = _convertString2Degrees(p_owner->__string);
					break;
				case 4:
					if (p_owner->__string[0] == 'S') p_owner->_coord_deg[PIF_GPS_LAT] = -p_owner->_coord_deg[PIF_GPS_LAT];
					break;
				case 5:
					p_owner->_coord_deg[PIF_GPS_LON] = _convertString2Degrees(p_owner->__string);
					break;
				case 6:
					if (p_owner->__string[0] == 'W') p_owner->_coord_deg[PIF_GPS_LON] = -p_owner->_coord_deg[PIF_GPS_LON];
					break;
				case 7:
					p_owner->_ground_speed = _convertString2Float(p_owner->__string) * 51444L;	// knots -> cm/s
					break;
				case 8:
					p_owner->_ground_course = _convertString2Float(p_owner->__string);
					break;
				case 9:
					_convertString2Date(p_owner->__string, &p_owner->_utc);
					break;
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_THS:
				break;

			case PIF_GPS_NMEA_MSG_ID_TXT:
				if (p_owner->__evt_text) {
					switch (p_owner->__param) {
					case 1:
						p_owner->__p_txt->total = _convertString2Interger(p_owner->__string);
						break;
					case 2:
						p_owner->__p_txt->num = _convertString2Interger(p_owner->__string);
						break;
					case 3:
						p_owner->__p_txt->type = _convertString2Interger(p_owner->__string);
						break;
					case 4:
						strncpy(p_owner->__p_txt->text, p_owner->__string, PIF_GPS_NMEA_TEXT_SIZE - 1);
						break;
					}
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_VLW:
				break;

			case PIF_GPS_NMEA_MSG_ID_VTG:
				switch (p_owner->__param) {
				case 1:
					p_owner->_ground_course = _convertString2Float(p_owner->__string);
					break;
				case 5:
					p_owner->_ground_speed = _convertString2Float(p_owner->__string) * 51444L;	// knots -> cm/s
					break;
				}
				break;

			case PIF_GPS_NMEA_MSG_ID_ZDA:
				switch (p_owner->__param) {
				case 1:
					_convertString2Time(p_owner->__string, &p_owner->_utc);
					break;
				case 2:
					p_owner->_utc.day = _convertString2Interger(p_owner->__string);
					break;
				case 3:
					p_owner->_utc.month = _convertString2Interger(p_owner->__string);
					break;
				case 4:
					p_owner->_utc.year = _convertString2Interger(p_owner->__string) - 2000;
					break;
				}
				break;
			}
		}
		if (p_owner->__msg_id) {
			p_owner->__param++;
			if (c == '*') p_owner->__checksum_param = 1;
			else p_owner->__parity ^= c;
			if (p_owner->evt_frame) {
				p_owner->__string[p_owner->__offset++] = c;
				p_owner->__string[p_owner->__offset++] = 0;
				(p_owner->evt_frame)(p_owner->__string);
			}
		}
		p_owner->__offset = 0;
	}
	else if (c == '\r' || c == '\n') {
		if (p_owner->__msg_id && p_owner->__checksum_param) { //parity checksum
			uint8_t checksum = _convertAscii2Hex(p_owner->__string[0]);
			checksum <<= 4;
			checksum += _convertAscii2Hex(p_owner->__string[1]);
			if (checksum == p_owner->__parity) {
				if (p_owner->evt_nmea_receive) {
					if ((*p_owner->evt_nmea_receive)(p_owner, p_owner->__msg_id)) pifGps_SendEvent(p_owner);
				}
				if (p_owner->__msg_id == PIF_GPS_NMEA_MSG_ID_TXT && p_owner->__evt_text) {
					(*p_owner->__evt_text)(p_owner->__p_txt);
				}
			}
			else {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_ERROR, "GPS(%u): MsgId=%u CS=%x:%x", __LINE__, p_owner->__msg_id, checksum, p_owner->__parity);
#endif
				if (p_owner->evt_nmea_receive) {
					(*p_owner->evt_nmea_receive)(p_owner, PIF_GPS_NMEA_MSG_ID_ERR);
				}
				p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
				p_owner->__param = 0;
			}
		}
		p_owner->__checksum_param = 0;
		if (p_owner->evt_frame) {
			p_owner->__string[p_owner->__offset++] = c;
			p_owner->__string[p_owner->__offset++] = 0;
			(p_owner->evt_frame)(p_owner->__string);
		}
		p_owner->__offset = 0;
	}
	else if (c >= 32 && c < 128) {
		if (p_owner->__offset < PIF_GPS_NMEA_VALUE_SIZE) {
			p_owner->__string[p_owner->__offset++] = c;
			if (!p_owner->__checksum_param) p_owner->__parity ^= c;
		}
	}
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
