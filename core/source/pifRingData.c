#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifRingData.h"


typedef struct _PIF_stRingDataBase
{
	// Public Member Variable
	PIF_stRingData stOwner;

	// Private Member Variable
    uint16_t usHead;
    uint16_t usTail;
    uint16_t usIndex;
    void *pvData;
} PIF_stRingDataBase;


/**
 * @fn pifRingData_Init
 * @brief
 * @param usPifId
 * @param usDataSize
 * @param usDataCount
 * @return
 */
PIF_stRingData *pifRingData_Init(PIF_usId usPifId, uint16_t usDataSize, uint16_t usDataCount)
{
	PIF_stRingDataBase *pstBase;
	PIF_stRingData *pstOwner;

	pstBase = calloc(sizeof(PIF_stRingDataBase) + usDataSize * usDataCount, 1);
	if (!pstBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstOwner = &pstBase->stOwner;
	if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->usPifId = usPifId;
    pstOwner->usDataSize = usDataSize;
    pstOwner->usDataCount = usDataCount;
    pstBase->usHead = 0;
    pstBase->usTail = 0;
    pstBase->pvData = (void *)pstOwner + sizeof(PIF_stRingData);
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "RingData:Init(S:%u C:%u) EC:%d", usDataSize, usDataCount, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifRingData_Exit
 * @brief
 * @param pstOwner
 */
void pifRingData_Exit(PIF_stRingData *pstOwner)
{
	if (pstOwner) {
        free(pstOwner);
        pstOwner = NULL;
    }
}

/**
 * @fn pifRingData_IsEmpty
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingData_IsEmpty(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;

	return pstBase->usHead == pstBase->usTail;
}

/**
 * @fn pifRingData_GetData
 * @brief
 * @param pstOwner
 * @param usIndex
 * @return
 */
void *pifRingData_GetData(PIF_stRingData *pstOwner, uint16_t usIndex)
{
	return ((PIF_stRingDataBase *)pstOwner)->pvData + (usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetFirstData
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_GetFirstData(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;

	if (pstBase->usHead == pstBase->usTail) return NULL;
	pstBase->usIndex = pstBase->usTail;
	return pstBase->pvData + (pstBase->usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetNextData
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_GetNextData(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;

	pstBase->usIndex++;
	if (pstBase->usIndex >= pstOwner->usDataCount) pstBase->usIndex = 0;
	if (pstBase->usIndex == pstBase->usHead) return NULL;
	return pstBase->pvData + (pstBase->usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetFillSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingData_GetFillSize(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;

	if (pstBase->usHead > pstBase->usTail) {
    	return pstBase->usHead - pstBase->usTail;
    }
    else {
    	return pstOwner->usDataCount - pstBase->usTail + pstBase->usHead;
    }
}

/**
 * @fn pifRingData_GetRemainSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingData_GetRemainSize(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;
	uint16_t usRemain;

    if (pstBase->usHead < pstBase->usTail) {
    	usRemain = pstBase->usTail - pstBase->usHead;
    }
    else {
    	usRemain = pstOwner->usDataCount - pstBase->usHead + pstBase->usTail;
    }
    return usRemain - 1;
}

/**
 * @fn pifRingData_Add
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_Add(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;
	uint16_t next =	pstBase->usHead + 1;

	if (next >= pstOwner->usDataCount) next = 0;
	if (next == pstBase->usTail) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

	void *pvData = pstBase->pvData + (pstBase->usHead * pstOwner->usDataSize);
	pstBase->usHead = next;
	return pvData;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "RingData:Add() EC:%d", pif_enError);
#endif
	return NULL;
}

/**
 * @fn pifRingData_Remove
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_Remove(PIF_stRingData *pstOwner)
{
	PIF_stRingDataBase *pstBase = (PIF_stRingDataBase *)pstOwner;

	if (pstBase->usHead == pstBase->usTail) {
		pif_enError = E_enEmptyInBuffer;
		goto fail;
	}

	void *pvData = pstBase->pvData + (pstBase->usTail * pstOwner->usDataSize);
	pstBase->usTail++;
	if (pstBase->usTail >= pstOwner->usDataCount) pstBase->usTail = 0;
	return pvData;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "RingData:Remove() EC:%d", pif_enError);
#endif
	return NULL;
}
