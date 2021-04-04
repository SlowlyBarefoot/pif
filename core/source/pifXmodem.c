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


static PIF_stXmodem *s_pstXmodem;
static uint8_t s_ucXmodemSize;
static uint8_t s_ucXmodemPos;

static PIF_stPulse *s_pstXmodemTimer;


static void _evtTimerRxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvIssuer;

	switch (pstOwner->__stTx.enState) {
	case XTS_enDelayC:
		pstOwner->__stTx.enState = XTS_enSendC;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enError, "XM ParsingPacket(Timeout) State:%u Cnt:%u",
				pstOwner->__stRx.enState, pstOwner->__stRx.usCount);
#endif
		pstOwner->__stTx.enState = XTS_enNAK;
		pstOwner->__stRx.enState = XRS_enIdle;
		break;
	}
}

static void _evtTimerTxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvIssuer;

	switch (pstOwner->__stTx.enState) {
	case XTS_enWaitResponse:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM TxTimeout State:%u", pstOwner->__stTx.enState);
#endif
		pstOwner->__stTx.enState = XTS_enIdle;
		(*pstOwner->__stTx.evtReceive)(ASCII_NAK, pstOwner->__pucData[1]);
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM TxTimeout State:%u", pstOwner->__stTx.enState);
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

static void _ParsingPacket(PIF_stXmodem *pstOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodemPacket *pstPacket = &pstOwner->__stRx.stPacket;
	uint8_t data;
	uint16_t crc;

	while (pifRingBuffer_GetByte(pstBuffer, &data)) {
		switch (pstOwner->__stRx.enState)	{
		case XRS_enIdle:
			switch (data) {
			case 'C':
				pstOwner->__stRx.enState = XRS_enC;
				break;

			case ASCII_SOH:
				pstOwner->__stTx.enState = XTS_enIdle;

				pstOwner->__stRx.usCount = 1;
				pstOwner->__stRx.enState = XRS_enGetHeader;
				if (!pifPulse_StartItem(pstOwner->__stRx.pstTimer, pstOwner->__stRx.usTimeout * 1000L / s_pstXmodemTimer->_unPeriodUs)) {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
				}
				break;

			case ASCII_EOT:
				pstOwner->__stRx.enState = XRS_enEOT;

				pstOwner->__stTx.enState = XTS_enACK;
				break;

			case ASCII_CAN:
				pstOwner->__stRx.enState = XRS_enCAN;
				break;
			}
			break;

		case XRS_enGetHeader:
			pstPacket->aucPacketNo[pstOwner->__stRx.usCount - 1] = data;
			pstOwner->__stRx.usCount++;
			if (pstOwner->__stRx.usCount >= 3) {
				if (pstPacket->aucPacketNo[0] + pstPacket->aucPacketNo[1] == 0xFF) {
					pstOwner->__stRx.enState = XRS_enGetData;
					pstPacket->pucData = pstOwner->__pucData;
					pstOwner->__stRx.usCount = 0;
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
			pstPacket->pucData[pstOwner->__stRx.usCount] = data;
			pstOwner->__stRx.usCount++;
			if (pstOwner->__stRx.usCount >= 128) {
				pstOwner->__stRx.enState = XRS_enGetCrc;
				pstOwner->__stRx.usCount = 0;
			}
			break;

		case XRS_enGetCrc:
			switch (pstOwner->__enType) {
			case XT_enOriginal:
				pstOwner->__stRx.usCrc = data;
				crc = pifCheckSum(pstPacket->pucData, 128);
				if (pstOwner->__stRx.usCrc == crc) {
					pifPulse_StopItem(pstOwner->__stRx.pstTimer);
					pstOwner->__stRx.enState = XRS_enSOH;

					pstOwner->__stTx.enState = XTS_enACK;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enError, "XM ParsingPacket(%s) %u!=%u", c_cPktErr[PKT_ERR_WRONG_CRC],
							(unsigned int)pstOwner->__stRx.usCrc, (unsigned int)crc);
#endif
					goto fail;
				}
				break;

			case XT_enCRC:
				if (!pstOwner->__stRx.usCount) {
					pstOwner->__stRx.usCount++;
					pstOwner->__stRx.usCrc = data << 8;
				}
				else {
					pstOwner->__stRx.usCrc |= data;
					crc = pifCrc16(pstPacket->pucData, 128);
					if (pstOwner->__stRx.usCrc == crc) {
			            pifPulse_StopItem(pstOwner->__stRx.pstTimer);
			            pstOwner->__stRx.enState = XRS_enSOH;

			            pstOwner->__stTx.enState = XTS_enACK;
					}
					else {
#ifndef __PIF_NO_LOG__
						pifLog_Printf(LT_enError, "XM ParsingPacket(%s) %u!=%u", c_cPktErr[PKT_ERR_WRONG_CRC],
								(unsigned int)pstOwner->__stRx.usCrc, (unsigned int)crc);
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
	pifPulse_StopItem(pstOwner->__stRx.pstTimer);
	pstOwner->__stRx.enState = XRS_enIdle;

	pstOwner->__stTx.enState = XTS_enNAK;
}

static void _evtParsing(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvOwner;
	uint8_t data;

	if (pstOwner->__stTx.enState == XTS_enWaitResponse) {
		if (pifRingBuffer_GetByte(pstBuffer, &data)) {
			switch (data) {
			case ASCII_ACK:
			case ASCII_NAK:
			case ASCII_CAN:
				pifPulse_StopItem(pstOwner->__stTx.pstTimer);
				pstOwner->__stTx.enState = XTS_enIdle;
				(*pstOwner->__stTx.evtReceive)(data, pstOwner->__pucData[1]);
				break;
			}
		}
	}
	else {
		_ParsingPacket(pstOwner, pstBuffer);

		switch (pstOwner->__stRx.enState) {
		case XRS_enC:
			(*pstOwner->__stTx.evtReceive)(pstOwner->__stRx.enState, 0);
			pstOwner->__stRx.enState = XRS_enIdle;
			pifLog_Printf(LT_enNone, "C");
			break;

		case XRS_enEOT:
		case XRS_enCAN:
			(*pstOwner->__stRx.evtReceive)(pstOwner->__stRx.enState, NULL);
			pstOwner->__stRx.enState = XRS_enIdle;
			break;

		case XRS_enSOH:
			(*pstOwner->__stRx.evtReceive)(pstOwner->__stRx.enState, &pstOwner->__stRx.stPacket);
			pstOwner->__stRx.enState = XRS_enIdle;
			break;

		default:
			break;
		}
	}
}

static BOOL _evtSending(void *pvClient, PIF_stRingBuffer *pstBuffer)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvClient;
	uint16_t usLength;

	switch (pstOwner->__stTx.enState) {
	case XTS_enSendC:
		if (pifRingBuffer_GetRemainSize(pstBuffer)) {
			pifRingBuffer_PutByte(pstBuffer, 'C');
   			if (!pifPulse_StartItem(pstOwner->__stRx.pstTimer, 3000 * 1000L / s_pstXmodemTimer->_unPeriodUs)) {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
			}
			pifLog_Printf(LT_enNone, "C");

			pstOwner->__stTx.enState = XTS_enDelayC;
		}
		return TRUE;

	case XTS_enSending:
		if (pifRingBuffer_IsEmpty(pstBuffer)) {
			usLength = pifRingBuffer_GetRemainSize(pstBuffer);
			if (pstOwner->__stTx.usDataPos + usLength > pstOwner->__usPacketSize) usLength = pstOwner->__usPacketSize - pstOwner->__stTx.usDataPos;
			if (pifRingBuffer_PutData(pstBuffer, pstOwner->__pucData + pstOwner->__stTx.usDataPos, usLength)) {
				pstOwner->__stTx.usDataPos += usLength;
				if (pstOwner->__stTx.usDataPos >= pstOwner->__usPacketSize) {
					pstOwner->__stTx.enState = XTS_enWaitResponse;
				}
				return TRUE;
			}
		}
		break;

	case XTS_enEOT:
		pifRingBuffer_PutByte(pstBuffer, pstOwner->__stTx.enState);
		pstOwner->__stTx.enState = XTS_enWaitResponse;
		return TRUE;

	case XTS_enCAN:
		pifRingBuffer_PutByte(pstBuffer, pstOwner->__stTx.enState);
		if (pstOwner->__stTx.evtReceive) {
			pstOwner->__stTx.enState = XTS_enWaitResponse;
		}
		else {
			pstOwner->__stTx.enState = XTS_enIdle;
		}
		return TRUE;

	case XTS_enACK:
	case XTS_enNAK:
		pifRingBuffer_PutByte(pstBuffer, pstOwner->__stTx.enState);
		pstOwner->__stTx.enState = XTS_enIdle;
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/**
 * @fn pifXmodem_Init
 * @brief
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifXmodem_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstXmodem = calloc(sizeof(PIF_stXmodem), ucSize);
    if (!s_pstXmodem) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucXmodemSize = ucSize;
    s_ucXmodemPos = 0;

    s_pstXmodemTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Xmodem:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifXmodem_Exit
 * @brief
 */
void pifXmodem_Exit()
{
    if (s_pstXmodem) {
        for (int i = 0; i < s_ucXmodemPos; i++) {
        	PIF_stXmodem *pstOwner = &s_pstXmodem[i];
			if (pstOwner->__pucData) {
				free(pstOwner->__pucData);
				pstOwner->__pucData = NULL;
			}
			if (pstOwner->__stRx.pstTimer) {
				pifPulse_RemoveItem(s_pstXmodemTimer, pstOwner->__stRx.pstTimer);
			}
			if (pstOwner->__stTx.pstTimer) {
				pifPulse_RemoveItem(s_pstXmodemTimer, pstOwner->__stTx.pstTimer);
			}
        }
    	free(s_pstXmodem);
        s_pstXmodem = NULL;
    }
}

/**
 * @fn pifXmodem_Add
 * @brief
 * @param usPifId
 * @param enType
 * @return
 */
PIF_stXmodem *pifXmodem_Add(PIF_usId usPifId, PIF_enXmodemType enType)
{
    if (s_ucXmodemPos >= s_ucXmodemSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stXmodem *pstOwner = &s_pstXmodem[s_ucXmodemPos];

    switch (enType) {
    case XT_enOriginal:
    	pstOwner->__usPacketSize = 3 + 128 + 1;
    	break;

    case XT_enCRC:
    	pstOwner->__usPacketSize = 3 + 128 + 2;
    	break;

    default:
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    pstOwner->__pucData = calloc(sizeof(uint8_t), pstOwner->__usPacketSize);
    if (!pstOwner->__pucData) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->__stTx.pstTimer = pifPulse_AddItem(s_pstXmodemTimer, PT_enOnce);
    if (!pstOwner->__stTx.pstTimer) goto fail;

    pstOwner->__stRx.pstTimer = pifPulse_AddItem(s_pstXmodemTimer, PT_enOnce);
    if (!pstOwner->__stRx.pstTimer) goto fail;

    pstOwner->__enType = enType;

    pstOwner->__stTx.enState = XTS_enIdle;
    pifPulse_AttachEvtFinish(pstOwner->__stTx.pstTimer, _evtTimerTxTimeout, pstOwner);
    pstOwner->__stTx.usTimeout = PIF_XMODEM_RESPONSE_TIMEOUT;

    pstOwner->__stRx.enState = XRS_enIdle;
    pifPulse_AttachEvtFinish(pstOwner->__stRx.pstTimer, _evtTimerRxTimeout, pstOwner);
    pstOwner->__stRx.usTimeout = PIF_XMODEM_RECEIVE_TIMEOUT;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;

    s_ucXmodemPos = s_ucXmodemPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "%u Xmodem:Init(T:%u) EC:%d", enType, pif_enError);
#endif
	return NULL;
}

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief
 * @param pstOwner
 * @param usResponseTimeout
 */
void pifXmodem_SetResponseTimeout(PIF_stXmodem *pstOwner, uint16_t usResponseTimeout)
{
	pstOwner->__stTx.usTimeout = usResponseTimeout;
}

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief
 * @param pstOwner
 * @param usReceiveTimeout
 */
void pifXmodem_SetReceiveTimeout(PIF_stXmodem *pstOwner, uint16_t usReceiveTimeout)
{
	pstOwner->__stRx.usTimeout = usReceiveTimeout;
}

/**
 * @fn pifXmodem_AttachComm
 * @brief
 * @param pstOwner
 * @param pstComm
 */
void pifXmodem_AttachComm(PIF_stXmodem *pstOwner, PIF_stComm *pstComm)
{
	pifComm_AttachClient(pstComm, pstOwner);
	pstComm->evtParsing = _evtParsing;
	pstComm->evtSending = _evtSending;
}

/**
 * @fn pifXmodem_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtTxReceive
 * @param evtRxReceive
 */
void pifXmodem_AttachEvent(PIF_stXmodem *pstOwner, PIF_evtXmodemTxReceive evtTxReceive, PIF_evtXmodemRxReceive evtRxReceive)
{
	pstOwner->__stTx.evtReceive = evtTxReceive;
	pstOwner->__stRx.evtReceive = evtRxReceive;
}

/**
 * @fn pifXmodem_SendData
 * @brief
 * @param pstOwner
 * @param ucPacketNo
 * @param pucData
 * @param usDataSize
 * @return
 */
BOOL pifXmodem_SendData(PIF_stXmodem *pstOwner, uint8_t ucPacketNo, uint8_t *pucData, uint16_t usDataSize)
{
	uint16_t i, p, crc;

	if (!pstOwner->__stTx.evtReceive) {
		pif_enError = E_enNotSetEvent;
		goto fail;
	}

	if (!usDataSize || usDataSize > 128) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

	if (pstOwner->__stTx.enState != XTS_enIdle) {
		pif_enError = E_enInvalidState;
		goto fail;
	}

	pstOwner->__pucData[0] = ASCII_SOH;
	pstOwner->__pucData[1] = ucPacketNo;
	pstOwner->__pucData[2] = 0xFF - ucPacketNo;
	p = 3;
	for (i = 0; i < usDataSize; i++) {
		pstOwner->__pucData[p++] = pucData[i];
	}
	if (usDataSize < 128) {
		for (i = 0; i < 128 - usDataSize; i++) {
			pstOwner->__pucData[p++] = ASCII_SUB;
		}
	}
	switch (pstOwner->__enType) {
	case XT_enOriginal:
		crc = pifCheckSum(&pstOwner->__pucData[3], 128);
		pstOwner->__pucData[p++] = crc;
		break;

	case XT_enCRC:
		crc = pifCrc16(&pstOwner->__pucData[3], 128);
		pstOwner->__pucData[p++] = crc >> 8;
		pstOwner->__pucData[p++] = crc & 0xFF;
		break;
	}

	pstOwner->__stTx.usDataPos = 0;
	pstOwner->__stTx.enState = XTS_enSending;
	if (!pifPulse_StartItem(pstOwner->__stTx.pstTimer, pstOwner->__stTx.usTimeout * 1000L / s_pstXmodemTimer->_unPeriodUs)) {
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "XM Not start timer");
#endif
	}
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Xmodem:SendData(PN:%u DS:%u S:%u) EC:%d", ucPacketNo, usDataSize, pstOwner->__stTx.enState, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifXmodem_SendEot
 * @brief
 * @param pstOwner
 */
void pifXmodem_SendEot(PIF_stXmodem *pstOwner)
{
	pstOwner->__stTx.enState = XTS_enEOT;
}

/**
 * @fn pifXmodem_SendCancel
 * @brief
 * @param pstOwner
 */
void pifXmodem_SendCancel(PIF_stXmodem *pstOwner)
{
	pstOwner->__stTx.enState = XTS_enCAN;
}

/**
 * @fn pifXmodem_ReadyReceive
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifXmodem_ReadyReceive(PIF_stXmodem *pstOwner)
{
	if (!pstOwner->__stRx.evtReceive) {
		pif_enError = E_enNotSetEvent;
		return FALSE;
	}

	pstOwner->__stTx.enState = XTS_enSendC;
	return TRUE;
}
