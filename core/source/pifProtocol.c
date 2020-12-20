#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifProtocol.h"


typedef enum _PIF_enProtocolRxState
{
	PRS_enIdle			= 0,
	PRS_enGetHeader		= 1,
	PRS_enGetData		= 2,
	PRS_enGetCrc		= 3,
	PRS_enGetTailer		= 4,
	PRS_enDone			= 5,
	PRS_enAck			= 6,
	PRS_enError			= 7
} PIF_enProtocolRxState;

typedef enum _PIF_enProtocolTxState
{
	PTS_enIdle			= 0,
	PTS_enSending		= 1,
	PTS_enWaitSended	= 2,
	PTS_enWaitResponse	= 3,
	PTS_enRetryDelay	= 4,
	PTS_enRetry			= 5
} PIF_enProtocolTxState;


typedef struct _PIF_stProtocolRx
{
	PIF_enProtocolRxState enState;
	uint8_t *pucPacket;
	uint16_t usPacketSize;
	uint8_t ucHeaderCount;
	BOOL bDataLinkEscape;
	PIF_stProtocolPacket stPacket;
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
	PIF_stPulseItem *pstTimer;
#endif
} PIF_stProtocolRx;

typedef struct _PIF_stProtocolTx
{
    PIF_stRingBuffer stRequestBuffer;
    PIF_stRingBuffer stAnswerBuffer;
	const PIF_stProtocolRequest *pstRequest;
	uint8_t *pucData;
	uint16_t usDataSize;
	PIF_enProtocolTxState enState;
	union {
		uint8_t ucInfo[9];
		struct {
			uint16_t usLength;
			uint16_t usTimeout;
			uint8_t ucRetry;
			uint8_t ucStx;
			uint8_t ucFlags;
			uint8_t ucCommand;
			uint8_t ucPacketId;
		};
	};
	uint16_t usPos;
	PIF_stPulseItem *pstTimer;
} PIF_stProtocolTx;

typedef struct _PIF_stProtocolBase
{
	// Public Member Variable
	PIF_stProtocol stOwner;

	// Private Member Variable
    const PIF_stProtocolQuestion *pstQuestions;
    PIF_stProtocolRx stRx;
    PIF_stProtocolTx stTx;
	uint8_t ucHeaderSize;
	uint8_t ucPacketId;
} PIF_stProtocolBase;


static PIF_stProtocolBase *s_pstProtocolBase = NULL;
static uint8_t s_ucProtocolBaseSize;
static uint8_t s_ucProtocolBasePos;

static PIF_stPulse *s_pstProtocolTimer;


#if PIF_PROTOCOL_RECEIVE_TIMEOUT

static void _evtTimerRxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pvIssuer;

#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ParsingPacket(Timeout) State:%u Len:%u Cnt:%u", pstBase->stOwner.unDeviceCode,
			pstBase->stRx.enState, pstBase->stRx.stPacket.usLength, pstBase->stRx.stPacket.usDataCount);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%x %x %x %x %x %x %x %x : %x : %x", pstBase->stRx.pucPacket[0], pstBase->stRx.pucPacket[1],
			pstBase->stRx.pucPacket[2], pstBase->stRx.pucPacket[3], pstBase->stRx.pucPacket[4], pstBase->stRx.pucPacket[5],
			pstBase->stRx.pucPacket[6], pstBase->stRx.pucPacket[7], pstBase->stRx.stPacket.ucCrc,
			pstBase->stRx.ucHeaderCount);
#endif
#endif
	pifRingBuffer_PutByte(&pstBase->stTx.stAnswerBuffer, ASCII_NAK);
	pstBase->stRx.enState = PRS_enIdle;
}

#endif

static void _evtTimerTxTimeout(void *pvIssuer)
{
	if (!pvIssuer) {
		pif_enError = E_enInvalidParam;
		return;
	}

	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pvIssuer;

	switch (pstBase->stTx.enState) {
	case PTS_enWaitResponse:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "PTC(%u) TxTimeout State:%u Len:%u Cnt:%u", pstBase->stOwner.unDeviceCode, pstBase->stTx.enState,
				pstBase->stRx.stPacket.usLength, pstBase->stRx.stPacket.usDataCount);
#endif
		pstBase->stTx.enState = PTS_enRetry;
		pstBase->stRx.enState = PRS_enIdle;
		break;

	case PTS_enRetryDelay:
		pstBase->stTx.enState = PTS_enRetry;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "PTC(%u) TxTimeout State:%u", pstBase->stOwner.unDeviceCode, pstBase->stTx.enState);
#endif
		break;
	}
}

#define PKT_ERR_BIG_LENGHT		0
#define PKT_ERR_INVALID_DATA    1
#define PKT_ERR_WRONG_COMMAND	2
#define PKT_ERR_WRONG_CRC    	3
#define PKT_ERR_WRONG_ETX    	4

const char *c_cPktErr[5] = {
		"Big Length",
		"Invalid Data",
		"Wrong Command",
		"Wrong CRC",
		"Wrong ETX"
};

static void _ParsingPacket(PIF_stProtocolBase *pstBase, PIF_stRingBuffer *pstBuffer)
{
	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pstBase;
	PIF_stProtocolPacket *pstPacket;
	uint8_t data;
	uint8_t ucPktErr;

	while (pifRingBuffer_GetByte(pstBuffer, &data)) {
		pstPacket = &pstBase->stRx.stPacket;

		switch (pstBase->stRx.enState)	{
		case PRS_enIdle:
			if (data == ASCII_STX) {
				pstBase->stRx.pucPacket[0] = data;
				pstBase->stRx.ucHeaderCount = 1;
				pstBase->stRx.enState = PRS_enGetHeader;
				pifCrc7_Init();
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
				if (pstBase->stTx.enState == PTS_enIdle) {
					pifPulse_StartItem(pstBase->stRx.pstTimer, PIF_PROTOCOL_RECEIVE_TIMEOUT);
				}
#endif
			}
			else if (pstBase->stTx.enState == PTS_enWaitResponse) {
				if (data == ASCII_ACK) {
					pstBase->stRx.enState = PRS_enAck;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_enWarn, "PTC(%u):Receive NAK(%xh)", pstBase->stOwner.unDeviceCode, data);
#endif
#if PIF_PROTOCOL_RETRY_DELAY
					pifPulse_StartItem(pstBase->stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
					pstBase->stTx.enState = PTS_enRetryDelay;
#else
					pifPulse_StopItem(pstBase->stTx.pstTimer);
					pstBase->stTx.enState = PTS_enRetry;
#endif
					pstBase->stRx.enState = PRS_enIdle;
				}
			}
			break;

		case PRS_enGetHeader:
			if (data >= 0x20) {
				pifCrc7_Calcurate(data);
				pstBase->stRx.pucPacket[pstBase->stRx.ucHeaderCount] = data;
				pstBase->stRx.ucHeaderCount++;
				if (pstBase->stRx.ucHeaderCount >= pstBase->ucHeaderSize) {
					pstPacket->enFlags = pstBase->stRx.pucPacket[1];
					pstPacket->ucCommand = pstBase->stRx.pucPacket[2];
					switch (pstOwner->enType) {
					case PT_enSmall:
						pstPacket->usLength = pstBase->stRx.pucPacket[3] - 0x20;
						break;

					case PT_enMedium:
						pstPacket->ucPacketId = pstBase->stRx.pucPacket[3];
						pstPacket->usLength = (pstBase->stRx.pucPacket[4] & 0x7F) + ((pstBase->stRx.pucPacket[5] & 0x7F) << 7);
						break;
					}
					if (!pstPacket->usLength) {
						pstBase->stRx.enState = PRS_enGetCrc;
						pstPacket->pucData = NULL;
						pstPacket->usDataCount = 0;
					}
					else if (pstPacket->usLength > pstBase->stRx.usPacketSize - 10) {
						ucPktErr = PKT_ERR_BIG_LENGHT;
						goto fail;
					}
					else {
						pstBase->stRx.enState = PRS_enGetData;
						pstBase->stRx.bDataLinkEscape = FALSE;
						pstPacket->pucData = pstBase->stRx.pucPacket + pstBase->ucHeaderSize;
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
				if (pstBase->stRx.bDataLinkEscape) {
					pstBase->stRx.bDataLinkEscape = FALSE;
					data &= 0x7F;
				}
				pstPacket->pucData[pstPacket->usDataCount] = data;
				pstPacket->usDataCount++;
				if (pstPacket->usDataCount >= pstPacket->usLength) {
					pstBase->stRx.enState = PRS_enGetCrc;
				}
			}
			else if (data == ASCII_DLE) {
				pstBase->stRx.bDataLinkEscape = TRUE;
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
					pstBase->stRx.enState = PRS_enGetTailer;
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
	            pifPulse_StopItem(pstBase->stRx.pstTimer);
#endif
	            pstBase->stRx.enState = PRS_enDone;
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
	pifLog_Printf(LT_enError, "PTC(%u) ParsingPacket(%s) TS:%u RS:%u Len:%u Cnt:%u", pstOwner->unDeviceCode, c_cPktErr[ucPktErr],
			pstBase->stTx.enState, pstBase->stRx.enState, pstPacket->usLength, pstPacket->usDataCount);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%x %x %x %x %x %x %x %x : %x %x : %x", pstBase->stRx.pucPacket[0], pstBase->stRx.pucPacket[1],
			pstBase->stRx.pucPacket[2], pstBase->stRx.pucPacket[3], pstBase->stRx.pucPacket[4], pstBase->stRx.pucPacket[5],
			pstBase->stRx.pucPacket[6], pstBase->stRx.pucPacket[7], pstBase->stRx.stPacket.ucCrc, data,	pstBase->stRx.ucHeaderCount);
#endif
#endif
    if (pstBase->stTx.enState == PTS_enIdle) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    	pifPulse_StopItem(pstBase->stRx.pstTimer);
#endif
    	pifRingBuffer_PutByte(&pstBase->stTx.stAnswerBuffer, ASCII_NAK);
    }
    else if (pstBase->stTx.enState == PTS_enWaitResponse) {
#if PIF_PROTOCOL_RETRY_DELAY
    	pifPulse_StartItem(pstBase->stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
		pstBase->stTx.enState = PTS_enRetryDelay;
#else
		pifPulse_StopItem(pstBase->stTx.pstTimer);
		pstBase->stTx.enState = PTS_enRetry;
#endif
    }
	pstBase->stRx.enState = PRS_enIdle;
}

static void _evtParsing(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pvOwner;
	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pstBase;
	PIF_stProtocolPacket *pstPacket;

	_ParsingPacket(pstBase, pstBuffer);

    if (pstBase->stRx.enState == PRS_enDone) {
#ifndef __PIF_NO_LOG__
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_enNone, "\n%u> %x %x %x %x %x %x : %x", pstOwner->unDeviceCode,
    			pstBase->stRx.pucPacket[0],	pstBase->stRx.pucPacket[1], pstBase->stRx.pucPacket[2], pstBase->stRx.pucPacket[3],
				pstBase->stRx.pucPacket[4],	pstBase->stRx.pucPacket[5],	pstBase->stRx.stPacket.ucCrc);
#endif
#endif

    	pstPacket = &pstBase->stRx.stPacket;

    	if ((pstPacket->enFlags & PF_enType_Mask) == PF_enType_Question) {
        	const PIF_stProtocolQuestion *pstQuestion = pstBase->pstQuestions;
        	while (pstQuestion->ucCommand) {
        		if (pstPacket->ucCommand == pstQuestion->ucCommand) {
#ifndef __PIF_NO_LOG__
        	    	if (pstPacket->enFlags & PF_enLogPrint_Mask) {
        	    		pifLog_Printf(LT_enComm, "PTC(%u) Qt:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->unDeviceCode, pstPacket->ucCommand,
        	    				pstPacket->enFlags, pstPacket->ucPacketId, pstPacket->usLength, pstPacket->ucCrc);
        	    	}
#endif

        	    	if ((pstPacket->enFlags & PF_enResponse_Mask) == PF_enResponse_Ack) {
        	    		pifRingBuffer_PutByte(&pstBase->stTx.stAnswerBuffer, ASCII_ACK);
        	    	}
       	    		(*pstQuestion->evtQuestion)(pstPacket);
        			break;
        		}
        		pstQuestion++;
        	}
    	}
    	else {
    	    if (pstBase->stTx.enState == PTS_enWaitResponse) {
#ifndef __PIF_NO_LOG__
    	    	if (pstPacket->enFlags & PF_enLogPrint_Mask) {
    	    		pifLog_Printf(LT_enComm, "PTC(%u) Rs:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->unDeviceCode, pstPacket->ucCommand,
    	    				pstPacket->enFlags, pstPacket->ucPacketId, pstPacket->usLength, pstPacket->ucCrc);
    	    	}
#endif

    	    	if (pstBase->stTx.ucCommand == pstPacket->ucCommand && (pstOwner->enType == PT_enSmall ||
    	    			pstBase->stTx.ucPacketId == pstPacket->ucPacketId)) {
    				pifRingBuffer_Remove(&pstBase->stTx.stRequestBuffer, 5 + pstBase->stTx.usLength);
		            pifPulse_StopItem(pstBase->stTx.pstTimer);
					(*pstBase->stTx.pstRequest->evtResponse)(pstPacket);
					pstBase->stTx.enState = PTS_enIdle;
    	    	}
    	    	else {
#if PIF_PROTOCOL_RETRY_DELAY
    	    		pifPulse_StartItem(pstBase->stTx.pstTimer, PIF_PROTOCOL_RETRY_DELAY);
					pstBase->stTx.enState = PTS_enRetryDelay;
#else
					pifPulse_StopItem(pstBase->stTx.pstTimer);
					pstBase->stTx.enState = PTS_enRetry;
#endif
    	    	}
    	    }
#ifndef __PIF_NO_LOG__
    	    else {
    	    	pifLog_Printf(LT_enWarn, "PTC(%u) Invalid State %d", pstBase->stOwner.unDeviceCode, pstBase->stTx.enState);
    	    }
#endif
    	}
    	pstBase->stRx.enState = PRS_enIdle;
    }
    else if (pstBase->stRx.enState == PRS_enAck) {
		pifRingBuffer_Remove(&pstBase->stTx.stRequestBuffer, 5 + pstBase->stTx.usLength);
        pifPulse_StopItem(pstBase->stTx.pstTimer);
		(*pstBase->stTx.pstRequest->evtResponse)(NULL);
		pstBase->stTx.enState = PTS_enIdle;
    	pstBase->stRx.enState = PRS_enIdle;
    }
}

static BOOL _evtSending(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pvOwner;
	PIF_stProtocol *pstOwner = (PIF_stProtocol *)pstBase;
	uint16_t usLength;

	if (pstBase->stRx.enState != PRS_enIdle) return FALSE;

	if (!pifRingBuffer_IsEmpty(&pstBase->stTx.stAnswerBuffer)) {
		usLength = pifRingBuffer_CopyAll(pstBuffer, &pstBase->stTx.stAnswerBuffer, 0);
		pifRingBuffer_Remove(&pstBase->stTx.stAnswerBuffer, usLength);
		return TRUE;
	}
	else {
	    switch (pstBase->stTx.enState) {
	    case PTS_enIdle:
	    	if (!pifRingBuffer_IsEmpty(&pstBase->stTx.stRequestBuffer)) {
				pifRingBuffer_CopyToArray(pstBase->stTx.ucInfo, &pstBase->stTx.stRequestBuffer, 9);
				pstBase->stTx.usPos = 0;
				pstBase->stTx.enState = PTS_enSending;
	    	}
	    	break;

	    case PTS_enSending:
	    	if (pifRingBuffer_IsEmpty(pstBuffer)) {
				pstBase->stTx.usPos += pifRingBuffer_CopyAll(pstBuffer, &pstBase->stTx.stRequestBuffer, 5 + pstBase->stTx.usPos);
				if (pstBase->stTx.usPos >= pstBase->stTx.usLength) {
					pstBase->stTx.enState = PTS_enWaitSended;
				}
	    	}
	    	break;

	    case PTS_enWaitSended:
	    	if (pifRingBuffer_IsEmpty(pstBuffer)) {
	    		if ((pstBase->stTx.ucFlags & PF_enResponse_Mask) == PF_enResponse_No) {
    				pifRingBuffer_Remove(&pstBase->stTx.stRequestBuffer, 5 + pstBase->stTx.usLength);
	    			pstBase->stTx.enState = PTS_enIdle;
	    		}
	    		else {
#ifndef __PIF_NO_LOG__
	    			if (!pifPulse_StartItem(pstBase->stTx.pstTimer, pstBase->stTx.usTimeout)) {
						pifLog_Printf(LT_enWarn, "PTC(%u) Not start timer", pstBase->stOwner.unDeviceCode);
					}
#endif
	    			pstBase->stTx.enState = PTS_enWaitResponse;
	    		}
				return TRUE;
	    	}
	    	break;

	    case PTS_enRetry:
	    	pstBase->stTx.ucRetry--;
			if (pstBase->stTx.ucRetry) {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enWarn, "PTC(%u) Retry: %d", pstOwner->unDeviceCode, pstBase->stTx.ucRetry);
#endif
				pstBase->stTx.usPos = 0;
				pstBase->stTx.enState = PTS_enSending;
			}
			else {
				if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner->unDeviceCode);
				pifRingBuffer_Remove(&pstBase->stTx.stRequestBuffer, 5 + pstBase->stTx.usLength);
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_enError, "PTC(%u) EventSending EC:%d", pstOwner->unDeviceCode, pif_enError);
#endif
				pstBase->stTx.enState = PTS_enIdle;
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
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifProtocol_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstProtocolBase = calloc(sizeof(PIF_stProtocolBase), ucSize);
    if (!s_pstProtocolBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucProtocolBaseSize = ucSize;
    s_ucProtocolBasePos = 0;

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
    if (s_pstProtocolBase) {
        for (int i = 0; i < s_ucProtocolBasePos; i++) {
        	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)&s_pstProtocolBase[i];
        	if (pstBase->stRx.pucPacket) {
        		free(pstBase->stRx.pucPacket);
        		pstBase->stRx.pucPacket = NULL;
        	}
        	pifRingBuffer_Exit(&pstBase->stTx.stRequestBuffer);
        	pifRingBuffer_Exit(&pstBase->stTx.stAnswerBuffer);
        	pifPulse_RemoveItem(s_pstProtocolTimer, pstBase->stTx.pstTimer);
        }
    	free(s_pstProtocolBase);
        s_pstProtocolBase = NULL;
    }
}

/**
 * @fn pifProtocol_Add
 * @brief
 * @param unDeviceCode
 * @param enType
 * @param pstQuestion
 * @return
 */
PIF_stProtocol *pifProtocol_Add(PIF_unDeviceCode unDeviceCode, PIF_enProtocolType enType,
		const PIF_stProtocolQuestion *pstQuestions)
{
    if (s_ucProtocolBasePos >= s_ucProtocolBaseSize) {
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

    PIF_stProtocolBase *pstBase = &s_pstProtocolBase[s_ucProtocolBasePos];

    switch (enType) {
	case PT_enSmall:
		pstBase->ucHeaderSize = 4;
		break;

	case PT_enMedium:
		pstBase->ucHeaderSize = 6;
		break;

	default:
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    PIF_stProtocol *pstOwner = (PIF_stProtocol *)pstBase;

    pstBase->stRx.pucPacket = calloc(sizeof(uint8_t), 10 + PIF_PROTOCOL_RX_PACKET_SIZE);
    if (!pstBase->stRx.pucPacket) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    pstBase->stRx.pstTimer = pifPulse_AddItem(s_pstProtocolTimer, PT_enOnce);
    if (!pstBase->stRx.pstTimer) goto fail;
    pifPulse_AttachEvtFinish(pstBase->stRx.pstTimer, _evtTimerRxTimeout, pstOwner);
#endif

    if (!pifRingBuffer_InitAlloc(&pstBase->stTx.stRequestBuffer, PIF_PROTOCOL_TX_REQUEST_SIZE)) goto fail;
    pifRingBuffer_SetDeviceCode(&pstBase->stTx.stRequestBuffer, unDeviceCode);
    pifRingBuffer_SetName(&pstBase->stTx.stRequestBuffer, "RQB");

    if (!pifRingBuffer_InitAlloc(&pstBase->stTx.stAnswerBuffer, PIF_PROTOCOL_TX_RESPONSE_SIZE)) goto fail;
    pifRingBuffer_SetDeviceCode(&pstBase->stTx.stAnswerBuffer, unDeviceCode);
    pifRingBuffer_SetName(&pstBase->stTx.stAnswerBuffer, "RSB");

    pstBase->stTx.pstTimer = pifPulse_AddItem(s_pstProtocolTimer, PT_enOnce);
    if (!pstBase->stTx.pstTimer) goto fail;
    pifPulse_AttachEvtFinish(pstBase->stTx.pstTimer, _evtTimerTxTimeout, pstOwner);

    pstOwner->enType = enType;
    pstBase->pstQuestions = pstQuestions;
    pstOwner->unDeviceCode = unDeviceCode;
    pstBase->ucPacketId = 0x20;
    pstOwner->ucOwnerId = 0xFF;
    pstBase->stRx.usPacketSize = 10 + PIF_PROTOCOL_RX_PACKET_SIZE;

    s_ucProtocolBasePos = s_ucProtocolBasePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) Add(T:%d) EC:%d", unDeviceCode, enType, pif_enError);
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
    PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pstOwner;

    if (!usRxPacketSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstBase->stRx.pucPacket = realloc(pstBase->stRx.pucPacket, sizeof(uint8_t) * (10 + usRxPacketSize));
    if (!pstBase->stRx.pucPacket) {
        pif_enError = E_enOutOfHeap;
    	goto fail;
    }

    pstBase->stRx.usPacketSize = 10 + usRxPacketSize;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeRxPacket(S:%u) EC:%d", pstOwner->unDeviceCode, usRxPacketSize, pif_enError);
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
    PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pstOwner;

    if (!usTxRequestSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	pifRingBuffer_Exit(&pstBase->stTx.stRequestBuffer);
    return pifRingBuffer_InitAlloc(&pstBase->stTx.stRequestBuffer, usTxRequestSize);

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeTxRequest(S:%u) EC:%d", pstOwner->unDeviceCode, usTxRequestSize, pif_enError);
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
    PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pstOwner;

    if (usTxResponseSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	pifRingBuffer_Exit(&pstBase->stTx.stAnswerBuffer);
    return pifRingBuffer_InitAlloc(&pstBase->stTx.stAnswerBuffer, usTxResponseSize);

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) ResizeTxResponset(S:%u) EC:%d", pstOwner->unDeviceCode, usTxResponseSize, pif_enError);
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
	pifComm_AttachClient(pstComm, pstOwner);
	pifComm_AttachEvtParsing(pstComm, _evtParsing);
	pifComm_AttachEvtSending(pstComm, _evtSending);
}

/**
 * @fn pifProtocol_SetOwnerId
 * @brief
 * @param pvOwner
 * @param ucOwnerId
 * @return
 */
BOOL pifProtocol_SetOwnerId(PIF_stProtocol *pstOwner, uint8_t ucOwnerId)
{
	if (ucOwnerId < 0x20) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}
	pstOwner->ucOwnerId = ucOwnerId;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "PTC(%u) SetOwnerId(I:%u) EC:%d", pstOwner->unDeviceCode, ucOwnerId, pif_enError);
#endif
	return FALSE;
}

static BOOL _MakeRequest(PIF_stProtocolBase *pstBase, uint16_t usDataSize)
{
	uint16_t usLength;
	uint8_t aucHeader[13];
	uint8_t aucTailer[2];
	uint8_t ucPacketId = 0, ucData;
	const PIF_stProtocolRequest *pstTable = pstBase->stTx.pstRequest;

	pifRingBuffer_BackupHead(&pstBase->stTx.stAnswerBuffer);

	uint16_t usCount = 0;
	for (uint16_t i = 0; i < usDataSize; i++) {
		if (pstBase->stTx.pucData[i] < 0x20) usCount++;
	}

	pifCrc7_Init();

	aucHeader[5] = ASCII_STX;
	aucHeader[6] = PF_enAlways | pstTable->enFlags;
	aucHeader[7] = pstTable->ucCommand;
	switch (pstBase->stOwner.enType) {
	case PT_enSmall:
		aucHeader[8] = 0x20 + usDataSize;
		break;

	case PT_enMedium:
		ucPacketId = pstBase->ucPacketId;
		aucHeader[8] = ucPacketId;
		pstBase->ucPacketId++;
		if (!pstBase->ucPacketId) pstBase->ucPacketId = 0x20;
		aucHeader[9] = 0x80 | (usDataSize & 0x7F);
		aucHeader[10] = 0x80 | ((usDataSize >> 7) & 0x7F);
		break;
	}
	usLength = pstBase->ucHeaderSize + usDataSize + usCount + 2;
	aucHeader[0] = usLength & 0xFF;
	aucHeader[1] = (usLength >> 8) & 0xFF;
	aucHeader[2] = pstTable->usTimeout & 0xFF;
	aucHeader[3] = (pstTable->usTimeout >> 8) & 0xFF;
	aucHeader[4] = pstTable->ucRetry;
	if (!pifRingBuffer_PutData(&pstBase->stTx.stRequestBuffer, aucHeader, 5 + pstBase->ucHeaderSize)) goto fail;
	for (int i = 1; i < pstBase->ucHeaderSize; i++) {
		pifCrc7_Calcurate(aucHeader[5 + i]);
	}

	for (uint16_t i = 0; i < usDataSize; i++) {
		ucData = pstBase->stTx.pucData[i];
		if (ucData < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(&pstBase->stTx.stRequestBuffer, ASCII_DLE)) goto fail;
			ucData |= 0x80;
		}
		pifCrc7_Calcurate(ucData);
		if (!pifRingBuffer_PutByte(&pstBase->stTx.stRequestBuffer, ucData)) goto fail;
	}

	aucTailer[0] = 0x80 | pifCrc7_Result();
	aucTailer[1] = ASCII_ETX;
	if (!pifRingBuffer_PutData(&pstBase->stTx.stRequestBuffer, aucTailer, 2)) goto fail;

#ifndef __PIF_NO_LOG__
	if (pstTable->enFlags & PF_enLogPrint_Mask) {
		pifLog_Printf(LT_enComm, "PTC(%u) Rq:%xh F:%xh P:%d L:%d CRC:%xh", pstBase->stOwner.unDeviceCode, pstTable->ucCommand,
				pstTable->enFlags, ucPacketId, usDataSize, aucTailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%u< %x %x %x %x %x %x %x %x", pstBase->stOwner.unDeviceCode,
			aucHeader[5], aucHeader[6], aucHeader[7], aucHeader[8], aucHeader[9], aucHeader[10], aucTailer[0], aucTailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(&pstBase->stTx.stAnswerBuffer);
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
	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pstOwner;

	pstBase->stTx.pstRequest = pstRequest;
	pstBase->stTx.pucData = pucData;
	pstBase->stTx.usDataSize = usDataSize;
	return _MakeRequest(pstBase, usDataSize);
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
	PIF_stProtocolBase *pstBase = (PIF_stProtocolBase *)pstOwner;
	uint8_t aucHeader[8];
	uint8_t aucTailer[2];
	uint8_t ucPacketId = 0, ucData;

	pifRingBuffer_BackupHead(&pstBase->stTx.stAnswerBuffer);

	pifCrc7_Init();

	aucHeader[0] = ASCII_STX;
	aucHeader[1] = PF_enAlways | PF_enType_Answer | enFlags;
	aucHeader[2] = pstQuestion->ucCommand;
	switch (pstOwner->enType) {
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
	if (!pifRingBuffer_PutData(&pstBase->stTx.stAnswerBuffer, aucHeader, pstBase->ucHeaderSize)) goto fail;
	for (int i = 1; i < pstBase->ucHeaderSize; i++) {
		pifCrc7_Calcurate(aucHeader[i]);
	}

	for (uint16_t i = 0; i < usDataSize; i++) {
		ucData = pucData[i];
		if (ucData < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(&pstBase->stTx.stAnswerBuffer, ASCII_DLE)) goto fail;
			ucData |= 0x80;
		}
		pifCrc7_Calcurate(ucData);
		if (!pifRingBuffer_PutByte(&pstBase->stTx.stAnswerBuffer, ucData)) goto fail;
	}

	aucTailer[0] = 0x80 | pifCrc7_Result();
	aucTailer[1] = ASCII_ETX;
	if (!pifRingBuffer_PutData(&pstBase->stTx.stAnswerBuffer, aucTailer, 2)) goto fail;

#ifndef __PIF_NO_LOG__
	if (enFlags & PF_enLogPrint_Mask) {
		pifLog_Printf(LT_enComm, "PTC(%u) As:%xh F:%xh P:%d L:%d CRC:%xh", pstOwner->unDeviceCode, pstQuestion->ucCommand,
				enFlags, ucPacketId, usDataSize, aucTailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_enNone, "\n%u< %x %x %x %x %x %x %x %x", pstOwner->unDeviceCode,
			aucHeader[0], aucHeader[1], aucHeader[2], aucHeader[3], aucHeader[4], aucHeader[5],	aucTailer[0], aucTailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(&pstBase->stTx.stAnswerBuffer);
	if (!pif_enError) pif_enError = E_enOverflowBuffer;
	return FALSE;
}
