// SPDX-License-Identifier: BSD-3-Clause
//
// The NMEA 0183 parser. Every field number below is the position of that field in the sentence, counted
// from the address field as field 0, as the standard lays the sentence out.

#include <ctype.h>
#include <string.h>

#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "gps/pif_gps.h"


// A speed in knots is a speed in nautical miles per hour, and a nautical mile is 1852 m, so one knot is
// 1852 cm/s per 36.
#define KNOT_TO_CM_PER_S		(1852.0 / 36.0)

// Characters of a field that are kept. The two over the terminator are what evt_frame appends: the
// separator that ended the field and a terminator of its own.
#define VALUE_LIMIT				(PIF_GPS_NMEA_VALUE_SIZE - 3)

// The three character sentence formatters the parser knows, in the order of the PIF_GPS_NMEA_MSG_ID_
// values, so the index of a formatter is its ID less one.
static const char kFormatter[][4] = {
		"DTM", "GBS", "GGA", "GLL", "GNS", "GRS", "GSA", "GST", "GSV", "RMC", "THS", "TXT", "VLW", "VTG", "ZDA"
};


/**
 * @brief Whether the first count characters of a string are all digits.
 * @param p_str String to look at.
 * @param count Characters to check.
 * @return TRUE when all of them are digits, which also means the string is at least that long.
 */
static BOOL _areDigits(const char *p_str, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!isdigit((int)p_str[i])) return FALSE;
	}
	return TRUE;
}

/**
 * @brief Converts a decimal ASCII string to an integer. A leading sign is taken, and the first character
 *        that is not a digit ends it.
 * @param p_str Numeric string to convert.
 * @return Converted value.
 */
static int32_t _parseInt(const char *p_str)
{
	BOOL minus = FALSE;
	int32_t value = 0;

	if (*p_str == '-') {
		minus = TRUE;
		p_str++;
	}
	else if (*p_str == '+') p_str++;

	while (isdigit((int)*p_str)) value = value * 10 + (*p_str++ - '0');
	return minus ? -value : value;
}

/**
 * @brief Converts a decimal ASCII string, with or without a fraction, to a double.
 * @param p_str Numeric string to convert.
 * @return Converted value.
 */
static double _parseDouble(const char *p_str)
{
	BOOL minus = FALSE;
	double value = 0.0, scale;

	if (*p_str == '-') {
		minus = TRUE;
		p_str++;
	}
	else if (*p_str == '+') p_str++;

	while (isdigit((int)*p_str)) value = value * 10 + (*p_str++ - '0');
	if (*p_str == '.') {
		p_str++;
		scale = 0.1;
		while (isdigit((int)*p_str)) {
			value += (*p_str++ - '0') * scale;
			scale /= 10;
		}
	}
	return minus ? -value : value;
}

/**
 * @brief Converts a coordinate field to degrees. A latitude is ddmm.mmmm and a longitude dddmm.mmmm: the
 *        last two digits before the point are whole minutes and everything before them is whole degrees,
 *        so the number of degree digits is whatever is left.
 *
 *        Every fractional digit of the minutes the receiver sends is taken, up to seven. Receivers send
 *        four or five, and the fifth is 0.00001 minute, about 2 cm.
 * @param p_str Coordinate field.
 * @return The angle in degrees, always positive: the hemisphere is a field of its own.
 */
static double _parseDegrees(const char *p_str)
{
	const char *p_end;
	uint16_t degree = 0, minute = 0;
	uint32_t fraction = 0, divisor = 1;
	int i;

	for (p_end = p_str; isdigit((int)*p_end); p_end++);

	// Whole degrees are everything but the last two digits.
	while (p_end - p_str > 2) degree = degree * 10 + (*p_str++ - '0');
	while (p_end > p_str) minute = minute * 10 + (*p_str++ - '0');

	if (*p_end == '.') {
		p_end++;
		for (i = 0; i < 7 && isdigit((int)*p_end); i++) {
			fraction = fraction * 10 + (*p_end++ - '0');
			divisor *= 10;
		}
	}
	return degree + (minute + (double)fraction / divisor) / 60;
}

/**
 * @brief Takes the date out of a ddmmyy field. A field that is not six digits is ignored.
 * @param p_str Date field.
 * @param p_date_time Date and time to fill in.
 */
static void _parseDate(const char *p_str, PifDateTime *p_date_time)
{
	if (!_areDigits(p_str, 6)) return;

	p_date_time->day = (p_str[0] - '0') * 10 + (p_str[1] - '0');
	p_date_time->month = (p_str[2] - '0') * 10 + (p_str[3] - '0');
	p_date_time->year = (p_str[4] - '0') * 10 + (p_str[5] - '0');
}

/**
 * @brief Takes the time out of an hhmmss.sss field, where the fraction of the second is optional. A field
 *        whose first six characters are not digits is ignored.
 * @param p_str Time field.
 * @param p_date_time Date and time to fill in.
 */
static void _parseTime(const char *p_str, PifDateTime *p_date_time)
{
	uint16_t scale;
	int i;

	if (!_areDigits(p_str, 6)) return;

	p_date_time->hour = (p_str[0] - '0') * 10 + (p_str[1] - '0');
	p_date_time->minute = (p_str[2] - '0') * 10 + (p_str[3] - '0');
	p_date_time->second = (p_str[4] - '0') * 10 + (p_str[5] - '0');
	p_date_time->millisecond = 0;
	if (p_str[6] == '.') {
		scale = 100;
		for (i = 7; i < 10 && isdigit((int)p_str[i]); i++) {
			p_date_time->millisecond += (p_str[i] - '0') * scale;
			scale /= 10;
		}
	}
}

/**
 * @brief Value of a hex digit.
 * @param c Hex digit, 0 to 9, A to F or a to f.
 * @return Value from 0 to 15, or 0xFF when the character is no hex digit.
 */
static uint8_t _parseHexDigit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0xFF;
}

/**
 * @brief Finds the ID of a sentence from its address field.
 * @param p_address Address field, the two character talker and the three character formatter.
 * @return One of PIF_GPS_NMEA_MSG_ID_, or PIF_GPS_NMEA_MSG_ID_NONE for a sentence the parser skips.
 */
static PifGpsNmeaMsgId _findMsgId(const char *p_address)
{
	uint8_t i;

	for (i = 0; i < sizeof(kFormatter) / sizeof(kFormatter[0]); i++) {
		if (!strncmp(p_address + 2, kFormatter[i], 3)) return i + 1;
	}
	return PIF_GPS_NMEA_MSG_ID_NONE;
}

/**
 * @brief Takes in one satellite field of a GSV sentence. After the three fields that count the sentences
 *        and the satellites, GSV repeats a block of four fields per satellite: ID, elevation, azimuth and
 *        carrier to noise ratio. Only the ID and the ratio are kept.
 * @param p_owner Pointer to the GPS instance.
 */
static void _parseGsvSatellite(PifGps *p_owner)
{
	uint8_t block, index, field;

	block = (p_owner->__param - 4) / 4;				// Satellite of this sentence, from 0
	field = p_owner->__param - 4 - 4 * block + 1;	// Field of that satellite, from 1
	index = block + 4 * (p_owner->__sv_msg_num - 1);	// Satellite of the whole message, from 0

	// NMEA 4.10 ends GSV with a signal ID, which falls where a fifth satellite would start. Both the
	// number of satellites the message announced and the room there is stop it from being taken for one.
	if (index >= p_owner->_sv_num_sv || index >= PIF_GPS_SV_MAXSATS) return;

	switch (field) {
	case 1:
		p_owner->_sv_chn[index] = index + 1;
		p_owner->_sv_svid[index] = _parseInt(p_owner->__string);
		break;

	case 4:
		p_owner->_sv_cno[index] = _parseInt(p_owner->__string);
		p_owner->_sv_quality[index] = 0;	// Only a UBX receiver reports it.
		break;

	default:
		break;
	}
}

/**
 * @brief Takes in the field that has just been terminated, by the ID of the sentence and the position of
 *        the field in it.
 * @param p_owner Pointer to the GPS instance.
 */
static void _parseField(PifGps *p_owner)
{
	PifDeg *p_coord = p_owner->_coord_deg;
	char *p_value = p_owner->__string;

	switch (p_owner->__msg_id) {
	// GGA: time, lat, N/S, lon, E/W, quality, satellites, HDOP, altitude, unit, ...
	case PIF_GPS_NMEA_MSG_ID_GGA:
		switch (p_owner->__param) {
		case 1: _parseTime(p_value, &p_owner->_utc);	break;
		case 2: p_coord[PIF_GPS_LAT] = _parseDegrees(p_value);	break;
		case 3: if (p_value[0] == 'S') p_coord[PIF_GPS_LAT] = -p_coord[PIF_GPS_LAT];	break;
		case 4: p_coord[PIF_GPS_LON] = _parseDegrees(p_value);	break;
		case 5: if (p_value[0] == 'W') p_coord[PIF_GPS_LON] = -p_coord[PIF_GPS_LON];	break;
		// Quality 0 is no fix, and every other value is a fix of some kind.
		case 6: p_owner->_fix = p_value[0] > '0';	break;
		case 7: p_owner->_num_sat = _parseInt(p_value);	break;
		case 8: p_owner->_hdop = _parseDouble(p_value) * 100;	break;
		case 9: p_owner->_altitude = _parseDouble(p_value);	break;
		default: break;
		}
		break;

	// GLL: lat, N/S, lon, E/W, time, status, ...
	case PIF_GPS_NMEA_MSG_ID_GLL:
		switch (p_owner->__param) {
		case 1: p_coord[PIF_GPS_LAT] = _parseDegrees(p_value);	break;
		case 2: if (p_value[0] == 'S') p_coord[PIF_GPS_LAT] = -p_coord[PIF_GPS_LAT];	break;
		case 3: p_coord[PIF_GPS_LON] = _parseDegrees(p_value);	break;
		case 4: if (p_value[0] == 'W') p_coord[PIF_GPS_LON] = -p_coord[PIF_GPS_LON];	break;
		case 5: _parseTime(p_value, &p_owner->_utc);	break;
		case 6: p_owner->_fix = p_value[0] == 'A';	break;
		default: break;
		}
		break;

	// GNS: time, lat, N/S, lon, E/W, mode, satellites, HDOP, altitude, ...
	case PIF_GPS_NMEA_MSG_ID_GNS:
		switch (p_owner->__param) {
		case 1: _parseTime(p_value, &p_owner->_utc);	break;
		case 2: p_coord[PIF_GPS_LAT] = _parseDegrees(p_value);	break;
		case 3: if (p_value[0] == 'S') p_coord[PIF_GPS_LAT] = -p_coord[PIF_GPS_LAT];	break;
		case 4: p_coord[PIF_GPS_LON] = _parseDegrees(p_value);	break;
		case 5: if (p_value[0] == 'W') p_coord[PIF_GPS_LON] = -p_coord[PIF_GPS_LON];	break;
		// The mode has one character per constellation, and N there is no fix from that one. A fix of
		// any constellation is a fix.
		case 6: p_owner->_fix = strspn(p_value, "N") < strlen(p_value);	break;
		case 7: p_owner->_num_sat = _parseInt(p_value);	break;
		case 8: p_owner->_hdop = _parseDouble(p_value) * 100;	break;
		case 9: p_owner->_altitude = _parseDouble(p_value);	break;
		default: break;
		}
		break;

	// GSV: sentences, sentence number, satellites in view, then four fields per satellite.
	case PIF_GPS_NMEA_MSG_ID_GSV:
		switch (p_owner->__param) {
		case 1: break;		// Sentences of this message
		case 2: p_owner->__sv_msg_num = _parseInt(p_value);	break;
		case 3: p_owner->_sv_num_sv = _parseInt(p_value);	break;
		default: _parseGsvSatellite(p_owner);	break;
		}
		break;

	// RMC: time, status, lat, N/S, lon, E/W, speed in knots, course, date, ...
	case PIF_GPS_NMEA_MSG_ID_RMC:
		switch (p_owner->__param) {
		case 1: _parseTime(p_value, &p_owner->_utc);	break;
		case 2: p_owner->_fix = p_value[0] == 'A';	break;
		case 3: p_coord[PIF_GPS_LAT] = _parseDegrees(p_value);	break;
		case 4: if (p_value[0] == 'S') p_coord[PIF_GPS_LAT] = -p_coord[PIF_GPS_LAT];	break;
		case 5: p_coord[PIF_GPS_LON] = _parseDegrees(p_value);	break;
		case 6: if (p_value[0] == 'W') p_coord[PIF_GPS_LON] = -p_coord[PIF_GPS_LON];	break;
		case 7: p_owner->_ground_speed = _parseDouble(p_value) * KNOT_TO_CM_PER_S;	break;
		case 8: p_owner->_ground_course = _parseDouble(p_value);	break;
		case 9: _parseDate(p_value, &p_owner->_utc);	break;
		default: break;
		}
		break;

	// TXT: sentences, sentence number, type, text.
	case PIF_GPS_NMEA_MSG_ID_TXT:
		if (!p_owner->__p_txt) break;
		switch (p_owner->__param) {
		case 1: p_owner->__p_txt->total = _parseInt(p_value);	break;
		case 2: p_owner->__p_txt->num = _parseInt(p_value);	break;
		case 3: p_owner->__p_txt->type = _parseInt(p_value);	break;
		case 4:
			strncpy(p_owner->__p_txt->text, p_value, PIF_GPS_NMEA_TEXT_SIZE - 1);
			p_owner->__p_txt->text[PIF_GPS_NMEA_TEXT_SIZE - 1] = 0;
			break;
		default: break;
		}
		break;

	// VTG: true course, T, magnetic course, M, speed in knots, N, speed in km/h, K, ...
	case PIF_GPS_NMEA_MSG_ID_VTG:
		switch (p_owner->__param) {
		case 1: p_owner->_ground_course = _parseDouble(p_value);	break;
		case 5: p_owner->_ground_speed = _parseDouble(p_value) * KNOT_TO_CM_PER_S;	break;
		default: break;
		}
		break;

	// ZDA: time, day, month, four digit year, ...
	case PIF_GPS_NMEA_MSG_ID_ZDA:
		switch (p_owner->__param) {
		case 1: _parseTime(p_value, &p_owner->_utc);	break;
		case 2: p_owner->_utc.day = _parseInt(p_value);	break;
		case 3: p_owner->_utc.month = _parseInt(p_value);	break;
		case 4: p_owner->_utc.year = _parseInt(p_value) - 2000;	break;
		default: break;
		}
		break;

	// Recognized, so that evt_nmea_receive gets them, but nothing of them is kept.
	default:
		break;
	}
}

/**
 * @brief Hands a piece of the sentence to evt_frame, with the character that ended it appended.
 * @param p_owner Pointer to the GPS instance.
 * @param c Character that ended the piece.
 */
static void _sendFrame(PifGps *p_owner, uint8_t c)
{
	p_owner->__string[p_owner->__offset] = c;
	p_owner->__string[p_owner->__offset + 1] = 0;
	(*p_owner->evt_frame)(p_owner->__string);
}

/**
 * @brief Checks the checksum of the sentence that has just ended and reports it.
 * @param p_owner Pointer to the GPS instance.
 * @return TRUE when the checksum matched.
 */
static BOOL _endSentence(PifGps *p_owner)
{
	uint8_t high, low, checksum;

	high = _parseHexDigit(p_owner->__string[0]);
	low = _parseHexDigit(p_owner->__string[1]);
	if (high == 0xFF || low == 0xFF) checksum = ~p_owner->__parity;	// Anything but a match
	else checksum = (high << 4) + low;

	if (checksum == p_owner->__parity) {
		if (p_owner->evt_nmea_receive) {
			if ((*p_owner->evt_nmea_receive)(p_owner, p_owner->__msg_id)) pifGps_SendEvent(p_owner);
		}
		if (p_owner->__msg_id == PIF_GPS_NMEA_MSG_ID_GSV) p_owner->_sv_received_count++;
		if (p_owner->__msg_id == PIF_GPS_NMEA_MSG_ID_TXT && p_owner->__evt_text) {
			(*p_owner->__evt_text)(p_owner->__p_txt);
		}
		return TRUE;
	}

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "GPS(%u): MsgId=%u CS=%x:%x", p_owner->_id, p_owner->__msg_id, checksum,
			p_owner->__parity);
#endif
	if (p_owner->evt_nmea_receive) (*p_owner->evt_nmea_receive)(p_owner, PIF_GPS_NMEA_MSG_ID_ERR);
	return FALSE;
}

/**
 * @brief Timeout callback that marks GPS link state as disconnected.
 * @param p_issuer Issuer object castable to `PifGps`.
 */
static void _evtTimerFinish(PifIssuerP p_issuer)
{
    PifGps *p_owner = (PifGps *)p_issuer;

	p_owner->_connect = FALSE;
    p_owner->_fix = FALSE;
    p_owner->_num_sat = 0;
	if (p_owner->__evt_timeout) (*p_owner->__evt_timeout)(p_owner);
}

BOOL pifGps_Init(PifGps *p_owner, PifId id)
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

void pifGps_Clear(PifGps *p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
	p_owner->__evt_timeout = NULL;
	if (p_owner->__p_txt) {
		free(p_owner->__p_txt);
		p_owner->__p_txt = NULL;
	}
	p_owner->__evt_text = NULL;
}

BOOL pifGps_SetTimeout(PifGps *p_owner, PifTimerManager *p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout)
{
	if (timeout > 0) {
		if (!p_owner->__p_timer) {
			p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
			if (!p_owner->__p_timer) return FALSE;
			pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner, FALSE);
		}
		p_owner->__evt_timeout = evt_timeout;
	    if (!pifTimer_Start(p_owner->__p_timer, timeout)) return FALSE;
	}
	else {
		if (p_owner->__p_timer) pifTimer_Stop(p_owner->__p_timer);
	}
    return TRUE;
}

void pifGps_SendEvent(PifGps *p_owner)
{
	p_owner->_connect = TRUE;
	if (p_owner->evt_receive) (*p_owner->evt_receive)(p_owner);
	if (p_owner->__p_timer) pifTimer_Reset(p_owner->__p_timer);
}

BOOL pifGps_SetEventNmeaText(PifGps *p_owner, PifEvtGpsNmeaText evt_text)
{
	if (!p_owner->__p_txt) {
		p_owner->__p_txt = (PifGpsNmeaTxt *)calloc(sizeof(PifGpsNmeaTxt), 1);
		if (!p_owner->__p_txt) {
			pif_error = E_OUT_OF_HEAP;
			return FALSE;
		}
	}

	p_owner->__evt_text = evt_text;
	return TRUE;
}

BOOL pifGps_ParsingNmea(PifGps *p_owner, uint8_t c)
{
    BOOL rtn = FALSE;

	if (c == '$') {
		// The start of a sentence, whatever came before it.
		p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
		p_owner->__param = 0;
		p_owner->__offset = 0;
		p_owner->__parity = 0;
		p_owner->__in_checksum = FALSE;
		if (p_owner->evt_frame) _sendFrame(p_owner, c);
	}
	else if (c == ',' || c == '*') {
		p_owner->__string[p_owner->__offset] = 0;
		if (p_owner->__param == 0) {
			// The address field: two characters of talker and three of formatter.
			if (p_owner->__offset == 5) p_owner->__msg_id = _findMsgId(p_owner->__string);
		}
		else if (p_owner->__msg_id && p_owner->__offset) {
			// An empty field says the receiver has nothing to put there, and leaves what it would fill.
			_parseField(p_owner);
		}
		// The count stops short of wrapping, so that a field of a sentence is never taken for the
		// address field of one.
		if (p_owner->__param < 255) p_owner->__param++;
		// The checksum covers everything between the "$" and the "*", the separators included.
		if (c == '*') p_owner->__in_checksum = TRUE;
		else p_owner->__parity ^= c;
		if (p_owner->__msg_id && p_owner->evt_frame) _sendFrame(p_owner, c);
		p_owner->__offset = 0;
	}
	else if (c == '\r' || c == '\n') {
		// The two hex digits of the checksum are in the buffer by now.
		if (p_owner->__msg_id && p_owner->__in_checksum) {
			rtn = _endSentence(p_owner);
			if (!rtn) {
				p_owner->__msg_id = PIF_GPS_NMEA_MSG_ID_NONE;
				p_owner->__param = 0;
			}
		}
		p_owner->__in_checksum = FALSE;
		if (p_owner->evt_frame) _sendFrame(p_owner, c);
		p_owner->__offset = 0;
	}
	else if (c >= 32 && c < 128) {
		// A field longer than the buffer is cut, and what is cut still counts towards the checksum.
		if (p_owner->__offset < VALUE_LIMIT) p_owner->__string[p_owner->__offset++] = c;
		if (!p_owner->__in_checksum) p_owner->__parity ^= c;
	}
	return rtn;
}

/**
 * @brief Splits an angle into whole degrees and the minutes left over.
 * @param degree Angle in degrees, of either sign.
 * @param p_deg_min Degrees and minutes of its magnitude.
 */
static void _toDegMin(PifDeg degree, PifDegMin *p_deg_min)
{
	if (degree < 0) degree = -degree;
	p_deg_min->degree = (uint16_t)degree;
	p_deg_min->minute = (degree - p_deg_min->degree) * 60;
}

/**
 * @brief Splits an angle into whole degrees, whole minutes and the seconds left over.
 * @param degree Angle in degrees, of either sign.
 * @param p_deg_min_sec Degrees, minutes and seconds of its magnitude.
 */
static void _toDegMinSec(PifDeg degree, PifDegMinSec *p_deg_min_sec)
{
	double minute;

	if (degree < 0) degree = -degree;
	p_deg_min_sec->degree = (uint16_t)degree;
	minute = (degree - p_deg_min_sec->degree) * 60;
	p_deg_min_sec->minute = (uint16_t)minute;
	p_deg_min_sec->second = (minute - p_deg_min_sec->minute) * 60;
}

void pifGps_ConvertLatitude2DegMin(PifGps *p_owner, PifDegMin *p_deg_min)
{
	_toDegMin(p_owner->_coord_deg[PIF_GPS_LAT], p_deg_min);
}

void pifGps_ConvertLongitude2DegMin(PifGps *p_owner, PifDegMin *p_deg_min)
{
	_toDegMin(p_owner->_coord_deg[PIF_GPS_LON], p_deg_min);
}

void pifGps_ConvertLatitude2DegMinSec(PifGps *p_owner, PifDegMinSec *p_deg_min_sec)
{
	_toDegMinSec(p_owner->_coord_deg[PIF_GPS_LAT], p_deg_min_sec);
}

void pifGps_ConvertLongitude2DegMinSec(PifGps *p_owner, PifDegMinSec *p_deg_min_sec)
{
	_toDegMinSec(p_owner->_coord_deg[PIF_GPS_LON], p_deg_min_sec);
}
