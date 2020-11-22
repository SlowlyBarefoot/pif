#include <string.h>

#include "pifLog.h"


static PIF_stComm *s_pstCommArray;
static uint8_t s_ucCommArraySize;
static uint8_t s_ucCommArrayPos;


static void _LoopCommon(PIF_stComm *pstOwner)
{
	uint8_t ucData;

	if (!pifRingBuffer_IsEmpty(&pstOwner->__stRxBuffer)) {
		if (pstOwner->__fnParing) {
			(*pstOwner->__fnParing)(pstOwner->__pvProcessor, &pstOwner->__stRxBuffer);
		}
	}

	if (pifRingBuffer_IsEmpty(&pstOwner->__stTxBuffer)) {
		switch (pstOwner->__enState) {
		case STS_enIdle:
			if (pstOwner->__fnSending) {
				if ((*pstOwner->__fnSending)(pstOwner->__pvProcessor, &pstOwner->__stTxBuffer)) {
					if (pstOwner->actSendData) {
						pifRingBuffer_Pop(&pstOwner->__stTxBuffer, &ucData);
						(*pstOwner->actSendData)(ucData);
					}
					pstOwner->__enState = STS_enSending;
				}
			}
			break;

		case STS_enSending:
			if (pstOwner->__fnSended) {
				if (pifRingBuffer_IsEmpty(&pstOwner->__stTxBuffer)) {
					(*pstOwner->__fnSended)(pstOwner->__pvProcessor);
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

    s_pstCommArray = calloc(sizeof(PIF_stComm), ucSize);
    if (!s_pstCommArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucCommArraySize = ucSize;
    s_ucCommArrayPos = 0;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Comm:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifComm_Exit
 * @brief 
 */
void pifComm_Exit()
{
    if (s_pstCommArray) {
        free(s_pstCommArray);
        s_pstCommArray = NULL;
    }
}

/**
 * @fn pifComm_Add
 * @brief 
 * @param unDeviceCode
 * @return 
 */
PIF_stComm *pifComm_Add(PIF_unDeviceCode unDeviceCode)
{
    if (s_ucCommArrayPos >= s_ucCommArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stComm *pstOwner = &s_pstCommArray[s_ucCommArrayPos];

    if (!pifRingBuffer_InitAlloc(&pstOwner->__stRxBuffer, PIF_COMM_RX_BUFFER_SIZE)) goto fail;
    if (!pifRingBuffer_InitAlloc(&pstOwner->__stTxBuffer, PIF_COMM_TX_BUFFER_SIZE)) goto fail;

    pstOwner->unDeviceCode = unDeviceCode;
	pstOwner->__enState = STS_enIdle;

    s_ucCommArrayPos = s_ucCommArrayPos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "Comm:Add(D:%u) EC:%d", unDeviceCode, pif_enError);
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

	pifRingBuffer_Exit(&pstOwner->__stRxBuffer);
    if (!pifRingBuffer_InitAlloc(&pstOwner->__stRxBuffer, usRxSize)) goto fail;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Comm:ResizeRxBuffer(S:%u) EC:%d", usRxSize, pif_enError);
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

	pifRingBuffer_Exit(&pstOwner->__stTxBuffer);
    if (!pifRingBuffer_InitAlloc(&pstOwner->__stTxBuffer, usTxSize)) goto fail;
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "Comm:ResizeTxBuffer(S:%u) EC:%d", usTxSize, pif_enError);
	return FALSE;
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
	return pifRingBuffer_PushByte(&pstOwner->__stRxBuffer, ucData);
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
	return pifRingBuffer_PushData(&pstOwner->__stRxBuffer, pucData, usLength);
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
    return pifRingBuffer_Pop(&pstOwner->__stTxBuffer, pucData);
}

/**
 * @fn pifComm_taskAll
 * @brief
 * @param pstTask
 */
void pifComm_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucCommArrayPos; i++) {
		PIF_stComm *pstOwner = &s_pstCommArray[i];
		if (!pstOwner->__enTaskLoop) _LoopCommon(pstOwner);
	}
}

/**
 * @fn pifComm_taskEach
 * @brief
 * @param pstTask
 */
void pifComm_taskEach(PIF_stTask *pstTask)
{
	PIF_stComm *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstOwner);
	}
}
