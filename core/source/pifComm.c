#include "pifComm.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stComm *s_pstComm = NULL;
static uint8_t s_ucCommSize;
static uint8_t s_ucCommPos;


static BOOL _actReceiveData(PIF_stComm *pstOwner, uint8_t *pucData)
{
	return pifRingBuffer_GetByte(pstOwner->_pstRxBuffer, pucData);
}

static uint16_t _actSendData(PIF_stComm *pstOwner, uint8_t *pucBuffer, uint16_t usSize)
{
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner->_pstTxBuffer);

	if (usSize > usRemain) usSize = usRemain;
	if (pifRingBuffer_PutData(pstOwner->_pstTxBuffer, pucBuffer, usSize)) {
		return usSize;
	}
	return 0;
}

static void _taskCommon(PIF_stComm *pstOwner)
{
	if (pstOwner->evtParsing) {
		if (pstOwner->__actReceiveData) {
			(*pstOwner->evtParsing)(pstOwner->__pvClient, pstOwner->__actReceiveData);
		}
		else if (pstOwner->_pstRxBuffer) {
			(*pstOwner->evtParsing)(pstOwner->__pvClient, _actReceiveData);
		}
	}

	switch (pstOwner->__enState) {
	case STS_enIdle:
		if (pstOwner->evtSending) {
			if (pstOwner->__actSendData) {
				(*pstOwner->evtSending)(pstOwner->__pvClient, pstOwner->__actSendData);
			}
			else if (pstOwner->_pstTxBuffer && pstOwner->__actStartTransfer) {
				if ((*pstOwner->evtSending)(pstOwner->__pvClient, _actSendData)) {
					(*pstOwner->__actStartTransfer)();
					pstOwner->__enState = STS_enSending;
				}
			}
		}
		break;

	case STS_enSending:
		if (pifRingBuffer_IsEmpty(pstOwner->_pstTxBuffer)) pstOwner->__enState = STS_enIdle;
    	break;
	}
}

/**
 * @fn pifComm_Init
 * @brief 
 * @param ucSize
 * @return 
 */
BOOL pifComm_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstComm = calloc(sizeof(PIF_stComm), ucSize);
    if (!s_pstComm) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucCommSize = ucSize;
    s_ucCommPos = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifComm_Exit
 * @brief 
 */
void pifComm_Exit()
{
    if (s_pstComm) {
    	for (int i = 0; i < s_ucCommPos; i++) {
    		PIF_stComm *pstOwner = &s_pstComm[i];
    		if (pstOwner->_pstRxBuffer)	pifRingBuffer_Exit(pstOwner->_pstRxBuffer);
    		if (pstOwner->_pstTxBuffer)	pifRingBuffer_Exit(pstOwner->_pstTxBuffer);
    	}
        free(s_pstComm);
        s_pstComm = NULL;
    }
}

/**
 * @fn pifComm_Add
 * @brief 
 * @param usPifId
 * @return 
 */
PIF_stComm *pifComm_Add(PIF_usId usPifId)
{
    if (s_ucCommPos >= s_ucCommSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stComm *pstOwner = &s_pstComm[s_ucCommPos];

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;

    pstOwner->_usPifId = usPifId;
    pstOwner->__enState = STS_enIdle;

    s_ucCommPos = s_ucCommPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:Add(D:%u) EC:%d", usPifId, pif_enError);
#endif
    return NULL;
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
	if (pstOwner->_pstRxBuffer)	return pifRingBuffer_PutByte(pstOwner->_pstRxBuffer, ucData);
	return FALSE;
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
	if (pstOwner->_pstRxBuffer) return pifRingBuffer_PutData(pstOwner->_pstRxBuffer, pucData, usLength);
	return FALSE;
}

/**
 * @fn pifComm_SendData
 * @brief
 * @param pstOwner
 * @param pucData
 * @return
 */
BOOL pifComm_SendData(PIF_stComm *pstOwner, uint8_t *pucData)
{
    if (pstOwner->_pstTxBuffer) return pifRingBuffer_GetByte(pstOwner->_pstTxBuffer, pucData);
	return FALSE;
}

/**
 * @fn pifComm_taskAll
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifComm_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucCommPos; i++) {
		PIF_stComm *pstOwner = &s_pstComm[i];
		if (!pstOwner->__enTaskLoop) _taskCommon(pstOwner);
	}
	return 0;
}

/**
 * @fn pifComm_taskEach
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifComm_taskEach(PIF_stTask *pstTask)
{
	PIF_stComm *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_taskCommon(pstOwner);
	}
	return 0;
}
