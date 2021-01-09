#include <string.h>

#include "pifComm.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


typedef struct _PIF_stCommBase
{
	// Public Member Variable
	PIF_stComm stOwner;

	// Private Member Variable
    void *pvClient;

    PIF_stRingBuffer stTxBuffer;
    PIF_enCommTxState enState;

    PIF_stRingBuffer stRxBuffer;

	PIF_enTaskLoop enTaskLoop;

	// Public Action Function
    PIF_actCommSendData actSendData;

    // Private Member Function
    PIF_evtCommParsing evtParsing;
    PIF_evtCommSending evtSending;
    PIF_evtCommSended evtSended;
} PIF_stCommBase;


static PIF_stCommBase *s_pstCommBase = NULL;
static uint8_t s_ucCommBaseSize;
static uint8_t s_ucCommBasePos;


static void _LoopCommon(PIF_stCommBase *pstBase)
{
	uint8_t ucData;

	if (!pifRingBuffer_IsEmpty(&pstBase->stRxBuffer)) {
		if (pstBase->evtParsing) {
			(*pstBase->evtParsing)(pstBase->pvClient, &pstBase->stRxBuffer);
		}
	}

	if (pifRingBuffer_IsEmpty(&pstBase->stTxBuffer)) {
		switch (pstBase->enState) {
		case STS_enIdle:
			if (pstBase->evtSending) {
				if ((*pstBase->evtSending)(pstBase->pvClient, &pstBase->stTxBuffer)) {
					if (pstBase->actSendData) {
						pifRingBuffer_GetByte(&pstBase->stTxBuffer, &ucData);
						(*pstBase->actSendData)(ucData);
					}
					pstBase->enState = STS_enSending;
				}
			}
			break;

		case STS_enSending:
			if (pstBase->evtSended) {
				if (pifRingBuffer_IsEmpty(&pstBase->stTxBuffer)) {
					(*pstBase->evtSended)(pstBase->pvClient);
				}
			}
			pstBase->enState = STS_enIdle;
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

    s_pstCommBase = calloc(sizeof(PIF_stCommBase), ucSize);
    if (!s_pstCommBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucCommBaseSize = ucSize;
    s_ucCommBasePos = 0;
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
    if (s_pstCommBase) {
    	for (int i = 0; i < s_ucCommBasePos; i++) {
    		PIF_stCommBase *pstBase = &s_pstCommBase[i];
        	pifRingBuffer_Exit(&pstBase->stRxBuffer);
        	pifRingBuffer_Exit(&pstBase->stTxBuffer);
    	}
        free(s_pstCommBase);
        s_pstCommBase = NULL;
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
    if (s_ucCommBasePos >= s_ucCommBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stCommBase *pstBase = &s_pstCommBase[s_ucCommBasePos];

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;

    if (!pifRingBuffer_InitAlloc(&pstBase->stRxBuffer, PIF_COMM_RX_BUFFER_SIZE)) goto fail;
    pifRingBuffer_SetPifId(&pstBase->stRxBuffer, usPifId);
    pifRingBuffer_SetName(&pstBase->stRxBuffer, "RB");

    if (!pifRingBuffer_InitAlloc(&pstBase->stTxBuffer, PIF_COMM_TX_BUFFER_SIZE)) goto fail;
    pifRingBuffer_SetPifId(&pstBase->stTxBuffer, usPifId);
    pifRingBuffer_SetName(&pstBase->stTxBuffer, "TB");

    pstBase->stOwner.usPifId = usPifId;
    pstBase->enState = STS_enIdle;

    s_ucCommBasePos = s_ucCommBasePos + 1;
    return &pstBase->stOwner;

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
	PIF_stCommBase *pstBase = (PIF_stCommBase *)pstOwner;

    if (!usRxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	pifRingBuffer_Exit(&pstBase->stRxBuffer);
    if (!pifRingBuffer_InitAlloc(&pstBase->stRxBuffer, usRxSize)) goto fail;
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
	PIF_stCommBase *pstBase = (PIF_stCommBase *)pstOwner;

	if (!usTxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	pifRingBuffer_Exit(&pstBase->stTxBuffer);
    if (!pifRingBuffer_InitAlloc(&pstBase->stTxBuffer, usTxSize)) goto fail;
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
	((PIF_stCommBase *)pstOwner)->pvClient = pvClient;
}

/**
 * @fn pifComm_AttachAction
 * @brief
 * @param pstOwner
 * @param actSendData
 */
void pifComm_AttachAction(PIF_stComm *pstOwner, PIF_actCommSendData actSendData)
{
	((PIF_stCommBase *)pstOwner)->actSendData = actSendData;
}

/**
 * @fn pifComm_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtParsing
 * @param evtSending
 * @param fnSended
 */
void pifComm_AttachEvent(PIF_stComm *pstOwner, PIF_evtCommParsing evtParsing, PIF_evtCommSending evtSending, PIF_evtCommSended evtSended)
{
	PIF_stCommBase *pstBase = (PIF_stCommBase *)pstOwner;

	pstBase->evtParsing = evtParsing;
	pstBase->evtSending = evtSending;
	pstBase->evtSended = evtSended;
}

/**
 * @fn pifComm_GetRemainSizeOfRxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetRemainSizeOfRxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetRemainSize(&((PIF_stCommBase *)pstOwner)->stRxBuffer);
}

/**
 * @fn pifComm_GetFillSizeOfTxBuffer
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifComm_GetFillSizeOfTxBuffer(PIF_stComm *pstOwner)
{
	return pifRingBuffer_GetFillSize(&((PIF_stCommBase *)pstOwner)->stTxBuffer);
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
	return pifRingBuffer_PutByte(&((PIF_stCommBase *)pstOwner)->stRxBuffer, ucData);
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
	return pifRingBuffer_PutData(&((PIF_stCommBase *)pstOwner)->stRxBuffer, pucData, usLength);
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
    return pifRingBuffer_GetByte(&((PIF_stCommBase *)pstOwner)->stTxBuffer, pucData);
}

/**
 * @fn pifComm_taskAll
 * @brief
 * @param pstTask
 */
void pifComm_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucCommBasePos; i++) {
		PIF_stCommBase *pstBase = &s_pstCommBase[i];
		if (!pstBase->enTaskLoop) _LoopCommon(pstBase);
	}
}

/**
 * @fn pifComm_taskEach
 * @brief
 * @param pstTask
 */
void pifComm_taskEach(PIF_stTask *pstTask)
{
	PIF_stCommBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstBase);
	}
}
