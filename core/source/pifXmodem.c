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


static void _evtTimerRxTimeout(void *pvIssuer)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvIssuer;

	switch (pstOwner->__stTx.ui.enState) {
	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(Timeout) State:%u Cnt:%u", pstOwner->_usPifId,
				pstOwner->__stRx.enState, pstOwner->__stRx.usCount);
#endif
		pstOwner->__stTx.ui.enState = XTS_enNAK;
		pstOwner->__stRx.enState = XRS_enIdle;
		break;
	}
}

static void _evtTimerTxTimeout(void *pvIssuer)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvIssuer;

	switch (pstOwner->__stTx.ui.enState) {
	case XTS_enWaitResponse:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u Count=%u", pstOwner->_usPifId,
				pstOwner->__stTx.ui.enState, pstOwner->__stRx.usCount);
#endif
		pstOwner->__stTx.ui.enState = XTS_enIdle;
		if (pstOwner->__stTx.evtReceive) {
			(*pstOwner->__stTx.evtReceive)(ASCII_NAK, pstOwner->__pucData[1]);
		}
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u", pstOwner->_usPifId, pstOwner->__stTx.ui.enState);
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

static void _ParsingPacket(PIF_stXmodem *pstOwner, PifActCommReceiveData actReceiveData)
{
	PIF_stXmodemPacket *pstPacket = &pstOwner->__stRx.stPacket;
	uint8_t data;
	uint16_t crc;

	while ((*actReceiveData)(pstOwner->__pstComm, &data)) {
		switch (pstOwner->__stRx.enState)	{
		case XRS_enIdle:
			switch (data) {
			case 'C':
				pstOwner->__stRx.enState = XRS_enC;
				break;

			case ASCII_SOH:
				pstOwner->__stTx.ui.enState = XTS_enIdle;

				pstOwner->__stRx.usCount = 1;
				pstOwner->__stRx.enState = XRS_enGetHeader;
				if (!pifPulse_StartItem(pstOwner->__stRx.pstTimer, pstOwner->__stRx.usTimeout * 1000L / pstOwner->__pstTimer->_period1us)) {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_WARN, "XM(%u) Not start timer", pstOwner->_usPifId);
#endif
				}
				break;

			case ASCII_EOT:
				pstOwner->__stRx.enState = XRS_enEOT;

				pstOwner->__stTx.ui.enState = XTS_enACK;
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
					pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", pstOwner->_usPifId,
							c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)pstPacket->aucPacketNo[0],
							(unsigned int)pstPacket->aucPacketNo[1]);
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

					pstOwner->__stTx.ui.enState = XTS_enACK;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", pstOwner->_usPifId,
							c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)pstOwner->__stRx.usCrc, (unsigned int)crc);
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

			            pstOwner->__stTx.ui.enState = XTS_enACK;
					}
					else {
#ifndef __PIF_NO_LOG__
						pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", pstOwner->_usPifId,
								c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)pstOwner->__stRx.usCrc, (unsigned int)crc);
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

	pstOwner->__stTx.ui.enState = XTS_enNAK;
}

static void _evtParsing(void *pvOwner, PifActCommReceiveData actReceiveData)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvOwner;
	uint8_t data;

	if (pstOwner->__stTx.ui.enState == XTS_enWaitResponse) {
		if ((*actReceiveData)(pstOwner->__pstComm, &data)) {
			switch (data) {
			case ASCII_ACK:
			case ASCII_NAK:
			case ASCII_CAN:
				pifPulse_StopItem(pstOwner->__stTx.pstTimer);
				pstOwner->__stTx.ui.enState = XTS_enIdle;
				if (pstOwner->__stTx.evtReceive) {
					(*pstOwner->__stTx.evtReceive)(data, pstOwner->__pucData[1]);
				}
				break;
			}
		}
	}
	else {
		_ParsingPacket(pstOwner, actReceiveData);

		switch (pstOwner->__stRx.enState) {
		case XRS_enC:
			if (pstOwner->__stTx.evtReceive) {
				(*pstOwner->__stTx.evtReceive)(pstOwner->__stRx.enState, 0);
			}
			pstOwner->__stRx.enState = XRS_enIdle;
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_NONE, "C");
#endif
			break;

		case XRS_enEOT:
		case XRS_enCAN:
			if (pstOwner->__stRx.evtReceive) {
				(*pstOwner->__stRx.evtReceive)(pstOwner->__stRx.enState, NULL);
			}
			pstOwner->__stRx.enState = XRS_enIdle;
			break;

		case XRS_enSOH:
			if (pstOwner->__stRx.evtReceive) {
				(*pstOwner->__stRx.evtReceive)(pstOwner->__stRx.enState, &pstOwner->__stRx.stPacket);
			}
			pstOwner->__stRx.enState = XRS_enIdle;
			break;

		default:
			break;
		}
	}
}

static BOOL _evtSending(void *pvClient, PifActCommSendData actSendData)
{
	PIF_stXmodem *pstOwner = (PIF_stXmodem *)pvClient;
	uint16_t usLength;
	uint8_t ucData;
	static uint32_t unTimer1ms;

	switch (pstOwner->__stTx.ui.enState) {
	case XTS_enSendC:
		ucData = 'C';
		if ((*actSendData)(pstOwner->__pstComm, &ucData, 1)) {
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_NONE, "C");
#endif
			unTimer1ms = pif_cumulative_timer1ms;
			pstOwner->__stTx.ui.enState = XTS_enDelayC;
			return TRUE;
		}
		break;

	case XTS_enDelayC:
		if (PIF_CHECK_ELAPSE_TIME_1MS(unTimer1ms, 3000)) {			// 3000ms
			pstOwner->__stTx.ui.enState = XTS_enSendC;
		}
		break;

	case XTS_enSending:
    	usLength = (*actSendData)(pstOwner->__pstComm, pstOwner->__pucData + pstOwner->__stTx.usDataPos,
    			pstOwner->__usPacketSize - pstOwner->__stTx.usDataPos);
		pstOwner->__stTx.usDataPos += usLength;
		if (pstOwner->__stTx.usDataPos >= pstOwner->__usPacketSize) {
			pstOwner->__stTx.ui.enState = XTS_enWaitResponse;
		}
		return TRUE;

	case XTS_enEOT:
		if ((*actSendData)(pstOwner->__pstComm, &pstOwner->__stTx.ui.ucState, 1)) {
			pstOwner->__stTx.ui.enState = XTS_enWaitResponse;
		}
		return TRUE;

	case XTS_enCAN:
		if ((*actSendData)(pstOwner->__pstComm, &pstOwner->__stTx.ui.ucState, 1)) {
			if (pstOwner->__stTx.evtReceive) {
				pstOwner->__stTx.ui.enState = XTS_enWaitResponse;
			}
			else {
				pstOwner->__stTx.ui.enState = XTS_enIdle;
			}
		}
		return TRUE;

	case XTS_enACK:
	case XTS_enNAK:
		if ((*actSendData)(pstOwner->__pstComm, &pstOwner->__stTx.ui.ucState, 1)) {
			pstOwner->__stTx.ui.enState = XTS_enIdle;
		}
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/**
 * @fn pifXmodem_Create
 * @brief
 * @param usPifId
 * @param pstTimer
 * @param enType
 * @return
 */
PIF_stXmodem *pifXmodem_Create(PifId usPifId, PifPulse *pstTimer, PIF_enXmodemType enType)
{
    PIF_stXmodem *pstOwner = NULL;

    if (!pstTimer) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stXmodem), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    pstOwner->__pstTimer = pstTimer;
    switch (enType) {
    case XT_enOriginal:
    	pstOwner->__usPacketSize = 3 + 128 + 1;
    	break;

    case XT_enCRC:
    	pstOwner->__usPacketSize = 3 + 128 + 2;
    	break;

    default:
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    pstOwner->__pucData = calloc(sizeof(uint8_t), pstOwner->__usPacketSize);
    if (!pstOwner->__pucData) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    pstOwner->__stTx.pstTimer = pifPulse_AddItem(pstTimer, PT_ONCE);
    if (!pstOwner->__stTx.pstTimer) goto fail;

    pstOwner->__stRx.pstTimer = pifPulse_AddItem(pstTimer, PT_ONCE);
    if (!pstOwner->__stRx.pstTimer) goto fail;

    pstOwner->__enType = enType;

    pstOwner->__stTx.ui.enState = XTS_enIdle;
    pifPulse_AttachEvtFinish(pstOwner->__stTx.pstTimer, _evtTimerTxTimeout, pstOwner);
    pstOwner->__stTx.usTimeout = PIF_XMODEM_RESPONSE_TIMEOUT;

    pstOwner->__stRx.enState = XRS_enIdle;
    pifPulse_AttachEvtFinish(pstOwner->__stRx.pstTimer, _evtTimerRxTimeout, pstOwner);
    pstOwner->__stRx.usTimeout = PIF_XMODEM_RECEIVE_TIMEOUT;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    return pstOwner;

fail:
	pifXmodem_Destroy(&pstOwner);
	return NULL;
}

/**
 * @fn pifXmodem_Destroy
 * @brief
 * @param pp_owner
 */
void pifXmodem_Destroy(PIF_stXmodem** pp_owner)
{
    if (*pp_owner) {
    	PIF_stXmodem* pstOwner = *pp_owner;
		if (pstOwner->__pucData) {
			free(pstOwner->__pucData);
			pstOwner->__pucData = NULL;
		}
		if (pstOwner->__stRx.pstTimer) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__stRx.pstTimer);
		}
		if (pstOwner->__stTx.pstTimer) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__stTx.pstTimer);
		}
    	free(*pp_owner);
    	*pp_owner = NULL;
    }
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
void pifXmodem_AttachComm(PIF_stXmodem *pstOwner, PifComm *pstComm)
{
	pstOwner->__pstComm = pstComm;
	pifComm_AttachClient(pstComm, pstOwner);
	pstComm->evt_parsing = _evtParsing;
	pstComm->evt_sending = _evtSending;
}

/**
 * @fn pifXmodem_AttachEvtTxReceive
 * @brief
 * @param pstOwner
 * @param evtTxReceive
 */
void pifXmodem_AttachEvtTxReceive(PIF_stXmodem *pstOwner, PIF_evtXmodemTxReceive evtTxReceive)
{
	pstOwner->__stTx.evtReceive = evtTxReceive;
}

/**
 * @fn pifXmodem_AttachEvtRxReceive
 * @brief
 * @param pstOwner
 * @param evtRxReceive
 */
void pifXmodem_AttachEvtRxReceive(PIF_stXmodem *pstOwner, PIF_evtXmodemRxReceive evtRxReceive)
{
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
		pif_error = E_NOT_SET_EVENT;
		return FALSE;
	}

	if (!usDataSize || usDataSize > 128) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (pstOwner->__stTx.ui.enState != XTS_enIdle) {
		pif_error = E_INVALID_STATE;
		return FALSE;
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
	pstOwner->__stTx.ui.enState = XTS_enSending;
	if (!pifPulse_StartItem(pstOwner->__stTx.pstTimer, pstOwner->__stTx.usTimeout * 1000L / pstOwner->__pstTimer->_period1us)) {
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) Not start timer", pstOwner->_usPifId);
#endif
	}
	return TRUE;
}

/**
 * @fn pifXmodem_SendEot
 * @brief
 * @param pstOwner
 */
void pifXmodem_SendEot(PIF_stXmodem *pstOwner)
{
	pstOwner->__stTx.ui.enState = XTS_enEOT;
}

/**
 * @fn pifXmodem_SendCancel
 * @brief
 * @param pstOwner
 */
void pifXmodem_SendCancel(PIF_stXmodem *pstOwner)
{
	pstOwner->__stTx.ui.enState = XTS_enCAN;
}

/**
 * @fn pifXmodem_ReadyReceive
 * @brief
 * @param pstOwner
 */
void pifXmodem_ReadyReceive(PIF_stXmodem *pstOwner)
{
	pstOwner->__stTx.ui.enState = XTS_enSendC;
}
