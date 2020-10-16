/*
 * pifRingData.c
 *
 *  Created on: 2020. 8. 16.
 *      Author: wonjh
 */

#include "pifLog.h"
#include "pifRingData.h"


/**
 * @fn pifRingData_Init
 * @brief
 * @param usDataSize
 * @param usDataCount
 * @return
 */
PIF_stRingData *pifRingData_Init(uint16_t usDataSize, uint16_t usDataCount)
{
	PIF_stRingData *pstOwner;

	pstOwner = calloc(sizeof(PIF_stRingData) + usDataSize * usDataCount, 1);
	if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    pstOwner->usDataSize = usDataSize;
    pstOwner->usDataCount = usDataCount;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    pstOwner->__pvData = (void *)pstOwner + sizeof(PIF_stRingData);
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "RingData:Init(S:%u C:%u) EC:%d", usDataSize, usDataCount, pif_enError);
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
	return pstOwner->__usHead == pstOwner->__usTail;
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
	return pstOwner->__pvData + (usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetFirstData
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_GetFirstData(PIF_stRingData *pstOwner)
{
	if (pstOwner->__usHead == pstOwner->__usTail) return NULL;
	pstOwner->__usIndex = pstOwner->__usTail;
	return pstOwner->__pvData + (pstOwner->__usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetNextData
 * @brief
 * @param pstOwner
 * @return
 */
void *pifRingData_GetNextData(PIF_stRingData *pstOwner)
{
	pstOwner->__usIndex++;
	if (pstOwner->__usIndex >= pstOwner->usDataCount) pstOwner->__usIndex = 0;
	if (pstOwner->__usIndex == pstOwner->__usHead) return NULL;
	return pstOwner->__pvData + (pstOwner->__usIndex * pstOwner->usDataSize);
}

/**
 * @fn pifRingData_GetFillSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingData_GetFillSize(PIF_stRingData *pstOwner)
{
    if (pstOwner->__usHead > pstOwner->__usTail) {
    	return pstOwner->__usHead - pstOwner->__usTail;
    }
    else {
    	return pstOwner->usDataCount - pstOwner->__usTail + pstOwner->__usHead;
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
	uint16_t usRemain;

    if (pstOwner->__usHead < pstOwner->__usTail) {
    	usRemain = pstOwner->__usTail - pstOwner->__usHead;
    }
    else {
    	usRemain = pstOwner->usDataCount - pstOwner->__usHead + pstOwner->__usTail;
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
	uint16_t next =	pstOwner->__usHead + 1;

	if (next >= pstOwner->usDataCount) next = 0;
	if (next == pstOwner->__usTail) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

	void *pvData = pstOwner->__pvData + (pstOwner->__usHead * pstOwner->usDataSize);
	pstOwner->__usHead = next;
	return pvData;

fail:
	pifLog_Printf(LT_enError, "RingData:Add() EC:%d", pif_enError);
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
	if (pstOwner->__usHead == pstOwner->__usTail) {
		pif_enError = E_enEmptyInBuffer;
		goto fail;
	}

	void *pvData = pstOwner->__pvData + (pstOwner->__usTail * pstOwner->usDataSize);
	pstOwner->__usTail++;
	if (pstOwner->__usTail >= pstOwner->usDataCount) pstOwner->__usTail = 0;
	return pvData;

fail:
	pifLog_Printf(LT_enError, "RingData:Remove() EC:%d", pif_enError);
	return NULL;
}