#include <string.h>

#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifRingBuffer.h"


static BOOL _ChopOff(PIF_stRingBuffer *pstOwner, uint16_t usAlloc)
{
	uint16_t usLength = pifRingBuffer_GetFillSize(pstOwner);
	uint16_t usSize, usTail;

	switch (pstOwner->_bt.ChopOff) {
	case RB_CHOP_OFF_CHAR:
		usSize = 0;
		usTail = pstOwner->__usTail;
		while (usTail != pstOwner->__usHead) {
			if (pstOwner->__pucBuffer[usTail] == pstOwner->__ui.cChopOffChar) {
				if (usSize > usAlloc) {
					pstOwner->__usTail = usTail;
					return TRUE;
				}
			}
			usTail++;
			if (usTail >= pstOwner->_usSize) usTail -= pstOwner->_usSize;
			usSize++;
		}
		break;

	case RB_CHOP_OFF_LENGTH:
		usSize = pstOwner->__ui.usChopOffLength;
		while (usAlloc > usSize) {
			usSize += pstOwner->__ui.usChopOffLength;
		}
		if (usSize < usLength) {
			pstOwner->__usTail += usSize;
			if (pstOwner->__usTail >= pstOwner->_usSize) pstOwner->__usTail -= pstOwner->_usSize;
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
 * @fn pifRingBuffer_InitHeap
 * @brief
 * @param usPifId
 * @param usSize
 * @return 
 */
PIF_stRingBuffer *pifRingBuffer_InitHeap(PifId usPifId, uint16_t usSize)
{
	PIF_stRingBuffer *pstOwner = NULL;

    if (!usSize) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stRingBuffer), 1);
	if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	pstOwner->__pucBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstOwner->__pucBuffer) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
	pstOwner->_usPifId = usPifId;
	pstOwner->__psName = NULL;
    pstOwner->_bt.Static = FALSE;
    pstOwner->_usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    return pstOwner;

fail:
	if (pstOwner) {
		if (pstOwner->__pucBuffer) free(pstOwner->__pucBuffer);
		free(pstOwner);
	}
    return NULL;
}

/**
 * @fn pifRingBuffer_InitStatic
 * @brief
 * @param usPifId
 * @param usSize
 * @param pcBuffer
 */
PIF_stRingBuffer *pifRingBuffer_InitStatic(PifId usPifId, uint16_t usSize, uint8_t *pucBuffer)
{
	PIF_stRingBuffer *pstOwner = NULL;

    if (!usSize) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stRingBuffer), 1);
	if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	pstOwner->__pucBuffer = pucBuffer;

	if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
	pstOwner->_usPifId = usPifId;
	pstOwner->__psName = NULL;
    pstOwner->_bt.Static = TRUE;
    pstOwner->_usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
    return NULL;
}

/**
 * @fn pifRingBuffer_Exit
 * @brief 
 * @param pstOwner
 */
void pifRingBuffer_Exit(PIF_stRingBuffer *pstOwner)
{
	if (pstOwner->_bt.Static == FALSE && pstOwner->__pucBuffer) {
        free(pstOwner->__pucBuffer);
        pstOwner->__pucBuffer = NULL;
    }
}

/**
 * @fn pifRingBuffer_ResizeHeap
 * @brief
 * @param pstOwner
 * @param usSize
 * @return
 */
BOOL pifRingBuffer_ResizeHeap(PIF_stRingBuffer *pstOwner, uint16_t usSize)
{
    if (pstOwner->_bt.Static) {
		pif_error = E_INVALID_STATE;
    	return FALSE;
    }

    if (pstOwner->__pucBuffer) {
    	free(pstOwner->__pucBuffer);
    	pstOwner->__pucBuffer = NULL;
    }

	pstOwner->__pucBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstOwner->__pucBuffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
    pstOwner->_usSize = usSize;
	return TRUE;
}

/**
 * @fn pifRingBuffer_SetName
 * @brief
 * @param pstOwner
 * @param psName
 */
void pifRingBuffer_SetName(PIF_stRingBuffer *pstOwner, const char *psName)
{
	pstOwner->__psName = psName;
}

/**
 * @fn pifRingBuffer_GetTailPointer
 * @brief
 * @param pstOwner
 * @param usPos
 * @return
 */
uint8_t *pifRingBuffer_GetTailPointer(PIF_stRingBuffer *pstOwner, uint16_t usPos)
{
	return &pstOwner->__pucBuffer[(pstOwner->__usTail + usPos) % pstOwner->_usSize];
}

/**
 * @fn pifRingBuffer_ChopsOffNone
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_ChopsOffNone(PIF_stRingBuffer *pstOwner)
{
	pstOwner->_bt.ChopOff = RB_CHOP_OFF_NONE;
}

/**
 * @fn pifRingBuffer_ChopsOffChar
 * @brief
 * @param pstOwner
 * @param cChar
 */
void pifRingBuffer_ChopsOffChar(PIF_stRingBuffer *pstOwner, char cChar)
{
	pstOwner->_bt.ChopOff = RB_CHOP_OFF_CHAR;
	pstOwner->__ui.cChopOffChar = cChar;
}

/**
 * @fn pifRingBuffer_ChopsOffLength
 * @brief
 * @param pstOwner
 * @param usLength
 */
void pifRingBuffer_ChopsOffLength(PIF_stRingBuffer *pstOwner, uint16_t usLength)
{
	pstOwner->_bt.ChopOff = RB_CHOP_OFF_LENGTH;
	pstOwner->__ui.usChopOffLength = usLength;
}

/**
 * @fn pifRingBuffer_IsBuffer
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsBuffer(PIF_stRingBuffer *pstOwner)
{
	return pstOwner->__pucBuffer != NULL;
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

    if (pstOwner->__usHead >= pstOwner->__usTail) {
    	usFill = pstOwner->__usHead - pstOwner->__usTail;
    }
    else {
    	usFill = pstOwner->_usSize - pstOwner->__usTail + pstOwner->__usHead;
    }
    return usFill;
}

/**
 * @fn pifRingBuffer_GetLinerSize
 * @brief
 * @param pstOwner
 * @param usPos
 * @return
 */
uint16_t pifRingBuffer_GetLinerSize(PIF_stRingBuffer *pstOwner, uint16_t usPos)
{
	uint16_t usTail = (pstOwner->__usTail + usPos) % pstOwner->_usSize;

    if (pstOwner->__usHead >= usTail) {
    	return pstOwner->__usHead - usTail;
    }
    else {
    	return pstOwner->_usSize - usTail;
    }
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
    	usRemain = pstOwner->_usSize - pstOwner->__usHead + pstOwner->__usTail;
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
	if (usNext >= pstOwner->_usSize) usNext = 0;
    if (usNext == pstOwner->__usTail) {
    	if (!_ChopOff(pstOwner, 1)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    pstOwner->__pucBuffer[pstOwner->__usHead] = ucData;
    pstOwner->__usHead = usNext;
    return TRUE;
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
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pucBuffer[pstOwner->__usHead] = pucData[i];
    	pstOwner->__usHead++;
    	if (pstOwner->__usHead >= pstOwner->_usSize) pstOwner->__usHead = 0;
    }
    return TRUE;
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
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pucBuffer[pstOwner->__usHead] = pcString[i];
    	pstOwner->__usHead++;
    	if (pstOwner->__usHead >= pstOwner->_usSize) pstOwner->__usHead = 0;
    }
    return TRUE;
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

	*pucData = pstOwner->__pucBuffer[pstOwner->__usTail];
	pstOwner->__usTail++;
	if (pstOwner->__usTail >= pstOwner->_usSize) pstOwner->__usTail = 0;
	return TRUE;
}

/**
 * @fn pifRingBuffer_CopyToArray
 * @brief
 * @param pucDst
 * @param usCount
 * @param pstSrc
 * @param usPos
 * @return
 */
uint16_t pifRingBuffer_CopyToArray(uint8_t *pucDst, uint16_t usCount, PIF_stRingBuffer *pstSrc, uint16_t usPos)
{
	uint16_t usTail = pstSrc->__usTail + usPos;
	if (usTail >= pstSrc->_usSize) usTail -= pstSrc->_usSize;

	for (uint16_t i = 0; i < usCount; i++) {
		pucDst[i] = pstSrc->__pucBuffer[usTail];
		usTail++;
		if (usTail >= pstSrc->_usSize) usTail = 0;
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
	if (usTail >= pstSrc->_usSize) usTail -= pstSrc->_usSize;
	for (uint16_t i = 0; i < usLength; i++) {
		pifRingBuffer_PutByte(pstDst, pstSrc->__pucBuffer[usTail]);
		usTail++;
		if (usTail >= pstSrc->_usSize) usTail = 0;
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
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (pifRingBuffer_GetRemainSize(pstDst) < usLength) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	uint16_t usTail = (pstSrc->__usTail + usPos) % pstSrc->_usSize;
	while (usTail != pstSrc->__usHead) {
		pifRingBuffer_PutByte(pstDst, pstSrc->__pucBuffer[usTail]);
		usTail++;
		if (usTail >= pstSrc->_usSize) usTail = 0;
	}
	return TRUE;
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
		pstOwner->__usTail = (pstOwner->__usTail + usSize) % pstOwner->_usSize;
	}
}
