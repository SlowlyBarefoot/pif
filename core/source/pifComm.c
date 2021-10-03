#include "pifComm.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static BOOL _actReceiveData(PIF_stComm *pstOwner, uint8_t *pucData)
{
	return pifRingBuffer_GetByte(pstOwner->_pstRxBuffer, pucData);
}

static uint16_t _actSendData(PIF_stComm *pstOwner, uint8_t *pucBuffer, uint16_t usSize)
{
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner->_pstTxBuffer);

	if (!usRemain) return 0;
	if (usSize > usRemain) usSize = usRemain;
	if (pifRingBuffer_PutData(pstOwner->_pstTxBuffer, pucBuffer, usSize)) {
		return usSize;
	}
	return 0;
}

static void _sendData(PIF_stComm *pstOwner)
{
	if (pstOwner->__actSendData) {
		(*pstOwner->evtSending)(pstOwner->__pvClient, pstOwner->__actSendData);
	}
	else if (pstOwner->_pstTxBuffer) {
		if ((*pstOwner->evtSending)(pstOwner->__pvClient, _actSendData)) {
			if (pstOwner->__enState == CTS_enIdle) {
				pstOwner->__enState = CTS_enSending;
				if (pstOwner->__actStartTransfer) {
					if (!(*pstOwner->__actStartTransfer)()) pstOwner->__enState = CTS_enIdle;
				}
			}
		}
	}
}

/**
 * @fn pifComm_Create
 * @brief 
 * @param usPifId
 * @return 
 */
PIF_stComm *pifComm_Create(PIF_usId usPifId)
{
    PIF_stComm *pstOwner = NULL;

    pstOwner = calloc(sizeof(PIF_stComm), 1);
    if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;

    pstOwner->_usPifId = usPifId;
    pstOwner->__enState = CTS_enIdle;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:Init(ID:%u) EC:%d", usPifId, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifComm_Destroy
 * @brief 
 * @param pp_owner
 */
void pifComm_Destroy(PIF_stComm** pp_owner)
{
	if (*pp_owner) {
		PIF_stComm* pstOwner = *pp_owner;
		if (pstOwner->_pstRxBuffer)	pifRingBuffer_Exit(pstOwner->_pstRxBuffer);
		if (pstOwner->_pstTxBuffer)	pifRingBuffer_Exit(pstOwner->_pstTxBuffer);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifComm_AllocRxBuffer
 * @brief
 * @param pstOwner
 * @param usRxSize
 * @return
 */
BOOL pifComm_AllocRxBuffer(PIF_stComm *pstOwner, uint16_t usRxSize)
{
    if (!usRxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstOwner->_pstRxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, usRxSize);
    if (!pstOwner->_pstRxBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->_pstRxBuffer, "RB");
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:ResizeRxBuffer(S:%u) EC:%d", usRxSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifComm_AllocTxBuffer
 * @brief
 * @param pstOwner
 * @param usTxSize
 * @return
 */
BOOL pifComm_AllocTxBuffer(PIF_stComm *pstOwner, uint16_t usTxSize)
{
	if (!usTxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstOwner->_pstTxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, usTxSize);
    if (!pstOwner->_pstTxBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->_pstTxBuffer, "TB");
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:ResizeTxBuffer(S:%u) EC:%d", usTxSize, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifComm_AttachClient
 * @brief
 * @param pstOwner
 * @param pvClient
 */
void pifComm_AttachClient(PIF_stComm *pstOwner, void *pvClient)
{
	pstOwner->__pvClient = pvClient;
}

/**
 * @fn pifComm_AttachActReceiveData
 * @brief
 * @param pstOwner
 * @param actReceiveData
 */
void pifComm_AttachActReceiveData(PIF_stComm *pstOwner, PIF_actCommReceiveData actReceiveData)
{
	pstOwner->__actReceiveData = actReceiveData;
}

/**
 * @fn pifComm_AttachActSendData
 * @brief
 * @param pstOwner
 * @param actReceiveData
 */
void pifComm_AttachActSendData(PIF_stComm *pstOwner, PIF_actCommSendData actSendData)
{
	pstOwner->__actSendData = actSendData;
}

/**
 * @fn pifComm_AttachActStartTransfer
 * @brief
 * @param pstOwner
 * @param actStartTransfer
 */
void pifComm_AttachActStartTransfer(PIF_stComm *pstOwner, PIF_actCommStartTransfer actStartTransfer)
{
	pstOwner->__actStartTransfer = actStartTransfer;
}

/**
 * @fn pifComm_GetRemainSizeOfRxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetRemainSizeOfRxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetRemainSize(pstOwner->_pstRxBuffer);
}

/**
 * @fn pifComm_GetFillSizeOfTxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetFillSizeOfTxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetFillSize(pstOwner->_pstTxBuffer);
}

/**
 * @fn pifComm_ReceiveData
 * @brief
 * @param pstOwner
 * @param ucData
 * @return 
 */
BOOL pifComm_ReceiveData(PIF_stComm *pstOwner, uint8_t ucData)
{
	if (!pstOwner->_pstRxBuffer) return FALSE;

	return pifRingBuffer_PutByte(pstOwner->_pstRxBuffer, ucData);
}

/**
 * @fn pifComm_ReceiveDatas
 * @brief
 * @param pstOwner
 * @param pucData
 * @param usLength
 * @return
 */
BOOL pifComm_ReceiveDatas(PIF_stComm *pstOwner, uint8_t *pucData, uint16_t usLength)
{
	if (!pstOwner->_pstRxBuffer) return FALSE;

	return pifRingBuffer_PutData(pstOwner->_pstRxBuffer, pucData, usLength);
}

/**
 * @fn pifComm_SendData
 * @brief
 * @param pstOwner
 * @param pucData
 * @return
 */
uint8_t pifComm_SendData(PIF_stComm *pstOwner, uint8_t *pucData)
{
	uint8_t ucState = PIF_COMM_SEND_DATA_STATE_INIT;

    if (!pstOwner->_pstTxBuffer) return ucState;

    ucState = pifRingBuffer_GetByte(pstOwner->_pstTxBuffer, pucData);
	if (ucState) {
		if (pifRingBuffer_IsEmpty(pstOwner->_pstTxBuffer)) {
			ucState |= PIF_COMM_SEND_DATA_STATE_EMPTY;
		}
	}
	else ucState |= PIF_COMM_SEND_DATA_STATE_EMPTY;
	return ucState;
}

/**
 * @fn pifComm_StartSendDatas
 * @brief
 * @param pstOwner
 * @param pucData
 * @param pusLength
 * @return
 */
uint8_t pifComm_StartSendDatas(PIF_stComm *pstOwner, uint8_t **ppucData, uint16_t *pusLength)
{
	uint16_t usLength;

    if (!pstOwner->_pstTxBuffer) return PIF_COMM_SEND_DATA_STATE_INIT;
    if (pifRingBuffer_IsEmpty(pstOwner->_pstTxBuffer)) return PIF_COMM_SEND_DATA_STATE_EMPTY;

    *ppucData = pifRingBuffer_GetTailPointer(pstOwner->_pstTxBuffer, 0);
    usLength = pifRingBuffer_GetLinerSize(pstOwner->_pstTxBuffer, 0);
    if (!*pusLength || usLength <= *pusLength) *pusLength = usLength;
	return PIF_COMM_SEND_DATA_STATE_DATA;
}

/**
 * @fn pifComm_EndSendDatas
 * @brief
 * @param pstOwner
 * @param usLength
 * @return
 */
uint8_t pifComm_EndSendDatas(PIF_stComm *pstOwner, uint16_t usLength)
{
    pifRingBuffer_Remove(pstOwner->_pstTxBuffer, usLength);
	return pifRingBuffer_IsEmpty(pstOwner->_pstTxBuffer) << 1;
}

/**
 * @fn pifComm_FinishTransfer
 * @brief
 * @param pstOwner
 */
void pifComm_FinishTransfer(PIF_stComm *pstOwner)
{
	pstOwner->__enState = CTS_enIdle;
	pstOwner->_pstTask->bImmediate = TRUE;
}

/**
 * @fn pifComm_ForceSendData
 * @brief
 * @param pstOwner
 */
void pifComm_ForceSendData(PIF_stComm *pstOwner)
{
	if (pstOwner->evtSending) _sendData(pstOwner);
}

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	PIF_stComm *pstOwner = pstTask->_pvClient;

	pstOwner->_pstTask = pstTask;

	if (pstOwner->evtParsing) {
		if (pstOwner->__actReceiveData) {
			(*pstOwner->evtParsing)(pstOwner->__pvClient, pstOwner->__actReceiveData);
		}
		else if (pstOwner->_pstRxBuffer) {
			(*pstOwner->evtParsing)(pstOwner->__pvClient, _actReceiveData);
		}
	}

	if (pstOwner->evtSending) _sendData(pstOwner);
	return 0;
}

/**
 * @fn pifComm_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifComm_AttachTask(PIF_stComm *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}

