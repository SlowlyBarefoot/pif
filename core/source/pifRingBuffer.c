#include <string.h>

#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifRingBuffer.h"


static BOOL _ChopOff(PIF_stRingBuffer *pstOwner, uint16_t usAlloc)
{
	uint16_t usLength = pifRingBuffer_GetFillSize(pstOwner);
	uint16_t usSize, usTail;

	switch (pstOwner->_btChopOff) {
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
			if (usTail >= pstOwner->_usSize) usTail -= pstOwner->_usSize;
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
PIF_stRingBuffer *pifRingBuffer_InitHeap(PIF_usId usPifId, uint16_t usSize)
{
	PIF_stRingBuffer *pstOwner = NULL;

    if (!usSize) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stRingBuffer), 1);
	if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstOwner->__pcBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstOwner->__pcBuffer) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
	pstOwner->_usPifId = usPifId;
	pstOwner->__psName = NULL;
    pstOwner->_btStatic = FALSE;
    pstOwner->_usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    return pstOwner;

fail:
	if (pstOwner) {
		if (pstOwner->__pcBuffer) {
			free(pstOwner->__pcBuffer);
		}
		free(pstOwner);
	}
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "RingBuffer:Init(S:%u) EC:%d", usSize, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifRingBuffer_InitStatic
 * @brief
 * @param usPifId
 * @param usSize
 * @param pcBuffer
 */
PIF_stRingBuffer *pifRingBuffer_InitStatic(PIF_usId usPifId, uint16_t usSize, char *pcBuffer)
{
	PIF_stRingBuffer *pstOwner = NULL;

    if (!usSize) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    pstOwner = calloc(sizeof(PIF_stRingBuffer), 1);
	if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstOwner->__pcBuffer = pcBuffer;

	if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
	pstOwner->_usPifId = usPifId;
	pstOwner->__psName = NULL;
    pstOwner->_btStatic = TRUE;
    pstOwner->_usSize = usSize;
    pstOwner->__usHead = 0;
    pstOwner->__usTail = 0;
    return pstOwner;

fail:
	if (pstOwner) {
		free(pstOwner);
	}
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "RingBuffer:Init(S:%u) EC:%d", usSize, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifRingBuffer_Exit
 * @brief 
 * @param pstOwner
 */
void pifRingBuffer_Exit(PIF_stRingBuffer *pstOwner)
{
	if (pstOwner->_btStatic == FALSE && pstOwner->__pcBuffer) {
        free(pstOwner->__pcBuffer);
        pstOwner->__pcBuffer = NULL;
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
    if (pstOwner->_btStatic) {
		pif_enError = E_enInvalidState;
    	return FALSE;
    }

    if (pstOwner->__pcBuffer) {
    	free(pstOwner->__pcBuffer);
    	pstOwner->__pcBuffer = NULL;
    }

	pstOwner->__pcBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstOwner->__pcBuffer) {
		pif_enError = E_enOutOfHeap;
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
 * @fn pifRingBuffer_ChopsOffNone
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_ChopsOffNone(PIF_stRingBuffer *pstOwner)
{
	pstOwner->_btChopOff = RB_CHOP_OFF_NONE;
}

/**
 * @fn pifRingBuffer_ChopsOffChar
 * @brief
 * @param pstOwner
 * @param cChar
 */
void pifRingBuffer_ChopsOffChar(PIF_stRingBuffer *pstOwner, char cChar)
{
	pstOwner->_btChopOff = RB_CHOP_OFF_CHAR;
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
	pstOwner->_btChopOff = RB_CHOP_OFF_LENGTH;
	pstOwner->__usChopOffLength = usLength;
}

/**
 * @fn pifRingBuffer_IsBuffer
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsBuffer(PIF_stRingBuffer *pstOwner)
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

    if (pstOwner->__usHead >= pstOwner->__usTail) {
    	usFill = pstOwner->__usHead - pstOwner->__usTail;
    }
    else {
    	usFill = pstOwner->_usSize - pstOwner->__usTail + pstOwner->__usHead;
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
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    pstOwner->__pcBuffer[pstOwner->__usHead] = ucData;
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
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pcBuffer[pstOwner->__usHead] = pucData[i];
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
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstOwner->__pcBuffer[pstOwner->__usHead] = pcString[i];
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

	*pucData = pstOwner->__pcBuffer[pstOwner->__usTail];
	pstOwner->__usTail++;
	if (pstOwner->__usTail >= pstOwner->_usSize) pstOwner->__usTail = 0;
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
		pifRingBuffer_PutByte(pstDst, pstSrc->__pcBuffer[usTail]);
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
		pif_enError = E_enInvalidParam;
		return FALSE;
	}
	if (pifRingBuffer_GetRemainSize(pstDst) < usLength) {
		pif_enError = E_enOverflowBuffer;
		return FALSE;
	}

	uint16_t usTail = (pstSrc->__usTail + usPos) % pstSrc->_usSize;
	while (usTail != pstSrc->__usHead) {
		pifRingBuffer_PutByte(pstDst, pstSrc->__pcBuffer[usTail]);
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
