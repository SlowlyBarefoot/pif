#include "pifLog.h"
#include "pifRingBuffer.h"
#include "pifXmodem.h"


// 한 packet을 보내고 응답을 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 500이고 타이머 단위가 1ms이기에 500 * 1ms = 500ms이다.
#define PIF_XMODEM_RESPONSE_TIMEOUT		500

// 한 packet을 전부 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 300이고 타이머 단위가 1ms이기에 300 * 1ms = 300ms이다.
#define PIF_XMODEM_RECEIVE_TIMEOUT		300


typedef enum _PIF_enXmodemRxState
{
	XRS_enIdle			= 0,
	XRS_enC				= 'C',
	XRS_enGetHeader		= 10,
	XRS_enGetData		= 11,
	XRS_enGetCrc		= 12,
	XRS_enSOH			= ASCII_SOH,	// 1
	XRS_enEOT			= ASCII_EOT,	// 4
	XRS_enCAN			= ASCII_CAN		// 24
} PIF_enXmodemRxState;

typedef enum _PIF_enXmodemTxState
{
	XTS_enIdle			= 0,
	XTS_enSendC			= 10,
	XTS_enDelayC		= 11,
	XTS_enSending		= 12,
	XTS_enWaitResponse	= 13,
	XTS_enEOT			= ASCII_EOT,	// 4
	XTS_enACK			= ASCII_ACK,	// 6
	XTS_enNAK			= ASCII_NAK,	// 21
	XTS_enCAN			= ASCII_CAN		// 24
} PIF_enXmodemTxState;


typedef struct _PIF_stXmodemTx
{
	PIF_enXmodemTxState enState;
	uint16_t usDataPos;
	uint16_t usTimeout;
	PIF_stPulseItem *pstTimer;
	PIF_evtXmodemTxReceive evtReceive;
} PIF_stXmodemTx;

typedef struct _PIF_stXmodemRx
{
	PIF_enXmodemRxState enState;
	PIF_stXmodemPacket stPacket;
	uint16_t usCount;
	uint16_t usCrc;
	uint16_t usTimeout;
	PIF_stPulseItem *pstTimer;
	PIF_evtXmodemRxReceive evtReceive;
} PIF_stXmodemRx;

typedef struct _PIF_stXmodemBase
{
	PIF_XmodemType enType;
	uint16_t usPacketSize;
	PIF_stXmodemTx stTx;
	PIF_stXmodemRx stRx;
    uint8_t *pucData;
} PIF_stXmodemBase;


static PIF_stXmodemBase s_stXmodemBase;

static PIF_stPulse *s_pstXmodemTimer;


static void _evtTimerRxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stXmodemBase *pstBase = (PIF_stXmodemBase *)pvIssuer;

	switch (pstBase->stTx.enState) {
	case XTS_enDelayC:
		pstBase->stTx.enState = XTS_enSendC;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enError, "XM ParsingPacket(Timeout) State:%u Cnt:%u",
				pstBase->stRx.enState, pstBase->stRx.usCount);
#endif
		pstBase->stTx.enState = XTS_enNAK;
		pstBase->stRx.enState = XRS_enIdle;
		break;
	}
}

static void _evtTimerTxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stXmodemBase *pstBase = (PIF_stXmodemBase *)pvIssuer;

	switch (pstBase->stTx.enState) {
	case XTS_enWaitResponse:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM TxTimeout State:%u", pstBase->stTx.enState);
#endif
		pstBase->stTx.enState = XTS_enIdle;
		(*pstBase->stTx.evtReceive)(ASCII_NAK, pstBase->pucData[1]);
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM TxTimeout State:%u", pstBase->stTx.enState);
#endif
		break;
	}
}

#ifndef __PIF_NO_LOG__

#define PKT_ERR_INVALID_PACKET_NO	0
#define PKT_ERR_WRONG_CRC    		1

static const char *c_cPktErr[] = {
		"Invalid Packet No",
		"Wrong CRC"
};

#endif

static void _ParsingPacket(PIF_stXmodemBase *pstBase, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodemPacket *pstPacket = &pstBase->stRx.stPacket;
	uint8_t data;
	uint16_t crc;

	while (pifRingBuffer_GetByte(pstBuffer, &data)) {
		switch (pstBase->stRx.enState)	{
		case XRS_enIdle:
			switch (data) {
			case 'C':
				pstBase->stRx.enState = XRS_enC;
				break;

			case ASCII_SOH:
				pstBase->stTx.enState = XTS_enIdle;

				pstBase->stRx.usCount = 1;
				pstBase->stRx.enState = XRS_enGetHeader;
				if (!pifPulse_StartItem(pstBase->stRx.pstTimer, pstBase->stRx.usTimeout)) {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
				}
				break;

			case ASCII_EOT:
				pstBase->stRx.enState = XRS_enEOT;

				pstBase->stTx.enState = XTS_enACK;
				break;

			case ASCII_CAN:
				pstBase->stRx.enState = XRS_enCAN;
				break;
			}
			break;

		case XRS_enGetHeader:
			pstPacket->aucPacketNo[pstBase->stRx.usCount - 1] = data;
			pstBase->stRx.usCount++;
			if (pstBase->stRx.usCount >= 3) {
				if (pstPacket->aucPacketNo[0] + pstPacket->aucPacketNo[1] == 0xFF) {
					pstBase->stRx.enState = XRS_enGetData;
					pstPacket->pucData = pstBase->pucData;
					pstBase->stRx.usCount = 0;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enError, "XM ParsingPacket(%s) %u!=%u", c_cPktErr[PKT_ERR_INVALID_PACKET_NO],
							(unsigned int)pstPacket->aucPacketNo[0], (unsigned int)pstPacket->aucPacketNo[1]);
#endif
					goto fail;
				}
			}
			break;

		case XRS_enGetData:
			pstPacket->pucData[pstBase->stRx.usCount] = data;
			pstBase->stRx.usCount++;
			if (pstBase->stRx.usCount >= 128) {
				pstBase->stRx.enState = XRS_enGetCrc;
				pstBase->stRx.usCount = 0;
			}
			break;

		case XRS_enGetCrc:
			switch (pstBase->enType) {
			case XT_Original:
				pstBase->stRx.usCrc = data;
				crc = pifCheckSum(pstPacket->pucData, 128);
				if (pstBase->stRx.usCrc == crc) {
					pifPulse_StopItem(pstBase->stRx.pstTimer);
					pstBase->stRx.enState = XRS_enSOH;

					pstBase->stTx.enState = XTS_enACK;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enError, "XM ParsingPacket(%s) %u!=%u", c_cPktErr[PKT_ERR_WRONG_CRC],
							(unsigned int)pstBase->stRx.usCrc, (unsigned int)crc);
#endif
					goto fail;
				}
				break;

			case XT_CRC:
				if (!pstBase->stRx.usCount) {
					pstBase->stRx.usCount++;
					pstBase->stRx.usCrc = data << 8;
				}
				else {
					pstBase->stRx.usCrc |= data;
					crc = pifCrc16(pstPacket->pucData, 128);
					if (pstBase->stRx.usCrc == crc) {
			            pifPulse_StopItem(pstBase->stRx.pstTimer);
			            pstBase->stRx.enState = XRS_enSOH;

			            pstBase->stTx.enState = XTS_enACK;
					}
					else {
#ifndef __PIF_NO_LOG__
						pifLog_Printf(LT_enError, "XM ParsingPacket(%s) %u!=%u", c_cPktErr[PKT_ERR_WRONG_CRC],
								(unsigned int)pstBase->stRx.usCrc, (unsigned int)crc);
#endif
						goto fail;
					}
				}
				break;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
	pifPulse_StopItem(pstBase->stRx.pstTimer);
	pstBase->stRx.enState = XRS_enIdle;

	pstBase->stTx.enState = XTS_enNAK;
}

static void _evtParsing(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodemBase *pstBase = (PIF_stXmodemBase *)pvOwner;
	uint8_t data;

	if (pstBase->stTx.enState == XTS_enWaitResponse) {
		if (pifRingBuffer_GetByte(pstBuffer, &data)) {
			switch (data) {
			case ASCII_ACK:
			case ASCII_NAK:
			case ASCII_CAN:
				pifPulse_StopItem(pstBase->stTx.pstTimer);
				pstBase->stTx.enState = XTS_enIdle;
				(*pstBase->stTx.evtReceive)(data, pstBase->pucData[1]);
				break;
			}
		}
	}
	else {
		_ParsingPacket(pstBase, pstBuffer);

		switch (pstBase->stRx.enState) {
		case XRS_enC:
			(*pstBase->stTx.evtReceive)(pstBase->stRx.enState, 0);
			pstBase->stRx.enState = XRS_enIdle;
			pifLog_Printf(LT_enNone, "C");
			break;

		case XRS_enEOT:
		case XRS_enCAN:
			(*pstBase->stRx.evtReceive)(pstBase->stRx.enState, NULL);
			pstBase->stRx.enState = XRS_enIdle;
			break;

		case XRS_enSOH:
			(*pstBase->stRx.evtReceive)(pstBase->stRx.enState, &pstBase->stRx.stPacket);
			pstBase->stRx.enState = XRS_enIdle;
			break;

		default:
			break;
		}
	}
}

static BOOL _evtSending(void *pvClient, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodemBase *pstBase = (PIF_stXmodemBase *)pvClient;
	uint16_t usLength;

	switch (pstBase->stTx.enState) {
	case XTS_enSendC:
		if (pifRingBuffer_GetRemainSize(pstBuffer)) {
			pifRingBuffer_PutByte(pstBuffer, 'C');
   			if (!pifPulse_StartItem(pstBase->stRx.pstTimer, 3000)) {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
			}
			pifLog_Printf(LT_enNone, "C");

			pstBase->stTx.enState = XTS_enDelayC;
		}
		return TRUE;

	case XTS_enSending:
		if (pifRingBuffer_IsEmpty(pstBuffer)) {
			usLength = pifRingBuffer_GetRemainSize(pstBuffer);
			if (pstBase->stTx.usDataPos + usLength > s_stXmodemBase.usPacketSize) usLength = s_stXmodemBase.usPacketSize - pstBase->stTx.usDataPos;
			if (pifRingBuffer_PutData(pstBuffer, pstBase->pucData + pstBase->stTx.usDataPos, usLength)) {
				pstBase->stTx.usDataPos += usLength;
				if (pstBase->stTx.usDataPos >= s_stXmodemBase.usPacketSize) {
					pstBase->stTx.enState = XTS_enWaitResponse;
				}
				return TRUE;
			}
		}
		break;

	case XTS_enEOT:
		pifRingBuffer_PutByte(pstBuffer, pstBase->stTx.enState);
		pstBase->stTx.enState = XTS_enWaitResponse;
		return TRUE;

	case XTS_enCAN:
		pifRingBuffer_PutByte(pstBuffer, pstBase->stTx.enState);
		if (pstBase->stTx.evtReceive) {
			pstBase->stTx.enState = XTS_enWaitResponse;
		}
		else {
			pstBase->stTx.enState = XTS_enIdle;
		}
		return TRUE;

	case XTS_enACK:
	case XTS_enNAK:
		pifRingBuffer_PutByte(pstBuffer, pstBase->stTx.enState);
		pstBase->stTx.enState = XTS_enIdle;
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/**
 * @fn pifXmodem_Init
 * @brief
 * @param pstTimer1ms
 * @return
 */
BOOL pifXmodem_Init(PIF_stPulse *pstTimer1ms, PIF_XmodemType enType)
{
    switch (enType) {
    case XT_Original:
    	s_stXmodemBase.usPacketSize = 3 + 128 + 1;
    	break;

    case XT_CRC:
    	s_stXmodemBase.usPacketSize = 3 + 128 + 2;
    	break;

    default:
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    s_stXmodemBase.pucData = calloc(sizeof(uint8_t), s_stXmodemBase.usPacketSize);
    if (!s_stXmodemBase.pucData) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    s_stXmodemBase.stTx.pstTimer = pifPulse_AddItem(pstTimer1ms, PT_enOnce);
    if (!s_stXmodemBase.stTx.pstTimer) goto fail;

    s_stXmodemBase.stRx.pstTimer = pifPulse_AddItem(pstTimer1ms, PT_enOnce);
    if (!s_stXmodemBase.stRx.pstTimer) goto fail;

    s_stXmodemBase.enType = enType;

    s_stXmodemBase.stTx.enState = XTS_enIdle;
    pifPulse_AttachEvtFinish(s_stXmodemBase.stTx.pstTimer, _evtTimerTxTimeout, &s_stXmodemBase);
    s_stXmodemBase.stTx.usTimeout = PIF_XMODEM_RESPONSE_TIMEOUT;

    s_stXmodemBase.stRx.enState = XRS_enIdle;
    pifPulse_AttachEvtFinish(s_stXmodemBase.stRx.pstTimer, _evtTimerRxTimeout, &s_stXmodemBase);
    s_stXmodemBase.stRx.usTimeout = PIF_XMODEM_RECEIVE_TIMEOUT;

    s_pstXmodemTimer = pstTimer1ms;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "%u Xmodem:Init(T:%u) EC:%d", enType, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifXmodem_Exit
 * @brief
 */
void pifXmodem_Exit()
{
    if (s_stXmodemBase.pucData) {
    	free(s_stXmodemBase.pucData);
    	s_stXmodemBase.pucData = NULL;
    }
}

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief
 * @param usResponseTimeout
 */
void pifXmodem_SetResponseTimeout(uint16_t usResponseTimeout)
{
	s_stXmodemBase.stTx.usTimeout = usResponseTimeout;
}

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief
 * @param usReceiveTimeout
 */
void pifXmodem_SetReceiveTimeout(uint16_t usReceiveTimeout)
{
	s_stXmodemBase.stRx.usTimeout = usReceiveTimeout;
}

/**
 * @fn pifXmodem_AttachComm
 * @brief
 * @param pstComm
 */
void pifXmodem_AttachComm(PIF_stComm *pstComm)
{
	pifComm_AttachClient(pstComm, &s_stXmodemBase);
	pifComm_AttachEvent(pstComm, _evtParsing, _evtSending, NULL);
}

/**
 * @fn pifXmodem_AttachEvent
 * @brief
 * @param evtTxReceive
 * @param evtRxReceive
 */
void pifXmodem_AttachEvent(PIF_evtXmodemTxReceive evtTxReceive, PIF_evtXmodemRxReceive evtRxReceive)
{
	s_stXmodemBase.stTx.evtReceive = evtTxReceive;
	s_stXmodemBase.stRx.evtReceive = evtRxReceive;
}

/**
 * @fn pifXmodem_SendData
 * @brief
 * @param ucPacketNo
 * @param pucData
 * @param usDataSize
 * @return
 */
BOOL pifXmodem_SendData(uint8_t ucPacketNo, uint8_t *pucData, uint16_t usDataSize)
{
	uint16_t i, p, crc;

	if (!s_stXmodemBase.stTx.evtReceive) {
		pif_enError = E_enNotSetEvent;
		goto fail;
	}

	if (!usDataSize || usDataSize > 128) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

	if (s_stXmodemBase.stTx.enState != XTS_enIdle) {
		pif_enError = E_enInvalidState;
		goto fail;
	}

	s_stXmodemBase.pucData[0] = ASCII_SOH;
	s_stXmodemBase.pucData[1] = ucPacketNo;
	s_stXmodemBase.pucData[2] = 0xFF - ucPacketNo;
	p = 3;
	for (i = 0; i < usDataSize; i++) {
		s_stXmodemBase.pucData[p++] = pucData[i];
	}
	if (usDataSize < 128) {
		for (i = 0; i < 128 - usDataSize; i++) {
			s_stXmodemBase.pucData[p++] = ASCII_SUB;
		}
	}
	switch (s_stXmodemBase.enType) {
	case XT_Original:
		crc = pifCheckSum(&s_stXmodemBase.pucData[3], 128);
		s_stXmodemBase.pucData[p++] = crc;
		break;

	case XT_CRC:
		crc = pifCrc16(&s_stXmodemBase.pucData[3], 128);
		s_stXmodemBase.pucData[p++] = crc >> 8;
		s_stXmodemBase.pucData[p++] = crc & 0xFF;
		break;
	}

	s_stXmodemBase.stTx.usDataPos = 0;
	s_stXmodemBase.stTx.enState = XTS_enSending;
	if (!pifPulse_StartItem(s_stXmodemBase.stTx.pstTimer, s_stXmodemBase.stTx.usTimeout)) {
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
	}
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Xmodem:SendData(PN:%u DS:%u S:%u) EC:%d", ucPacketNo, usDataSize, s_stXmodemBase.stTx.enState, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifXmodem_SendEot
 * @brief
 */
void pifXmodem_SendEot()
{
	s_stXmodemBase.stTx.enState = XTS_enEOT;
}

/**
 * @fn pifXmodem_SendCancel
 * @brief
 */
void pifXmodem_SendCancel()
{
	s_stXmodemBase.stTx.enState = XTS_enCAN;
}

/**
 * @fn pifXmodem_ReadyReceive
 * @brief
 * @return
 */
BOOL pifXmodem_ReadyReceive()
{
	if (!s_stXmodemBase.stRx.evtReceive) {
		pif_enError = E_enNotSetEvent;
		return FALSE;
	}

	s_stXmodemBase.stTx.enState = XTS_enSendC;
	return TRUE;
}
