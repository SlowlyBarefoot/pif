#include <string.h>

#include "pifLog.h"
#include "pifRingBuffer.h"


static BOOL _ChopOff(PIF_stRingBuffer *pstOwner, uint16_t usAlloc)
{
	uint16_t usLength = pifRingBuffer_GetFillSize(pstOwner);
	uint16_t usSize, usTail;

	switch (pstOwner->btChopOff) {
	case RB_CHOP_OFF_CHAR:
		usSize = 0;
		usTail = pstOwner->__usTail;
		while (usTail != pstOwner->__usHead) {
			if (pstOwner->__pcBuffer[usTail] == pstOwner->__cChopOffChar) {
				if (usSize > usAlloc) {
					pstOwner->__usTail = usTail;
					return TRUE;
				}
			}
			usTail++;
			if (usTail >= pstOwner->usSize) usTail -= pstOwner->usSize;
			usSize++;
		}
		break;

	case RB_CHOP_OFF_LENGTH:
		usSize = pstOwner->__usChopOffLength;
		while (usAlloc > usSize) {
			usSize += pstOwner->__usChopOffLength;
		}
		if (usSize < usLength) {
			pstOwner->__usTail += usSize;
			if (pstOwner->__usTail >= pstOwner->usSize) pstOwner->__usTail -= pstOwner->usSize;
			return TRUE;
		}
		else if (usAlloc <= usLength) {
			pstOwner->__usTail = pstOwner->__usHead;
			return TRUE;
		}
		break;
	}
	return FALSE;
}

/**
 * @fn pifRingBuffer_InitAlloc
 * @brief 
 * @param pstOwner
 */
void pifRingBuffer_Init(PIF_stRingBuffer *pstOwner)
{
	pstOwner->unDeviceCode = 0;
	pstOwner->psName = NULL;
	pstOwner->__pcBuffer = NULL;
	pstOwner->__usHead = 0;
	pstOwner->__usTail = 0;
}

/**
 * @fn pifRingBuffer_InitAlloc
 * @brief
 * @param pstOwner
 * @param usSize
 * @return 
 */
BOOL pifRingBuffer_InitAlloc(PIF_stRingBuffer *pstOwner, uint16_t usSize)
{
	if (usSize) {
		pstOwner->__pcBuffer = calloc(sizeof(uint8_t), usSize);
		if (!pstOwner->__pcBuffer) {
			pif_enError = E_enOutOfHeap;
			goto fail;
		}
	}
	else {
		pstOwner->__pcBuffer = NULL;
	}

	pstOwner->unDeviceCode = 0;
	pstOwner->psName = NULL;
    pstOwner->btShare = FALSE;
    pstOwner->usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "RingBuffer:Init(S:%u) EC:%d", usSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifRingBuffer_InitShare
 * @brief
 * @param pstOwner
 * @param usSize
 * @param pcBuffer
 */
void pifRingBuffer_InitShare(PIF_stRingBuffer *pstOwner, uint16_t usSize, char *pcBuffer)
{
	pstOwner->__pcBuffer = pcBuffer;

	pstOwner->unDeviceCode = 0;
	pstOwner->psName = NULL;
    pstOwner->btShare = TRUE;
    pstOwner->usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
}

/**
 * @fn pifRingBuffer_Exit
 * @brief 
 * @param pstOwner
 */
void pifRingBuffer_Exit(PIF_stRingBuffer *pstOwner)
{
	if (pstOwner->btShare == FALSE && pstOwner->__pcBuffer) {
        free(pstOwner->__pcBuffer);
        pstOwner->__pcBuffer = NULL;
    }
}

/**
 * @fn pifRingBuffer_SetDeviceCode
 * @brief
 * @param pstOwner
 * @param unDeviceCode
 */
void pifRingBuffer_SetDeviceCode(PIF_stRingBuffer *pstOwner, PIF_unDeviceCode unDeviceCode)
{
	pstOwner->unDeviceCode = unDeviceCode;
}

/**
 * @fn pifRingBuffer_SetName
 * @brief
 * @param pstOwner
 * @param psName
 */
void pifRingBuffer_SetName(PIF_stRingBuffer *pstOwner, const char *psName)
{
	pstOwner->psName = psName;
}

/**
 * @fn pifRingBuffer_ChopsOffNone
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_ChopsOffNone(PIF_stRingBuffer *pstOwner)
{
	pstOwner->btChopOff = RB_CHOP_OFF_NONE;
}

/**
 * @fn pifRingBuffer_ChopsOffChar
 * @brief
 * @param pstOwner
 * @param cChar
 */
void pifRingBuffer_ChopsOffChar(PIF_stRingBuffer *pstOwner, char cChar)
{
	pstOwner->btChopOff = RB_CHOP_OFF_CHAR;
	pstOwner->__cChopOffChar = cChar;
}

/**
 * @fn pifRingBuffer_ChopsOffLength
 * @brief
 * @param pstOwner
 * @param usLength
 */
void pifRingBuffer_ChopsOffLength(PIF_stRingBuffer *pstOwner, uint16_t usLength)
{
	pstOwner->btChopOff = RB_CHOP_OFF_LENGTH;
	pstOwner->__usChopOffLength = usLength;
}

/**
 * @fn pifRingBuffer_IsAlloc
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsAlloc(PIF_stRingBuffer *pstOwner)
{
	return pstOwner->__pcBuffer != NULL;
}

/**
 * @fn pifRingBuffer_IsEmpty
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsEmpty(PIF_stRingBuffer *pstOwner)
{
	return pstOwner->__usHead == pstOwner->__usTail;
}

/**
 * @fn pifRingBuffer_GetFillSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingBuffer_GetFillSize(PIF_stRingBuffer *pstOwner)
{
	uint16_t usFill;

    if (pstOwner->__usHead > pstOwner->__usTail) {
    	usFill = pstOwner->__usHead - pstOwner->__usTail;
    }
    else {
    	usFill = pstOwner->usSize - pstOwner->__usTail + pstOwner->__usHead;
    }
    return usFill;
}

/**
 * @fn pifRingBuffer_GetRemainSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingBuffer_GetRemainSize(PIF_stRingBuffer *pstOwner)
{
	uint16_t usRemain;

    if (pstOwner->__usHead < pstOwner->__usTail) {
    	usRemain = pstOwner->__usTail - pstOwner->__usHead;
    }
    else {
    	usRemain = pstOwner->usSize - pstOwner->__usHead + pstOwner->__usTail;
    }
    return usRemain - 1;
}

/**
 * @fn pifRingBuffer_BackupHead
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_BackupHead(PIF_stRingBuffer *pstOwner)
{
	pstOwner->__usBackupHead = pstOwner->__usHead;
}

/**
 * @fn pifRingBuffer_RestoreHead
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_RestoreHead(PIF_stRingBuffer *pstOwner)
{
	pstOwner->__usHead = pstOwner->__usBackupHead;
}

/**
 * @fn pifRingBuffer_PutByte
 * @brief
 * @param pstOwner
 * @param ucData
 * @return
 */
BOOL pifRingBuffer_PutByte(PIF_stRingBuffer *pstOwner, uint8_t ucData)
{
    uint16_t usNext;

    usNext = pstOwner->__usHead + 1;
	if (usNext >= pstOwner->usSize) usNext = 0;
    if (usNext == pstOwner->__usTail) {
    	if (!_ChopOff(pstOwner, 1)) {
    		pif_enError = E_enOverflowBuffer;
    		goto fail;
    	}
    }

    pstOwner->__pcBuffer[pstOwner->__usHead] = ucData;
    pstOwner->__usHead = usNext;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "RingBuffer(%d/%s):PutByte(D:%u) EC:%d", pstOwner->unDeviceCode, pstOwner->psName, ucData, pif_enError);
	return FALSE;
}

/**
 * @fn pifRingBuffer_PutData
 * @brief
 * @param pstOwner
 * @param pucData
 * @param usLength
 * @return
 */
BOOL pifRingBuffer_PutData(PIF_stRingBuffer *pstOwner, uint8_t *pucData, uint16_t usLength)
{
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner);

    if (usLength > usRemain) {
    	if (!_ChopOff(pstOwner, usLength - usRemain)) {
    		pif_enError = E_enOverflowBuffer;
    		goto fail;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pcBuffer[pstOwner->__usHead] = pucData[i];
    	pstOwner->__usHead++;
    	if (pstOwner->__usHead >= pstOwner->usSize) pstOwner->__usHead = 0;
    }
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "RingBuffer(%d/%s):PutData(L:%u) EC:%d", pstOwner->unDeviceCode, pstOwner->psName, usLength, pif_enError);
	return FALSE;
}

/**
 * @fn pifRingBuffer_PutString
 * @brief
 * @param pstOwner
 * @param pcString
 * @return
 */
BOOL pifRingBuffer_PutString(PIF_stRingBuffer *pstOwner, char *pcString)
{
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner);
	uint16_t usLength = strlen(pcString);

    if (usLength > usRemain) {
    	if (!_ChopOff(pstOwner, usLength - usRemain)) {
    		pif_enError = E_enOverflowBuffer;
    		goto fail;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pcBuffer[pstOwner->__usHead] = pcString[i];
    	pstOwner->__usHead++;
    	if (pstOwner->__usHead >= pstOwner->usSize) pstOwner->__usHead = 0;
    }
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "RingBuffer(%d/%s):PutString() EC:%d Len:%u Rem:%u", pstOwner->unDeviceCode, pstOwner->psName, pif_enError, usLength, usRemain);
	return FALSE;
}

/**
 * @fn pifRingBuffer_GetByte
 * @brief
 * @param pstOwner
 * @param pucData
 * @return
 */
BOOL pifRingBuffer_GetByte(PIF_stRingBuffer *pstOwner, uint8_t *pucData)
{
	if (pstOwner->__usTail == pstOwner->__usHead) return FALSE;

	*pucData = pstOwner->__pcBuffer[pstOwner->__usTail];
	pstOwner->__usTail++;
	if (pstOwner->__usTail >= pstOwner->usSize) pstOwner->__usTail = 0;
	return TRUE;
}

/**
 * @fn pifRingBuffer_CopyToArray
 * @brief
 * @param pucDst
 * @param pstSrc
 * @param usCount
 * @return
 */
uint16_t pifRingBuffer_CopyToArray(uint8_t *pucDst, PIF_stRingBuffer *pstSrc, uint16_t usCount)
{
	uint16_t usTail = pstSrc->__usTail;

	for (uint16_t i = 0; i < usCount; i++) {
		pucDst[i] = pstSrc->__pcBuffer[usTail];
		usTail++;
		if (usTail >= pstSrc->usSize) usTail = 0;
		if (usTail == pstSrc->__usHead) return i + 1;
	}
	return usCount;
}

/**
 * @fn pifRingBuffer_CopyAll
 * @brief
 * @param pstDst
 * @param pstSrc
 * @param usPos
 * @return
 */
uint16_t pifRingBuffer_CopyAll(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc, uint16_t usPos)
{
	uint16_t usFill = pifRingBuffer_GetFillSize(pstSrc) - usPos;
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstDst);
	uint16_t usLength = usRemain < usFill ? usRemain : usFill;

	uint16_t usTail = pstSrc->__usTail + usPos;
	if (usTail >= pstSrc->usSize) usTail -= pstSrc->usSize;
	for (uint16_t i = 0; i < usLength; i++) {
		pifRingBuffer_PutByte(pstDst, pstSrc->__pcBuffer[usTail]);
		usTail++;
		if (usTail >= pstSrc->usSize) usTail = 0;
	}
	return usLength;
}

/**
 * @fn pifRingBuffer_CopyLength
 * @brief
 * @param pstDst
 * @param pstSrc
 * @param usPos
 * @param usLength
 * @return
 */
BOOL pifRingBuffer_CopyLength(PIF_stRingBuffer *pstDst, PIF_stRingBuffer *pstSrc, uint16_t usPos, uint16_t usLength)
{
	uint16_t usFill = pifRingBuffer_GetFillSize(pstSrc);

	if (usPos + usLength > usFill) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}
	if (pifRingBuffer_GetRemainSize(pstDst) < usLength) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

	uint16_t usTail = (pstSrc->__usTail + usPos) % pstSrc->usSize;
	while (usTail != pstSrc->__usHead) {
		pifRingBuffer_PutByte(pstDst, pstSrc->__pcBuffer[usTail]);
		usTail++;
		if (usTail >= pstSrc->usSize) usTail = 0;
	}
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "RingBuffer(%d/%s):CopyLength(P:%u L:%u) EC:%d Fill:%u", pstSrc->unDeviceCode, pstSrc->psName, usPos, usLength, pif_enError, usFill);
	return FALSE;
}

/**
 * @fn pifRingBuffer_Remove
 * @brief
 * @param pstOwner
 * @param usSize
 * @return
 */
void pifRingBuffer_Remove(PIF_stRingBuffer *pstOwner, uint16_t usSize)
{
	uint16_t usFill = pifRingBuffer_GetFillSize(pstOwner);

	if (usSize >= usFill) {
		pstOwner->__usTail = pstOwner->__usHead;
	}
	else {
		pstOwner->__usTail = (pstOwner->__usTail + usSize) % pstOwner->usSize;
	}
}
