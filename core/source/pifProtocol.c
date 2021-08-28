#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifProtocol.h"


static PIF_stProtocol *s_pstProtocol = NULL;
static uint8_t s_ucProtocolSize;
static uint8_t s_ucProtocolPos;

static PIF_stPulse *s_pstProtocolTimer;


#if PIF_PROTOCOL_RECEIVE_TIMEOUT

static void _evtTimerRxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pvIssuer;

#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ParsingPacket(Timeout) State:%u Len:%u Cnt:%u", pstOwner->_usPifId,
			pstOwner->__stRx.enState, pstOwner->__stRx.stPacket.usLength, pstOwner->__stRx.stPacket.usDataCount);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%x %x %x %x %x %x %x %x : %x : %x", pstOwner->__stRx.pucPacket[0], pstOwner->__stRx.pucPacket[1],
			pstOwner->__stRx.pucPacket[2], pstOwner->__stRx.pucPacket[3], pstOwner->__stRx.pucPacket[4], pstOwner->__stRx.pucPacket[5],
			pstOwner->__stRx.pucPacket[6], pstOwner->__stRx.pucPacket[7], pstOwner->__stRx.stPacket.ucCrc,
			pstOwner->__stRx.ucHeaderCount);
#endif
#endif
	pifRingBuffer_PutByte(pstOwner->__stTx.pstAnswerBuffer, ASCII_NAK);
	pstOwner->__stRx.enState = PRS_enIdle;
}

#endif

static void _evtTimerTxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pvIssuer;

	switch (pstOwner->__stTx.enState) {
	case PTS_enWaitResponse:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "PTC(%u) TxTimeout State:%u Len:%u Cnt:%u", pstOwner->_usPifId, pstOwner->__stTx.enState,
				pstOwner->__stRx.stPacket.usLength, pstOwner->__stRx.stPacket.usDataCount);
#endif
		pstOwner->__stTx.enState = PTS_enRetry;
		break;

	case PTS_enRetryDelay:
		pstOwner->__stTx.enState = PTS_enRetry;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "PTC(%u) TxTimeout State:%u", pstOwner->_usPifId, pstOwner->__stTx.enState);
#endif
		break;
	}
}

#define PKT_ERR_BIG_LENGHT		0
#define PKT_ERR_INVALID_DATA    1
#define PKT_ERR_WRONG_COMMAND	2
#define PKT_ERR_WRONG_CRC    	3
#define PKT_ERR_WRONG_ETX    	4

static const char *c_cPktErr[5] = {
		"Big Length",
		"Invalid Data",
		"Wrong Command",
		"Wrong CRC",
		"Wrong ETX"
};

static void _ParsingPacket(PIF_stProtocol *pstOwner, PIF_actCommReceiveData actReceiveData)
{
	PIF_stProtocolPacket *pstPacket = &pstOwner->__stRx.stPacket;
	uint8_t data;
	uint8_t ucPktErr;

	while ((*actReceiveData)(pstOwner->__pstComm, &data)) {
		switch (pstOwner->__stRx.enState)	{
		case PRS_enIdle:
			if (data == ASCII_STX) {
				pstOwner->__stRx.pucPacket[0] = data;
				pstOwner->__stRx.ucHeaderCount = 1;
				pstOwner->__stRx.enState = PRS_enGetHeader;
				pifCrc7_Init();
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
				if (pstOwner->__stTx.enState == PTS_enIdle) {
					pifPulse_StartItem(pstOwner->__stRx.pstTimer, PIF_PROTOCOL_RECEIVE_TIMEOUT);
				}
#endif
			}
			else if (pstOwner->__stTx.enState == PTS_enWaitResponse) {
				if (data == ASCII_ACK) {
					pstOwner->__stRx.enState = PRS_enAck;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enWarn, "PTC(%u):Receive NAK(%xh)", pstOwner->_usPifId, data);
#endif
#if PIF_PROTOCOL_RETRY_DELAY
					pifPulse_StartItem(pstOwner->__stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
					pstOwner->__stTx.enState = PTS_enRetryDelay;
#else
					pifPulse_StopItem(pstOwner->__stTx.pstTimer);
					pstOwner->__stTx.enState = PTS_enRetry;
#endif
					pstOwner->__stRx.enState = PRS_enIdle;
				}
			}
			break;

		case PRS_enGetHeader:
			if (data >= 0x20) {
				pifCrc7_Calcurate(data);
				pstOwner->__stRx.pucPacket[pstOwner->__stRx.ucHeaderCount] = data;
				pstOwner->__stRx.ucHeaderCount++;
				if (pstOwner->__stRx.ucHeaderCount >= pstOwner->__ucHeaderSize) {
					pstPacket->enFlags = pstOwner->__stRx.pucPacket[1];
					pstPacket->ucCommand = pstOwner->__stRx.pucPacket[2];
					switch (pstOwner->_enType) {
					case PT_enSmall:
						pstPacket->usLength = pstOwner->__stRx.pucPacket[3] - 0x20;
						break;

					case PT_enMedium:
						pstPacket->ucPacketId = pstOwner->__stRx.pucPacket[3];
						pstPacket->usLength = (pstOwner->__stRx.pucPacket[4] & 0x7F) + ((pstOwner->__stRx.pucPacket[5] & 0x7F) << 7);
						break;
					}
					if (!pstPacket->usLength) {
						pstOwner->__stRx.enState = PRS_enGetCrc;
						pstPacket->pucData = NULL;
						pstPacket->usDataCount = 0;
					}
					else if (pstPacket->usLength > pstOwner->__stRx.usPacketSize - 10) {
						ucPktErr = PKT_ERR_BIG_LENGHT;
						goto fail;
					}
					else {
						pstOwner->__stRx.enState = PRS_enGetData;
						pstOwner->__stRx.bDataLinkEscape = FALSE;
						pstPacket->pucData = pstOwner->__stRx.pucPacket + pstOwner->__ucHeaderSize;
						pstPacket->usDataCount = 0;
					}
				}
			}
			else {
				ucPktErr = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_enGetData:
			pifCrc7_Calcurate(data);
			if (data >= 0x20) {
				if (pstOwner->__stRx.bDataLinkEscape) {
					pstOwner->__stRx.bDataLinkEscape = FALSE;
					data &= 0x7F;
				}
				pstPacket->pucData[pstPacket->usDataCount] = data;
				pstPacket->usDataCount++;
				if (pstPacket->usDataCount >= pstPacket->usLength) {
					pstOwner->__stRx.enState = PRS_enGetCrc;
				}
			}
			else if (data == ASCII_DLE) {
				pstOwner->__stRx.bDataLinkEscape = TRUE;
			}
			else {
				ucPktErr = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_enGetCrc:
			if (data >= 0x20) {
				pstPacket->ucCrc = data;
				if (pstPacket->ucCrc == (0x80 | pifCrc7_Result())) {
					pstOwner->__stRx.enState = PRS_enGetTailer;
				}
				else {
					ucPktErr = PKT_ERR_WRONG_CRC;
					goto fail;
				}
			}
			else {
				ucPktErr = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_enGetTailer:
			if (data == ASCII_ETX) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
	            pifPulse_StopItem(pstOwner->__stRx.pstTimer);
#endif
	            pstOwner->__stRx.enState = PRS_enDone;
	            return;
			}
			else {
				ucPktErr = PKT_ERR_WRONG_ETX;
				goto fail;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ParsingPacket(%s) TS:%u RS:%u Len:%u Cnt:%u", pstOwner->_usPifId, c_cPktErr[ucPktErr],
			pstOwner->__stTx.enState, pstOwner->__stRx.enState, pstPacket->usLength, pstPacket->usDataCount);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%x %x %x %x %x %x %x %x : %x %x : %x", pstOwner->__stRx.pucPacket[0], pstOwner->__stRx.pucPacket[1],
			pstOwner->__stRx.pucPacket[2], pstOwner->__stRx.pucPacket[3], pstOwner->__stRx.pucPacket[4], pstOwner->__stRx.pucPacket[5],
			pstOwner->__stRx.pucPacket[6], pstOwner->__stRx.pucPacket[7], pstOwner->__stRx.stPacket.ucCrc, data,	pstOwner->__stRx.ucHeaderCount);
#endif
#endif
    if (pstOwner->__stTx.enState == PTS_enIdle) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    	pifPulse_StopItem(pstOwner->__stRx.pstTimer);
#endif
    	pifRingBuffer_PutByte(pstOwner->__stTx.pstAnswerBuffer, ASCII_NAK);
    }
    else if (pstOwner->__stTx.enState == PTS_enWaitResponse) {
#if PIF_PROTOCOL_RETRY_DELAY
    	pifPulse_StartItem(pstOwner->__stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
		pstOwner->__stTx.enState = PTS_enRetryDelay;
#else
		pifPulse_StopItem(pstOwner->__stTx.pstTimer);
		pstOwner->__stTx.enState = PTS_enRetry;
#endif
    }
	pstOwner->__stRx.enState = PRS_enIdle;
}

static void _evtParsing(void *pvOwner, PIF_actCommReceiveData actReceiveData)
{
	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pvOwner;
	PIF_stProtocolPacket *pstPacket;

	_ParsingPacket(pstOwner, actReceiveData);

    if (pstOwner->__stRx.enState == PRS_enDone) {
#ifndef __PIF_NO_LOG__
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_enNone, "\n%u> %x %x %x %x %x %x : %x", pstOwner->_usPifId,
    			pstOwner->__stRx.pucPacket[0],	pstOwner->__stRx.pucPacket[1], pstOwner->__stRx.pucPacket[2], pstOwner->__stRx.pucPacket[3],
				pstOwner->__stRx.pucPacket[4],	pstOwner->__stRx.pucPacket[5],	pstOwner->__stRx.stPacket.ucCrc);
#endif
#endif

    	pstPacket = &pstOwner->__stRx.stPacket;

    	if ((pstPacket->enFlags & PF_enType_Mask) == PF_enType_Question) {
        	const PIF_stProtocolQuestion *pstQuestion = pstOwner->__pstQuestions;
        	while (pstQuestion->ucCommand) {
        		if (pstPacket->ucCommand == pstQuestion->ucCommand) {
#ifndef __PIF_NO_LOG__
        	    	if (pstPacket->enFlags & PF_enLogPrint_Mask) {
        	    		pifLog_Printf(LT_enComm, "PTC(%u) Qt:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->_usPifId, pstPacket->ucCommand,
        	    				pstPacket->enFlags, pstPacket->ucPacketId, pstPacket->usLength, pstPacket->ucCrc);
        	    	}
#endif
        	    	if (pstQuestion->evtAnswer) (*pstQuestion->evtAnswer)(pstPacket);
        			break;
        		}
        		pstQuestion++;
        	}
    	}
    	else {
    	    if (pstOwner->__stTx.enState == PTS_enWaitResponse) {
#ifndef __PIF_NO_LOG__
    	    	if (pstPacket->enFlags & PF_enLogPrint_Mask) {
    	    		pifLog_Printf(LT_enComm, "PTC(%u) Rs:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->_usPifId, pstPacket->ucCommand,
    	    				pstPacket->enFlags, pstPacket->ucPacketId, pstPacket->usLength, pstPacket->ucCrc);
    	    	}
#endif

    	    	if (pstOwner->__stTx.ui.stInfo.ucCommand == pstPacket->ucCommand && (pstOwner->_enType == PT_enSmall ||
    	    			pstOwner->__stTx.ui.stInfo.ucPacketId == pstPacket->ucPacketId)) {
    				pifRingBuffer_Remove(pstOwner->__stTx.pstRequestBuffer, 5 + pstOwner->__stTx.ui.stInfo.usLength);
		            pifPulse_StopItem(pstOwner->__stTx.pstTimer);
					(*pstOwner->__stTx.pstRequest->evtResponse)(pstPacket);
					pstOwner->__stTx.enState = PTS_enIdle;
    	    	}
    	    	else {
#if PIF_PROTOCOL_RETRY_DELAY
    	    		pifPulse_StartItem(pstOwner->__stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
					pstOwner->__stTx.enState = PTS_enRetryDelay;
#else
					pifPulse_StopItem(pstOwner->__stTx.pstTimer);
					pstOwner->__stTx.enState = PTS_enRetry;
#endif
    	    	}
    	    }
#ifndef __PIF_NO_LOG__
    	    else {
    	    	pifLog_Printf(LT_enWarn, "PTC(%u) Invalid State %d", pstOwner->_usPifId, pstOwner->__stTx.enState);
    	    }
#endif
    	}
    	pstOwner->__stRx.enState = PRS_enIdle;
    }
    else if (pstOwner->__stRx.enState == PRS_enAck) {
		pifRingBuffer_Remove(pstOwner->__stTx.pstRequestBuffer, 5 + pstOwner->__stTx.ui.stInfo.usLength);
        pifPulse_StopItem(pstOwner->__stTx.pstTimer);
		(*pstOwner->__stTx.pstRequest->evtResponse)(NULL);
		pstOwner->__stTx.enState = PTS_enIdle;
    	pstOwner->__stRx.enState = PRS_enIdle;
    }
}

static BOOL _evtSending(void *pvOwner, PIF_actCommSendData actSendData)
{
	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pvOwner;
	uint16_t usLength;

	if (pstOwner->__stRx.enState != PRS_enIdle) return FALSE;

	if (!pifRingBuffer_IsEmpty(pstOwner->__stTx.pstAnswerBuffer)) {
		switch (pstOwner->__stTx.enState) {
	    case PTS_enIdle:
	    	if (!pifRingBuffer_IsEmpty(pstOwner->__stTx.pstAnswerBuffer)) {
				pstOwner->__stTx.ui.stInfo.usLength = pifRingBuffer_GetFillSize(pstOwner->__stTx.pstAnswerBuffer);
				pstOwner->__stTx.usPos = 0;
				pstOwner->__stTx.enState = PTS_enSending;
	    	}
	    	break;

	    case PTS_enSending:
	    	usLength = (*actSendData)(pstOwner->__pstComm, pifRingBuffer_GetTailPointer(pstOwner->__stTx.pstAnswerBuffer, pstOwner->__stTx.usPos),
	    			pifRingBuffer_GetLinerSize(pstOwner->__stTx.pstAnswerBuffer, pstOwner->__stTx.usPos));
	    	if (!usLength) return FALSE;

	    	pstOwner->__stTx.usPos += usLength;
			if (pstOwner->__stTx.usPos >= pstOwner->__stTx.ui.stInfo.usLength) {
				pifRingBuffer_Remove(pstOwner->__stTx.pstAnswerBuffer, pstOwner->__stTx.usPos);
				pstOwner->__stTx.enState = PTS_enIdle;
			}
			return TRUE;

		default:
			break;
	    }
	}
	else {
	    switch (pstOwner->__stTx.enState) {
	    case PTS_enIdle:
	    	if (!pifRingBuffer_IsEmpty(pstOwner->__stTx.pstRequestBuffer)) {
				pifRingBuffer_CopyToArray(pstOwner->__stTx.ui.ucInfo, 9, pstOwner->__stTx.pstRequestBuffer, 0);
				pstOwner->__stTx.usPos = 5;
				pstOwner->__stTx.enState = PTS_enSending;
	    	}
	    	break;

	    case PTS_enSending:
	    	usLength = (*actSendData)(pstOwner->__pstComm, pifRingBuffer_GetTailPointer(pstOwner->__stTx.pstRequestBuffer, pstOwner->__stTx.usPos),
	    			pifRingBuffer_GetLinerSize(pstOwner->__stTx.pstRequestBuffer, pstOwner->__stTx.usPos));
	    	pstOwner->__stTx.usPos += usLength;
			if (pstOwner->__stTx.usPos >= 5 + pstOwner->__stTx.ui.stInfo.usLength) {
				pstOwner->__stTx.enState = PTS_enWaitSended;
			}
			return TRUE;

	    case PTS_enWaitSended:
			if ((pstOwner->__stTx.ui.stInfo.ucFlags & PF_enResponse_Mask) == PF_enResponse_No) {
				pifRingBuffer_Remove(pstOwner->__stTx.pstRequestBuffer, 5 + pstOwner->__stTx.ui.stInfo.usLength);
				pstOwner->__stTx.enState = PTS_enIdle;
			}
			else {
				if (!pifPulse_StartItem(pstOwner->__stTx.pstTimer, pstOwner->__stTx.ui.stInfo.usTimeout)) {
					pif_enError = E_enOverflowBuffer;
					if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner->_usPifId);
					pifRingBuffer_Remove(pstOwner->__stTx.pstRequestBuffer, 5 + pstOwner->__stTx.ui.stInfo.usLength);
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enWarn, "PTC(%u) Not start timer", pstOwner->_usPifId);
#endif
					pstOwner->__stTx.enState = PTS_enIdle;
				}
				else {
					pstOwner->__stTx.enState = PTS_enWaitResponse;
				}
			}
			break;

	    case PTS_enRetry:
	    	pstOwner->__stTx.ui.stInfo.ucRetry--;
			if (pstOwner->__stTx.ui.stInfo.ucRetry) {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enWarn, "PTC(%u) Retry: %d", pstOwner->_usPifId, pstOwner->__stTx.ui.stInfo.ucRetry);
#endif
				pstOwner->__stRx.enState = PRS_enIdle;
				pstOwner->__stTx.usPos = 5;
				pstOwner->__stTx.enState = PTS_enSending;
			}
			else {
				pif_enError = E_enTransferFailed;
				if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner->_usPifId);
				pifRingBuffer_Remove(pstOwner->__stTx.pstRequestBuffer, 5 + pstOwner->__stTx.ui.stInfo.usLength);
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enError, "PTC(%u) EventSending EC:%d", pstOwner->_usPifId, pif_enError);
#endif
				pstOwner->__stTx.enState = PTS_enIdle;
			}
	    	break;

		default:
			break;
	    }
	}
	return FALSE;
}

/**
 * @fn pifProtocol_Init
 * @brief
 * @param ucSize
 * @param pstTimer
 * @return
 */
BOOL pifProtocol_Init(uint8_t ucSize, PIF_stPulse *pstTimer)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstProtocol = calloc(sizeof(PIF_stProtocol), ucSize);
    if (!s_pstProtocol) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucProtocolSize = ucSize;
    s_ucProtocolPos = 0;

    s_pstProtocolTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifProtocol_Exit
 * @brief
 */
void pifProtocol_Exit()
{
    if (s_pstProtocol) {
        for (int i = 0; i < s_ucProtocolPos; i++) {
        	PIF_stProtocol *pstOwner = (PIF_stProtocol *)&s_pstProtocol[i];
        	if (pstOwner->__stRx.pucPacket) {
        		free(pstOwner->__stRx.pucPacket);
        		pstOwner->__stRx.pucPacket = NULL;
        	}
        	pifRingBuffer_Exit(pstOwner->__stTx.pstRequestBuffer);
        	pifRingBuffer_Exit(pstOwner->__stTx.pstAnswerBuffer);
        	if (pstOwner->__stRx.pstTimer) {
        		pifPulse_RemoveItem(s_pstProtocolTimer, pstOwner->__stRx.pstTimer);
        	}
        	if (pstOwner->__stTx.pstTimer) {
        		pifPulse_RemoveItem(s_pstProtocolTimer, pstOwner->__stTx.pstTimer);
        	}
        }
    	free(s_pstProtocol);
        s_pstProtocol = NULL;
    }
}

/**
 * @fn pifProtocol_Add
 * @brief
 * @param usPifId
 * @param enType
 * @param pstQuestion
 * @return
 */
PIF_stProtocol *pifProtocol_Add(PIF_usId usPifId, PIF_enProtocolType enType,
		const PIF_stProtocolQuestion *pstQuestions)
{
    if (s_ucProtocolPos >= s_ucProtocolSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

	const PIF_stProtocolQuestion *pstQuestion = pstQuestions;
	while (pstQuestion->ucCommand) {
		if (pstQuestion->ucCommand < 0x20) {
	        pif_enError = E_enInvalidParam;
			goto fail;
		}
		pstQuestion++;
	}

    PIF_stProtocol *pstOwner = &s_pstProtocol[s_ucProtocolPos];

    switch (enType) {
	case PT_enSmall:
		pstOwner->__ucHeaderSize = 4;
		break;

	case PT_enMedium:
		pstOwner->__ucHeaderSize = 6;
		break;

	default:
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    pstOwner->__stRx.pucPacket = calloc(sizeof(uint8_t), 10 + PIF_PROTOCOL_RX_PACKET_SIZE);
    if (!pstOwner->__stRx.pucPacket) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    pstOwner->__stRx.pstTimer = pifPulse_AddItem(s_pstProtocolTimer, PT_enOnce);
    if (!pstOwner->__stRx.pstTimer) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__stRx.pstTimer, _evtTimerRxTimeout, pstOwner);
#endif

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;

    pstOwner->__stTx.pstRequestBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_PROTOCOL_TX_REQUEST_SIZE);
    if (!pstOwner->__stTx.pstRequestBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->__stTx.pstRequestBuffer, "RQB");

    pstOwner->__stTx.pstAnswerBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_PROTOCOL_TX_ANSWER_SIZE);
    if (!pstOwner->__stTx.pstAnswerBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->__stTx.pstAnswerBuffer, "RSB");

    pstOwner->__stTx.pstTimer = pifPulse_AddItem(s_pstProtocolTimer, PT_enOnce);
    if (!pstOwner->__stTx.pstTimer) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__stTx.pstTimer, _evtTimerTxTimeout, pstOwner);

    pstOwner->_enType = enType;
    pstOwner->__pstQuestions = pstQuestions;
    pstOwner->_usPifId = usPifId;
    pstOwner->__ucPacketId = 0x20;
    pstOwner->__stRx.usPacketSize = 10 + PIF_PROTOCOL_RX_PACKET_SIZE;

    s_ucProtocolPos = s_ucProtocolPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) Add(T:%d) EC:%d", usPifId, enType, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifProtocol_ResizeRxPacket
 * @brief
 * @param pvOwner
 * @param usRxPacketSize
 * @return
 */
BOOL pifProtocol_ResizeRxPacket(PIF_stProtocol *pstOwner, uint16_t usRxPacketSize)
{
    if (!usRxPacketSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstOwner->__stRx.pucPacket = realloc(pstOwner->__stRx.pucPacket, sizeof(uint8_t) * (10 + usRxPacketSize));
    if (!pstOwner->__stRx.pucPacket) {
        pif_enError = E_enOutOfHeap;
    	goto fail;
    }

    pstOwner->__stRx.usPacketSize = 10 + usRxPacketSize;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeRxPacket(S:%u) EC:%d", pstOwner->_usPifId, usRxPacketSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifProtocol_ResizeTxRequest
 * @brief
 * @param pvOwner
 * @param usTxRequestSize
 * @return
 */
BOOL pifProtocol_ResizeTxRequest(PIF_stProtocol *pstOwner, uint16_t usTxRequestSize)
{
    if (!usTxRequestSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    return pifRingBuffer_ResizeHeap(pstOwner->__stTx.pstRequestBuffer, usTxRequestSize);

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeTxRequest(S:%u) EC:%d", pstOwner->_usPifId, usTxRequestSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifProtocol_ResizeTxResponse
 * @brief
 * @param pvOwner
 * @param usTxResponseSize
 * @return
 */
BOOL pifProtocol_ResizeTxResponse(PIF_stProtocol *pstOwner, uint16_t usTxResponseSize)
{
    if (usTxResponseSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    return pifRingBuffer_ResizeHeap(pstOwner->__stTx.pstAnswerBuffer, usTxResponseSize);

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeTxResponset(S:%u) EC:%d", pstOwner->_usPifId, usTxResponseSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifProtocol_AttachComm
 * @brief
 * @param pstOwner
 * @param pstComm
 */
void pifProtocol_AttachComm(PIF_stProtocol *pstOwner, PIF_stComm *pstComm)
{
	pstOwner->__pstComm = pstComm;
	pifComm_AttachClient(pstComm, pstOwner);
	pstComm->evtParsing = _evtParsing;
	pstComm->evtSending = _evtSending;
}

static BOOL _MakeRequest(PIF_stProtocol *pstOwner, uint16_t usDataSize)
{
	uint16_t usLength;
	uint8_t aucHeader[13];
	uint8_t aucTailer[2];
	uint8_t ucPacketId = 0, ucData;
	const PIF_stProtocolRequest *pstTable = pstOwner->__stTx.pstRequest;

	pifRingBuffer_BackupHead(pstOwner->__stTx.pstRequestBuffer);

	uint16_t usCount = 0;
	for (uint16_t i = 0; i < usDataSize; i++) {
		if (pstOwner->__stTx.pucData[i] < 0x20) usCount++;
	}

	pifCrc7_Init();

	aucHeader[5] = ASCII_STX;
	aucHeader[6] = PF_enAlways | pstTable->enFlags;
	aucHeader[7] = pstTable->ucCommand;
	switch (pstOwner->_enType) {
	case PT_enSmall:
		aucHeader[8] = 0x20 + usDataSize;
		break;

	case PT_enMedium:
		ucPacketId = pstOwner->__ucPacketId;
		aucHeader[8] = ucPacketId;
		pstOwner->__ucPacketId++;
		if (!pstOwner->__ucPacketId) pstOwner->__ucPacketId = 0x20;
		aucHeader[9] = 0x80 | (usDataSize & 0x7F);
		aucHeader[10] = 0x80 | ((usDataSize >> 7) & 0x7F);
		break;
	}
	usLength = pstOwner->__ucHeaderSize + usDataSize + usCount + 2;
	aucHeader[0] = usLength & 0xFF;
	aucHeader[1] = (usLength >> 8) & 0xFF;
	aucHeader[2] = pstTable->usTimeout & 0xFF;
	aucHeader[3] = (pstTable->usTimeout >> 8) & 0xFF;
	aucHeader[4] = pstTable->ucRetry;
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstRequestBuffer, aucHeader, 5 + pstOwner->__ucHeaderSize)) goto fail;
	for (int i = 1; i < pstOwner->__ucHeaderSize; i++) {
		pifCrc7_Calcurate(aucHeader[5 + i]);
	}

	for (uint16_t i = 0; i < usDataSize; i++) {
		ucData = pstOwner->__stTx.pucData[i];
		if (ucData < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(pstOwner->__stTx.pstRequestBuffer, ASCII_DLE)) goto fail;
			ucData |= 0x80;
		}
		pifCrc7_Calcurate(ucData);
		if (!pifRingBuffer_PutByte(pstOwner->__stTx.pstRequestBuffer, ucData)) goto fail;
	}

	aucTailer[0] = 0x80 | pifCrc7_Result();
	aucTailer[1] = ASCII_ETX;
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstRequestBuffer, aucTailer, 2)) goto fail;

#ifndef __PIF_NO_LOG__
	if (pstTable->enFlags & PF_enLogPrint_Mask) {
		pifLog_Printf(LT_enComm, "PTC(%u) Rq:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->_usPifId, pstTable->ucCommand,
				pstTable->enFlags, ucPacketId, usDataSize, aucTailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%u< %x %x %x %x %x %x %x %x", pstOwner->_usPifId,
			aucHeader[5], aucHeader[6], aucHeader[7], aucHeader[8], aucHeader[9], aucHeader[10], aucTailer[0], aucTailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(pstOwner->__stTx.pstRequestBuffer);
	if (!pif_enError) pif_enError = E_enOverflowBuffer;
	return FALSE;
}

/**
 * @fn pifProtocol_MakeRequest
 * @brief
 * @param pvOwner
 * @param pstRequest
 * @param pucData
 * @param usDataSize
 * @return
 */
BOOL pifProtocol_MakeRequest(PIF_stProtocol *pstOwner, const PIF_stProtocolRequest *pstRequest, uint8_t *pucData, uint16_t usDataSize)
{
	if (pstOwner->__stTx.enState != PTS_enIdle) {
		pif_enError = E_enInvalidState;
		return FALSE;
	}

	pstOwner->__stTx.pstRequest = pstRequest;
	pstOwner->__stTx.pucData = pucData;
	pstOwner->__stTx.usDataSize = usDataSize;
	return _MakeRequest(pstOwner, usDataSize);
}

/**
 * @fn pifProtocol_MakeAnswer
 * @brief
 * @param pvOwner
 * @param pstQuestion
 * @param enFlags
 * @param pucData
 * @param usDataSize
 * @return
 */
BOOL pifProtocol_MakeAnswer(PIF_stProtocol *pstOwner, PIF_stProtocolPacket *pstQuestion, uint8_t enFlags,
		uint8_t *pucData, uint16_t usDataSize)
{
	uint8_t aucHeader[8];
	uint8_t aucTailer[2];
	uint8_t ucPacketId = 0, ucData;

	pifRingBuffer_BackupHead(pstOwner->__stTx.pstAnswerBuffer);

	pifCrc7_Init();

	aucHeader[0] = ASCII_STX;
	aucHeader[1] = PF_enAlways | PF_enType_Answer | enFlags;
	aucHeader[2] = pstQuestion->ucCommand;
	switch (pstOwner->_enType) {
	case PT_enSmall:
		aucHeader[3] = 0x20 + usDataSize;
		break;

	case PT_enMedium:
		ucPacketId = pstQuestion->ucPacketId;
		aucHeader[3] = ucPacketId;
		aucHeader[4] = 0x80 | (usDataSize & 0x7F);
		aucHeader[5] = 0x80 | ((usDataSize >> 7) & 0x7F);
		break;
	}
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstAnswerBuffer, aucHeader, pstOwner->__ucHeaderSize)) goto fail;
	for (int i = 1; i < pstOwner->__ucHeaderSize; i++) {
		pifCrc7_Calcurate(aucHeader[i]);
	}

	for (uint16_t i = 0; i < usDataSize; i++) {
		ucData = pucData[i];
		if (ucData < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(pstOwner->__stTx.pstAnswerBuffer, ASCII_DLE)) goto fail;
			ucData |= 0x80;
		}
		pifCrc7_Calcurate(ucData);
		if (!pifRingBuffer_PutByte(pstOwner->__stTx.pstAnswerBuffer, ucData)) goto fail;
	}

	aucTailer[0] = 0x80 | pifCrc7_Result();
	aucTailer[1] = ASCII_ETX;
	if (!pifRingBuffer_PutData(pstOwner->__stTx.pstAnswerBuffer, aucTailer, 2)) goto fail;

#ifndef __PIF_NO_LOG__
	if (enFlags & PF_enLogPrint_Mask) {
		pifLog_Printf(LT_enComm, "PTC(%u) As:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->_usPifId, pstQuestion->ucCommand,
				enFlags, ucPacketId, usDataSize, aucTailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%u< %x %x %x %x %x %x %x %x", pstOwner->_usPifId,
			aucHeader[0], aucHeader[1], aucHeader[2], aucHeader[3], aucHeader[4], aucHeader[5],	aucTailer[0], aucTailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(pstOwner->__stTx.pstAnswerBuffer);
	if (!pif_enError) pif_enError = E_enOverflowBuffer;
	return FALSE;
}
