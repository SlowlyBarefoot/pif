#include <string.h>

#include "pifComm.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stComm *s_pstComm = NULL;
static uint8_t s_ucCommSize;
static uint8_t s_ucCommPos;


static void _taskCommon(PIF_stComm *pstOwner)
{
	uint8_t ucData;

	if (!pifRingBuffer_IsEmpty(pstOwner->__pstRxBuffer)) {
		if (pstOwner->evtParsing) {
			(*pstOwner->evtParsing)(pstOwner->__pvClient, pstOwner->__pstRxBuffer);
		}
	}

	if (pifRingBuffer_IsEmpty(pstOwner->__pstTxBuffer)) {
		switch (pstOwner->__enState) {
		case STS_enIdle:
			if (pstOwner->evtSending) {
				if ((*pstOwner->evtSending)(pstOwner->__pvClient, pstOwner->__pstTxBuffer)) {
					if (pstOwner->__actSendData) {
						pifRingBuffer_GetByte(pstOwner->__pstTxBuffer, &ucData);
						(*pstOwner->__actSendData)(ucData);
					}
					pstOwner->__enState = STS_enSending;
				}
			}
			break;

		case STS_enSending:
			if (pstOwner->evtSended) {
				if (pifRingBuffer_IsEmpty(pstOwner->__pstTxBuffer)) {
					(*pstOwner->evtSended)(pstOwner->__pvClient);
				}
			}
			pstOwner->__enState = STS_enIdle;
	    	break;
		}
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
        	pifRingBuffer_Exit(pstOwner->__pstRxBuffer);
        	pifRingBuffer_Exit(pstOwner->__pstTxBuffer);
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

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;

    pstOwner->__pstRxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_COMM_RX_BUFFER_SIZE);
    if (!pstOwner->__pstRxBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->__pstRxBuffer, "RB");

    pstOwner->__pstTxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_COMM_TX_BUFFER_SIZE);
    if (!pstOwner->__pstTxBuffer) goto fail;
    pifRingBuffer_SetName(pstOwner->__pstTxBuffer, "TB");

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
 * @fn pifComm_ResizeRxBuffer
 * @brief
 * @param pstOwner
 * @param usRxSize
 * @return
 */
BOOL pifComm_ResizeRxBuffer(PIF_stComm *pstOwner, uint16_t usRxSize)
{
    if (!usRxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (!pifRingBuffer_ResizeHeap(pstOwner->__pstRxBuffer, usRxSize)) goto fail;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Comm:ResizeRxBuffer(S:%u) EC:%d", usRxSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifComm_ResizeTxBuffer
 * @brief
 * @param pstOwner
 * @param usTxSize
 * @return
 */
BOOL pifComm_ResizeTxBuffer(PIF_stComm *pstOwner, uint16_t usTxSize)
{
	if (!usTxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (!pifRingBuffer_ResizeHeap(pstOwner->__pstTxBuffer, usTxSize)) goto fail;
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
 * @fn pifComm_AttachAction
 * @brief
 * @param pstOwner
 * @param actSendData
 */
void pifComm_AttachAction(PIF_stComm *pstOwner, PIF_actCommSendData actSendData)
{
	pstOwner->__actSendData = actSendData;
}

/**
 * @fn pifComm_GetRemainSizeOfRxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetRemainSizeOfRxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetRemainSize(pstOwner->__pstRxBuffer);
}

/**
 * @fn pifComm_GetFillSizeOfTxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetFillSizeOfTxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetFillSize(pstOwner->__pstTxBuffer);
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
	return pifRingBuffer_PutByte(pstOwner->__pstRxBuffer, ucData);
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
	return pifRingBuffer_PutData(pstOwner->__pstRxBuffer, pucData, usLength);
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
    return pifRingBuffer_GetByte(pstOwner->__pstTxBuffer, pucData);
}

/**
 * @fn pifComm_taskAll
 * @brief
 * @param pstTask
 */
void pifComm_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucCommPos; i++) {
		PIF_stComm *pstOwner = &s_pstComm[i];
		if (!pstOwner->__enTaskLoop) _taskCommon(pstOwner);
	}
}

/**
 * @fn pifComm_taskEach
 * @brief
 * @param pstTask
 */
void pifComm_taskEach(PIF_stTask *pstTask)
{
	PIF_stComm *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_taskCommon(pstOwner);
	}
}
