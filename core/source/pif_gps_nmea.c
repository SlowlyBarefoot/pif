#include <ctype.h>
#include <string.h>

#include "pif_gps_nmea.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif


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
	return deg + min / 60.0 + frac_min / 600000.0;
}

static uint8_t _convertAscii2Hex(char n)    // convert '0'..'9','A'..'F' to 0..15
{
	n -= '0';
	if (n > 9) n -= 7;
	n &= 0x0F;
	return n;
}

static BOOL _makePacket(PifGpsNmea* p_owner, char* p_data)
{
	uint8_t aucHeader[4];
	uint8_t parity = 0;
	int i;

	i = 1;
	while (TRUE) {
		if (p_data[i] == '*') {
			i++;
			break;
		}
		else {
			parity ^= p_data[i];
			i++;
		}
	}
	p_data[i] = kPifHexUpperChar[(parity >> 4) & 0x0F]; i++;
	p_data[i] = kPifHexUpperChar[parity & 0x0F]; i++;
	p_data[i] = '\r'; i++;
	p_data[i] = '\n'; i++;
	p_data[i] = 0;

	pifRingBuffer_BackupHead(&p_owner->__tx.buffer);

	aucHeader[0] = i;
	aucHeader[1] = 0;
	aucHeader[2] = 0;
	aucHeader[3] = 0;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, aucHeader, 4)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t *)p_data, aucHeader[0])) goto fail;
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(&p_owner->__tx.buffer);
	return FALSE;
}

/* This is a light implementation of a GPS frame decoding
   This should work with most of modern GPS devices configured to output NMEA frames.
   It assumes there are some NMEA GGA frames to decode on the serial bus
   Here we use only the following data :
     - latitude
     - longitude
     - GPS fix is/is not ok
     - GPS num sat (4 is enough to be +/- reliable)
     - GPS altitude
     - GPS speed
*/

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
	PifGpsNmea *p_owner = (PifGpsNmea *)p_client;
	PifGps *p_parent = &p_owner->_gps;
	uint8_t c, frame_ok = 0;
	static uint8_t param = 0, offset = 0, parity = 0;
	static char string[PIF_GPS_NMEA_VALUE_SIZE];
	static uint8_t checksum_param = 0;
	static uint16_t message_id = NMEA_MESSAGE_ID_NONE;

	while ((*act_receive_data)(p_owner->__p_comm, &c)) {
		if (c == '$') {
			param = 0;
			offset = 0;
			parity = 0;
		}
		else if (c == ',' || c == '*') {
			string[offset] = 0;
			if (param == 0) { //frame identification
				message_id = NMEA_MESSAGE_ID_NONE;
				if (p_owner->__tx.state != GPTS_IDLE) {
					pifRingBuffer_Remove(&p_owner->__tx.buffer, 4 + p_owner->__tx.ui.st.length);
					p_owner->__tx.state = GPTS_IDLE;
				}
				else {
					if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_DTM)) && string[2] == 'D' && string[3] == 'T' && string[4] == 'M') message_id = NMEA_MESSAGE_ID_DTM;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GBS)) && string[2] == 'G' && string[3] == 'B' && string[4] == 'S') message_id = NMEA_MESSAGE_ID_GBS;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GGA)) && string[2] == 'G' && string[3] == 'G' && string[4] == 'A') message_id = NMEA_MESSAGE_ID_GGA;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GLL)) && string[2] == 'G' && string[3] == 'L' && string[4] == 'L') message_id = NMEA_MESSAGE_ID_GLL;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GNS)) && string[2] == 'G' && string[3] == 'N' && string[4] == 'S') message_id = NMEA_MESSAGE_ID_GNS;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GRS)) && string[2] == 'G' && string[3] == 'R' && string[4] == 'S') message_id = NMEA_MESSAGE_ID_GRS;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GSA)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'A') message_id = NMEA_MESSAGE_ID_GSA;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GST)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'T') message_id = NMEA_MESSAGE_ID_GST;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_GSV)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'V') message_id = NMEA_MESSAGE_ID_GSV;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_RMC)) && string[2] == 'R' && string[3] == 'M' && string[4] == 'C') message_id = NMEA_MESSAGE_ID_RMC;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_THS)) && string[2] == 'T' && string[3] == 'H' && string[4] == 'S') message_id = NMEA_MESSAGE_ID_THS;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_TXT)) && string[2] == 'T' && string[3] == 'X' && string[4] == 'T') message_id = NMEA_MESSAGE_ID_TXT;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_VLW)) && string[2] == 'V' && string[3] == 'L' && string[4] == 'W') message_id = NMEA_MESSAGE_ID_VLW;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_VTG)) && string[2] == 'V' && string[3] == 'T' && string[4] == 'G') message_id = NMEA_MESSAGE_ID_VTG;
					else if ((p_owner->__process_message_id & (1 << NMEA_MESSAGE_ID_ZDA)) && string[2] == 'Z' && string[3] == 'D' && string[4] == 'A') message_id = NMEA_MESSAGE_ID_ZDA;
				}
			}
			else if (offset) {
				switch (message_id) {
				case NMEA_MESSAGE_ID_DTM:
					break;

				case NMEA_MESSAGE_ID_GBS:
					break;

				case NMEA_MESSAGE_ID_GGA:
					if (param == 1) _convertString2Time(string, &p_parent->_date_time);
					else if (param == 2) p_parent->_coord_deg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 3 && string[0] == 'S') p_parent->_coord_deg[GPS_LAT] = -p_parent->_coord_deg[GPS_LAT];
					else if (param == 4) p_parent->_coord_deg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 5 && string[0] == 'W') p_parent->_coord_deg[GPS_LON] = -p_parent->_coord_deg[GPS_LON];
					else if (param == 6) p_parent->_fix = (string[0]  > '0');
					else if (param == 7) p_parent->_num_sat = _convertString2Interger(string);
					else if (param == 9) p_parent->_altitude = _convertString2Float(string);
					break;

				case NMEA_MESSAGE_ID_GLL:
					if (param == 1) p_parent->_coord_deg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 2 && string[0] == 'S') p_parent->_coord_deg[GPS_LAT] = -p_parent->_coord_deg[GPS_LAT];
					else if (param == 3) p_parent->_coord_deg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 4 && string[0] == 'W') p_parent->_coord_deg[GPS_LON] = -p_parent->_coord_deg[GPS_LON];
					else if (param == 5) _convertString2Time(string, &p_parent->_date_time);
					break;

				case NMEA_MESSAGE_ID_GNS:
					if (param == 1) _convertString2Time(string, &p_parent->_date_time);
					else if (param == 2) p_parent->_coord_deg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 3 && string[0] == 'S') p_parent->_coord_deg[GPS_LAT] = -p_parent->_coord_deg[GPS_LAT];
					else if (param == 4) p_parent->_coord_deg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 5 && string[0] == 'W') p_parent->_coord_deg[GPS_LON] = -p_parent->_coord_deg[GPS_LON];
					else if (param == 7) p_parent->_num_sat = _convertString2Interger(string);
					else if (param == 9) p_parent->_altitude = _convertString2Float(string);
					break;

				case NMEA_MESSAGE_ID_GRS:
					break;

				case NMEA_MESSAGE_ID_GSA:
					break;

				case NMEA_MESSAGE_ID_GST:
					break;

				case NMEA_MESSAGE_ID_GSV:
					break;

				case NMEA_MESSAGE_ID_RMC:
					if (param == 1) _convertString2Time(string, &p_parent->_date_time);
					else if (param == 3) p_parent->_coord_deg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 4 && string[0] == 'S') p_parent->_coord_deg[GPS_LAT] = -p_parent->_coord_deg[GPS_LAT];
					else if (param == 5) p_parent->_coord_deg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 6 && string[0] == 'W') p_parent->_coord_deg[GPS_LON] = -p_parent->_coord_deg[GPS_LON];
					else if (param == 7) p_parent->_speed_n = _convertString2Float(string);
					else if (param == 8) p_parent->_ground_course = _convertString2Float(string);
					else if (param == 9) _convertString2Date(string, &p_parent->_date_time);
					break;

				case NMEA_MESSAGE_ID_THS:
					break;

				case NMEA_MESSAGE_ID_TXT:
					if (param == 1) p_owner->__p_txt->total = _convertString2Interger(string);
					else if (param == 2) p_owner->__p_txt->num = _convertString2Interger(string);
					else if (param == 3) p_owner->__p_txt->type = _convertString2Interger(string);
					else if (param == 4) strncpy(p_owner->__p_txt->text, string, PIF_GPS_NMEA_VALUE_SIZE - 1);
					break;

				case NMEA_MESSAGE_ID_VLW:
					break;

				case NMEA_MESSAGE_ID_VTG:
					if (param == 1) p_parent->_ground_course = _convertString2Float(string);
					else if (param == 5) p_parent->_speed_n = _convertString2Float(string);
					else if (param == 7) p_parent->_speed_k = _convertString2Float(string);
					break;

				case NMEA_MESSAGE_ID_ZDA:
					if (param == 1) _convertString2Time(string, &p_parent->_date_time);
					else if (param == 2) p_parent->_date_time.day = _convertString2Interger(string);
					else if (param == 3) p_parent->_date_time.month = _convertString2Interger(string);
					else if (param == 4) p_parent->_date_time.year = _convertString2Interger(string) - 2000;
					break;
				}
			}
			param++;
			offset = 0;
			if (c == '*') checksum_param = 1;
			else parity ^= c;
		}
		else if (c == '\r' || c == '\n') {
			if (message_id && checksum_param) { //parity checksum
				uint8_t checksum = _convertAscii2Hex(string[0]);
				checksum <<= 4;
				checksum += _convertAscii2Hex(string[1]);
				if (checksum == parity) {
					checksum_param = 0;
					if (p_owner->__event_message_id && message_id == p_owner->__event_message_id) frame_ok = 1;
					if (message_id == NMEA_MESSAGE_ID_TXT && p_owner->evt_text) {
						(p_owner->evt_text)(p_owner->__p_txt);
					}
					break;
				}
#ifndef __PIF_NO_LOG__
				else {
					pifLog_Printf(LT_ERROR, "GN(%u): MagId=%u checksum=%x:%x", __LINE__, message_id, checksum, parity);
				}
#endif
			}
			checksum_param = 0;
		}
		else {
			if (offset < PIF_GPS_NMEA_VALUE_SIZE) string[offset++] = c;
			if (!checksum_param) parity ^= c;
		}
	}
	if (frame_ok) {
		pifGps_SendEvent(p_parent);
	}
}

BOOL _evtSending(void* p_client, PifActCommSendData act_send_data)
{
	PifGpsNmea *p_owner = (PifGpsNmea *)p_client;
	uint16_t length;

	switch (p_owner->__tx.state) {
	case GPTS_IDLE:
		if (!pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
			pifRingBuffer_CopyToArray(p_owner->__tx.ui.info, 4, &p_owner->__tx.buffer, 0);
			p_owner->__tx.pos = 4;
			p_owner->__tx.state = GPTS_SENDING;
		}
		break;

	case GPTS_SENDING:
		length = (*act_send_data)(p_owner->__p_comm, pifRingBuffer_GetTailPointer(&p_owner->__tx.buffer, p_owner->__tx.pos),
				pifRingBuffer_GetLinerSize(&p_owner->__tx.buffer, p_owner->__tx.pos));
		p_owner->__tx.pos += length;
		if (p_owner->__tx.pos >= 4 + p_owner->__tx.ui.st.length) {
			p_owner->__tx.state = GPTS_WAIT_SENDED;
		}
		return TRUE;

	case GPTS_WAIT_SENDED:
		if (!p_owner->__tx.ui.st.response) {
			pifRingBuffer_Remove(&p_owner->__tx.buffer, 4 + p_owner->__tx.ui.st.length);
			p_owner->__tx.state = GPTS_IDLE;
		}
		break;

	default:
		break;
	}
	return FALSE;
}

BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGpsNmea));

    if (!pifGps_Init(&p_owner->_gps, id)) goto fail;

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.buffer, PIF_ID_AUTO, PIF_GPS_NMEA_TX_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.buffer, "TxB");
    return TRUE;

fail:
	pifGpsNmea_Clear(p_owner);
    return FALSE;
}

void pifGpsNmea_Clear(PifGpsNmea* p_owner)
{
	pifRingBuffer_Clear(&p_owner->__tx.buffer);
	if (p_owner->__p_txt) {
		free(p_owner->__p_txt);
		p_owner->__p_txt = NULL;
	}
}

void pifGpsNmea_AttachComm(PifGpsNmea* p_owner, PifComm* p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner);
	p_comm->evt_parsing = _evtParsing;
	p_comm->evt_sending = _evtSending;
}

BOOL pifGpsNmea_SetProcessMessageId(PifGpsNmea* p_owner, int count, ...)
{
	va_list ap;
	int i, arg;

	va_start(ap, count);

	p_owner->__process_message_id = 0UL;
	for (i = 0; i < count; i++) {
		arg = va_arg(ap, int);
		p_owner->__process_message_id |= 1UL << arg;

		if (arg == NMEA_MESSAGE_ID_TXT && !p_owner->__p_txt) {
			p_owner->__p_txt = calloc(PIF_GPS_NMEA_VALUE_SIZE, 1);
			if (!p_owner->__p_txt) {
				pif_error = E_OUT_OF_HEAP;
				return FALSE;
			}
		}
	}

	va_end(ap);

	for (i = 1; i <= NMEA_MESSAGE_ID_MAX; i++) {
		if (!(p_owner->__process_message_id & (1UL << i))) {
			switch (i) {
			case NMEA_MESSAGE_ID_TXT:
				if (p_owner->__p_txt) {
					free(p_owner->__p_txt);
					p_owner->__p_txt = NULL;
				}
				break;
			}
		}
	}
	return TRUE;
}

void pifGpsNmea_SetEventMessageId(PifGpsNmea* p_owner, PifGpsNmeaMessageId message_id)
{
	p_owner->__event_message_id = message_id;
}

BOOL pifGpsNmea_PollRequestGBQ(PifGpsNmea* p_owner, const char* p_mag_id)
{
	char data[16] = "$GBGBQ,";
	int i;

	if (!p_owner->__p_comm->act_send_data) {
		pif_error = E_TRANSFER_FAILED;
		return FALSE;
	}

	if (p_owner->__tx.state != GPTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makePacket(p_owner, data);
}

BOOL pifGpsNmea_PollRequestGLQ(PifGpsNmea* p_owner, const char* p_mag_id)
{
	char data[16] = "$GLGLQ,";
	int i;

	if (!p_owner->__p_comm->act_send_data) {
		pif_error = E_TRANSFER_FAILED;
		return FALSE;
	}

	if (p_owner->__tx.state != GPTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makePacket(p_owner, data);
}

BOOL pifGpsNmea_PollRequestGNQ(PifGpsNmea* p_owner, const char* p_mag_id)
{
	char data[16] = "$GNGNQ,";
	int i;

	if (!p_owner->__p_comm->act_send_data) {
		pif_error = E_TRANSFER_FAILED;
		return FALSE;
	}

	if (p_owner->__tx.state != GPTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makePacket(p_owner, data);
}

BOOL pifGpsNmea_PollRequestGPQ(PifGpsNmea* p_owner, const char* p_mag_id)
{
	char data[16] = "$GPGPQ,";
	int i;

	if (!p_owner->__p_comm->act_send_data) {
		pif_error = E_TRANSFER_FAILED;
		return FALSE;
	}

	if (p_owner->__tx.state != GPTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makePacket(p_owner, data);
}
