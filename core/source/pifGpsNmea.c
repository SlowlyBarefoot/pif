#include <ctype.h>
#include <string.h>

#include "pifGpsNmea.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define DIGIT_TO_VAL(_x)        (_x - '0')


static void _convertString2Date(char *str, PIF_stDateTime *pstDateTime)
{
	pstDateTime->ucDay = DIGIT_TO_VAL(str[0]) * 10 + DIGIT_TO_VAL(str[1]);
	pstDateTime->ucMonth = DIGIT_TO_VAL(str[2]) * 10 + DIGIT_TO_VAL(str[3]);
	pstDateTime->ucYear = DIGIT_TO_VAL(str[4]) * 10 + DIGIT_TO_VAL(str[5]);
}

static void _convertString2Time(char *str, PIF_stDateTime *pstDateTime)
{
	int i, digit;

	pstDateTime->ucHour = DIGIT_TO_VAL(str[0]) * 10 + DIGIT_TO_VAL(str[1]);
	pstDateTime->ucMinute = DIGIT_TO_VAL(str[2]) * 10 + DIGIT_TO_VAL(str[3]);
	pstDateTime->ucSecond = DIGIT_TO_VAL(str[4]) * 10 + DIGIT_TO_VAL(str[5]);
	if (str[6] == '.') {
		pstDateTime->usMilisecond = 0;
		digit = 100;
		for (i = 7; i < 10; i++) {
			if (!isdigit((int)str[i])) break;
			pstDateTime->usMilisecond += DIGIT_TO_VAL(str[i]) * digit;
			digit /= 10;
		}
	}
}

static double _convertString2Float(char* str)
{
	char *p;
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
	char *p;
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

static double _convertString2Degrees(char *s)
{
	char *p, *q;
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

static BOOL _MakePacket(PIF_stGpsNmea *pstOwner, char *pcData)
{
	uint8_t aucHeader[4];
	uint8_t parity = 0;
	int i;

	i = 1;
	while (TRUE) {
		if (pcData[i] == '*') {
			i++;
			break;
		}
		else {
			parity ^= pcData[i];
			i++;
		}
	}
	pcData[i] = pif_pcHexUpperChar[(parity >> 4) & 0x0F]; i++;
	pcData[i] = pif_pcHexUpperChar[parity & 0x0F]; i++;
	pcData[i] = '\r'; i++;
	pcData[i] = '\n'; i++;
	pcData[i] = 0;
	pifLog_Printf(LT_enNone, pcData);

	pifRingBuffer_BackupHead(pstOwner->__stTx.pstBuffer);

	aucHeader[0] = i;
	aucHeader[1] = 0;
	aucHeader[2] = 0;
	aucHeader[3] = 0;
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstBuffer, aucHeader, 4)) goto fail;
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstBuffer, (uint8_t *)pcData, aucHeader[0])) goto fail;
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(pstOwner->__stTx.pstBuffer);
	if (!pif_enError) pif_enError = E_enOverflowBuffer;
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

static void _evtParsing(void *pvClient, PIF_actCommReceiveData actReceiveData)
{
	PIF_stGpsNmea *pstOwner = (PIF_stGpsNmea *)pvClient;
	PIF_stGps *pstParent = &pstOwner->__stGps;
	uint8_t c, frameOK = 0;
	static uint8_t param = 0, offset = 0, parity = 0;
	static char string[PIF_GPS_NMEA_VALUE_SIZE];
	static uint8_t checksum_param = 0;
	static uint16_t usMessageId = NMEA_MESSAGE_ID_NONE;

	while ((*actReceiveData)(pstOwner->__pstComm, &c)) {
		if (c == '$') {
			param = 0;
			offset = 0;
			parity = 0;
		}
		else if (c == ',' || c == '*') {
			string[offset] = 0;
			if (param == 0) { //frame identification
				usMessageId = NMEA_MESSAGE_ID_NONE;
				if (pstOwner->__stTx.enState != GPTS_enIdle) {
					pifRingBuffer_Remove(pstOwner->__stTx.pstBuffer, 4 + pstOwner->__stTx.ui.stInfo.ucLength);
					pstOwner->__stTx.enState = GPTS_enIdle;
				}
				else {
					if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_DTM)) && string[2] == 'D' && string[3] == 'T' && string[4] == 'M') usMessageId = NMEA_MESSAGE_ID_DTM;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GBS)) && string[2] == 'G' && string[3] == 'B' && string[4] == 'S') usMessageId = NMEA_MESSAGE_ID_GBS;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GGA)) && string[2] == 'G' && string[3] == 'G' && string[4] == 'A') usMessageId = NMEA_MESSAGE_ID_GGA;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GLL)) && string[2] == 'G' && string[3] == 'L' && string[4] == 'L') usMessageId = NMEA_MESSAGE_ID_GLL;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GNS)) && string[2] == 'G' && string[3] == 'N' && string[4] == 'S') usMessageId = NMEA_MESSAGE_ID_GNS;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GRS)) && string[2] == 'G' && string[3] == 'R' && string[4] == 'S') usMessageId = NMEA_MESSAGE_ID_GRS;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GSA)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'A') usMessageId = NMEA_MESSAGE_ID_GSA;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GST)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'T') usMessageId = NMEA_MESSAGE_ID_GST;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_GSV)) && string[2] == 'G' && string[3] == 'S' && string[4] == 'V') usMessageId = NMEA_MESSAGE_ID_GSV;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_RMC)) && string[2] == 'R' && string[3] == 'M' && string[4] == 'C') usMessageId = NMEA_MESSAGE_ID_RMC;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_THS)) && string[2] == 'T' && string[3] == 'H' && string[4] == 'S') usMessageId = NMEA_MESSAGE_ID_THS;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_TXT)) && string[2] == 'T' && string[3] == 'X' && string[4] == 'T') usMessageId = NMEA_MESSAGE_ID_TXT;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_VLW)) && string[2] == 'V' && string[3] == 'L' && string[4] == 'W') usMessageId = NMEA_MESSAGE_ID_VLW;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_VTG)) && string[2] == 'V' && string[3] == 'T' && string[4] == 'G') usMessageId = NMEA_MESSAGE_ID_VTG;
					else if ((pstOwner->__unProcessMessageId & (1 << NMEA_MESSAGE_ID_ZDA)) && string[2] == 'Z' && string[3] == 'D' && string[4] == 'A') usMessageId = NMEA_MESSAGE_ID_ZDA;
				}
			}
			else if (offset) {
				switch (usMessageId) {
				case NMEA_MESSAGE_ID_DTM:
					break;

				case NMEA_MESSAGE_ID_GBS:
					break;

				case NMEA_MESSAGE_ID_GGA:
					if (param == 1) _convertString2Time(string, &pstParent->_stDateTime);
					else if (param == 2) pstParent->_dCoordDeg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 3 && string[0] == 'S') pstParent->_dCoordDeg[GPS_LAT] = -pstParent->_dCoordDeg[GPS_LAT];
					else if (param == 4) pstParent->_dCoordDeg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 5 && string[0] == 'W') pstParent->_dCoordDeg[GPS_LON] = -pstParent->_dCoordDeg[GPS_LON];
					else if (param == 6) pstParent->_ucFix = (string[0]  > '0');
					else if (param == 7) pstParent->_ucNumSat = _convertString2Interger(string);
					else if (param == 9) pstParent->_dAltitude = _convertString2Float(string);
					break;

				case NMEA_MESSAGE_ID_GLL:
					if (param == 1) pstParent->_dCoordDeg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 2 && string[0] == 'S') pstParent->_dCoordDeg[GPS_LAT] = -pstParent->_dCoordDeg[GPS_LAT];
					else if (param == 3) pstParent->_dCoordDeg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 4 && string[0] == 'W') pstParent->_dCoordDeg[GPS_LON] = -pstParent->_dCoordDeg[GPS_LON];
					else if (param == 5) _convertString2Time(string, &pstParent->_stDateTime);
					break;

				case NMEA_MESSAGE_ID_GNS:
					if (param == 1) _convertString2Time(string, &pstParent->_stDateTime);
					else if (param == 2) pstParent->_dCoordDeg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 3 && string[0] == 'S') pstParent->_dCoordDeg[GPS_LAT] = -pstParent->_dCoordDeg[GPS_LAT];
					else if (param == 4) pstParent->_dCoordDeg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 5 && string[0] == 'W') pstParent->_dCoordDeg[GPS_LON] = -pstParent->_dCoordDeg[GPS_LON];
					else if (param == 7) pstParent->_ucNumSat = _convertString2Interger(string);
					else if (param == 9) pstParent->_dAltitude = _convertString2Float(string);
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
					if (param == 1) _convertString2Time(string, &pstParent->_stDateTime);
					else if (param == 3) pstParent->_dCoordDeg[GPS_LAT] = _convertString2Degrees(string);
					else if (param == 4 && string[0] == 'S') pstParent->_dCoordDeg[GPS_LAT] = -pstParent->_dCoordDeg[GPS_LAT];
					else if (param == 5) pstParent->_dCoordDeg[GPS_LON] = _convertString2Degrees(string);
					else if (param == 6 && string[0] == 'W') pstParent->_dCoordDeg[GPS_LON] = -pstParent->_dCoordDeg[GPS_LON];
					else if (param == 7) pstParent->_dSpeedN = _convertString2Float(string);
					else if (param == 8) pstParent->_dGroundCourse = _convertString2Float(string);
					else if (param == 9) _convertString2Date(string, &pstParent->_stDateTime);
					break;

				case NMEA_MESSAGE_ID_THS:
					break;

				case NMEA_MESSAGE_ID_TXT:
					if (param == 1) pstOwner->__pstTxt->ucTotal = _convertString2Interger(string);
					else if (param == 2) pstOwner->__pstTxt->ucNum = _convertString2Interger(string);
					else if (param == 3) pstOwner->__pstTxt->ucType = _convertString2Interger(string);
					else if (param == 4) strncpy(pstOwner->__pstTxt->acText, string, PIF_GPS_NMEA_VALUE_SIZE - 1);
					break;

				case NMEA_MESSAGE_ID_VLW:
					break;

				case NMEA_MESSAGE_ID_VTG:
					if (param == 1) pstParent->_dGroundCourse = _convertString2Float(string);
					else if (param == 5) pstParent->_dSpeedN = _convertString2Float(string);
					else if (param == 7) pstParent->_dSpeedK = _convertString2Float(string);
					break;

				case NMEA_MESSAGE_ID_ZDA:
					if (param == 1) _convertString2Time(string, &pstParent->_stDateTime);
					else if (param == 2) pstParent->_stDateTime.ucDay = _convertString2Interger(string);
					else if (param == 3) pstParent->_stDateTime.ucMonth = _convertString2Interger(string);
					else if (param == 4) pstParent->_stDateTime.ucYear = _convertString2Interger(string) - 2000;
					break;
				}
			}
			param++;
			offset = 0;
			if (c == '*') checksum_param = 1;
			else parity ^= c;
		}
		else if (c == '\r' || c == '\n') {
			if (usMessageId && checksum_param) { //parity checksum
				uint8_t checksum = _convertAscii2Hex(string[0]);
				checksum <<= 4;
				checksum += _convertAscii2Hex(string[1]);
				if (checksum == parity) {
					checksum_param = 0;
					if (pstOwner->__ucEventMessageId && usMessageId == pstOwner->__ucEventMessageId) frameOK = 1;
					if (usMessageId == NMEA_MESSAGE_ID_TXT && pstOwner->__evtText) {
						(pstOwner->__evtText)(pstOwner->__pstTxt);
					}
					break;
				}
#ifndef __PIF_NO_LOG__
				else {
					pifLog_Printf(LT_enError, "GN(%u): MagId=%u checksum=%x:%x", __LINE__, usMessageId, checksum, parity);
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
	if (frameOK) {
		if (pstParent->__evtReceive) (*pstParent->__evtReceive)(pstParent);
	}
}

BOOL _evtSending(void *pvClient, PIF_actCommSendData actSendData)
{
	PIF_stGpsNmea *pstOwner = (PIF_stGpsNmea *)pvClient;
	uint16_t usLength;

	switch (pstOwner->__stTx.enState) {
	case GPTS_enIdle:
		if (!pifRingBuffer_IsEmpty(pstOwner->__stTx.pstBuffer)) {
			pifRingBuffer_CopyToArray(pstOwner->__stTx.ui.ucInfo, 4, pstOwner->__stTx.pstBuffer, 0);
			pstOwner->__stTx.ucPos = 4;
			pstOwner->__stTx.enState = GPTS_enSending;
		}
		break;

	case GPTS_enSending:
		usLength = (*actSendData)(pstOwner->__pstComm, pifRingBuffer_GetTailPointer(pstOwner->__stTx.pstBuffer, pstOwner->__stTx.ucPos),
				pifRingBuffer_GetLinerSize(pstOwner->__stTx.pstBuffer, pstOwner->__stTx.ucPos));
		pstOwner->__stTx.ucPos += usLength;
		if (pstOwner->__stTx.ucPos >= 4 + pstOwner->__stTx.ui.stInfo.ucLength) {
			pstOwner->__stTx.enState = GPTS_enWaitSended;
		}
		return TRUE;

	case GPTS_enWaitSended:
		if (!pstOwner->__stTx.ui.stInfo.ucResponse) {
			pifRingBuffer_Remove(pstOwner->__stTx.pstBuffer, 4 + pstOwner->__stTx.ui.stInfo.ucLength);
			pstOwner->__stTx.enState = GPTS_enIdle;
		}
		break;

	default:
		break;
	}
	return FALSE;
}

/**
 * @fn pifGpsNmea_Create
 * @brief
 * @param usPifId
 * @return
 */
PIF_stGpsNmea *pifGpsNmea_Create(PIF_usId usPifId)
{
	PIF_stGpsNmea *pstOwner = calloc(sizeof(PIF_stGpsNmea), 1);
    if (!pstOwner) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pifGps_Init(&pstOwner->__stGps, usPifId);

    pstOwner->__stTx.pstBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_GPS_NMEA_TX_SIZE);
    if (!pstOwner->__stTx.pstBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->__stTx.pstBuffer, "TxB");
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GN:%u(%u) EC:%d", __LINE__, usPifId, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifGpsNmea_Destroy
 * @brief
 * @param ppstOwner
 */
void pifGpsNmea_Destroy(PIF_stGpsNmea **ppstOwner)
{
	if (*ppstOwner) {
		if ((*ppstOwner)->__stTx.pstBuffer) {
			pifRingBuffer_Exit((*ppstOwner)->__stTx.pstBuffer);
			(*ppstOwner)->__stTx.pstBuffer = NULL;
		}
		if ((*ppstOwner)->__pstTxt) {
			free((*ppstOwner)->__pstTxt);
			(*ppstOwner)->__pstTxt = NULL;
		}
		free(*ppstOwner);
		*ppstOwner = NULL;
	}
}

/**
 * @fn pifGpsNmea_AttachComm
 * @brief
 * @param pstOwner
 * @param pstComm
 */
void pifGpsNmea_AttachComm(PIF_stGpsNmea *pstOwner, PIF_stComm *pstComm)
{
	pstOwner->__pstComm = pstComm;
	pifComm_AttachClient(pstComm, pstOwner);
	pstComm->evtParsing = _evtParsing;
	pstComm->evtSending = _evtSending;
}

/**
 * @fn pifGpsNmea_AttachEvtText
 * @brief
 * @param pstOwner
 * @param evtText
 * @param ucMaxSize
 */
void pifGpsNmea_AttachEvtText(PIF_stGpsNmea *pstOwner, PIF_evtGpsNmeaText evtText)
{
	pstOwner->__evtText = evtText;
}

/**
 * @fn pifGpsNmea_SetProcessMessageId
 * @brief
 * @param pstOwner
 * @param nCount
 * @return
 */
BOOL pifGpsNmea_SetProcessMessageId(PIF_stGpsNmea *pstOwner, int nCount, ...)
{
	va_list ap;
	int i, arg;

	va_start(ap, nCount);

	pstOwner->__unProcessMessageId = 0UL;
	for (i = 0; i < nCount; i++) {
		arg = va_arg(ap, int);
		pstOwner->__unProcessMessageId |= 1UL << arg;

		if (arg == NMEA_MESSAGE_ID_TXT && !pstOwner->__pstTxt) {
			pstOwner->__pstTxt = calloc(PIF_GPS_NMEA_VALUE_SIZE, 1);
			if (!pstOwner->__pstTxt) {
				pif_enError = E_enOutOfHeap;
				return FALSE;
			}
		}
	}

	va_end(ap);

	for (i = 1; i <= NMEA_MESSAGE_ID_MAX; i++) {
		if (!(pstOwner->__unProcessMessageId & (1UL << i))) {
			switch (i) {
			case NMEA_MESSAGE_ID_TXT:
				if (pstOwner->__pstTxt) {
					free(pstOwner->__pstTxt);
					pstOwner->__pstTxt = NULL;
				}
				break;
			}
		}
	}
	return TRUE;
}

/**
 * @fn pifGpsNmea_SetEventMessageId
 * @brief
 * @param pstOwner
 * @param ucMessageId
 */
void pifGpsNmea_SetEventMessageId(PIF_stGpsNmea *pstOwner, PIF_ucGpsNmeaMessageId ucMessageId)
{
	pstOwner->__ucEventMessageId = ucMessageId;
}

/**
 * @fn pifGpsNmea_PollRequestGBQ
 * @brief
 * @param pstOwner
 * @param pcMagId
 * @return
 */
BOOL pifGpsNmea_PollRequestGBQ(PIF_stGpsNmea *pstOwner, const char *pcMagId)
{
	char data[16] = "$GBGBQ,";
	int i;

	if (!pstOwner->__pstComm->__actSendData) {
		pif_enError = E_enTransferFailed;
		return FALSE;
	}

	if (pstOwner->__stTx.enState != GPTS_enIdle) {
		pif_enError = E_enInvalidState;
		return FALSE;
	}

	i = 0;
	while (pcMagId[i]) {
		data[7 + i] = pcMagId[i];
		i++;
	}
	data[7 + i] = '*';

	return _MakePacket(pstOwner, data);
}

/**
 * @fn pifGpsNmea_PollRequestGLQ
 * @brief
 * @param pstOwner
 * @param pcMagId
 * @return
 */
BOOL pifGpsNmea_PollRequestGLQ(PIF_stGpsNmea *pstOwner, const char *pcMagId)
{
	char data[16] = "$GLGLQ,";
	int i;

	if (!pstOwner->__pstComm->__actSendData) {
		pif_enError = E_enTransferFailed;
		return FALSE;
	}

	if (pstOwner->__stTx.enState != GPTS_enIdle) {
		pif_enError = E_enInvalidState;
		return FALSE;
	}

	i = 0;
	while (pcMagId[i]) {
		data[7 + i] = pcMagId[i];
		i++;
	}
	data[7 + i] = '*';

	return _MakePacket(pstOwner, data);
}

/**
 * @fn pifGpsNmea_PollRequestGNQ
 * @brief
 * @param pstOwner
 * @param pcMagId
 * @return
 */
BOOL pifGpsNmea_PollRequestGNQ(PIF_stGpsNmea *pstOwner, const char *pcMagId)
{
	char data[16] = "$GNGNQ,";
	int i;

	if (!pstOwner->__pstComm->__actSendData) {
		pif_enError = E_enTransferFailed;
		return FALSE;
	}

	if (pstOwner->__stTx.enState != GPTS_enIdle) {
		pif_enError = E_enInvalidState;
		return FALSE;
	}

	i = 0;
	while (pcMagId[i]) {
		data[7 + i] = pcMagId[i];
		i++;
	}
	data[7 + i] = '*';

	return _MakePacket(pstOwner, data);
}

/**
 * @fn pifGpsNmea_PollRequestGPQ
 * @brief
 * @param pstOwner
 * @param pcMagId
 * @return
 */
BOOL pifGpsNmea_PollRequestGPQ(PIF_stGpsNmea *pstOwner, const char *pcMagId)
{
	char data[16] = "$GPGPQ,";
	int i;

	if (!pstOwner->__pstComm->__actSendData) {
		pif_enError = E_enTransferFailed;
		return FALSE;
	}

	if (pstOwner->__stTx.enState != GPTS_enIdle) {
		pif_enError = E_enInvalidState;
		return FALSE;
	}

	i = 0;
	while (pcMagId[i]) {
		data[7 + i] = pcMagId[i];
		i++;
	}
	data[7 + i] = '*';

	return _MakePacket(pstOwner, data);
}
