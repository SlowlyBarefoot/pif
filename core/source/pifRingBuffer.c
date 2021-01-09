#include <string.h>

#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifRingBuffer.h"


typedef struct _PIF_stRingBufferBase
{
	// Public Member Variable
	PIF_stRingBuffer stOwner;

	// Private Member Variable
	const char *psName;
    char *pcBuffer;
    uint16_t usHead;
    uint16_t usTail;
    uint16_t usBackupHead;
    union {
		char cChopOffChar;
		uint16_t usChopOffLength;
    };
} PIF_stRingBufferBase;


static BOOL _ChopOff(PIF_stRingBufferBase *pstBase, uint16_t usAlloc)
{
	PIF_stRingBuffer *pstOwner = (PIF_stRingBuffer *)pstBase;
	uint16_t usLength = pifRingBuffer_GetFillSize(pstOwner);
	uint16_t usSize, usTail;

	switch (pstOwner->btChopOff) {
	case RB_CHOP_OFF_CHAR:
		usSize = 0;
		usTail = pstBase->usTail;
		while (usTail != pstBase->usHead) {
			if (pstBase->pcBuffer[usTail] == pstBase->cChopOffChar) {
				if (usSize > usAlloc) {
					pstBase->usTail = usTail;
					return TRUE;
				}
			}
			usTail++;
			if (usTail >= pstOwner->usSize) usTail -= pstOwner->usSize;
			usSize++;
		}
		break;

	case RB_CHOP_OFF_LENGTH:
		usSize = pstBase->usChopOffLength;
		while (usAlloc > usSize) {
			usSize += pstBase->usChopOffLength;
		}
		if (usSize < usLength) {
			pstBase->usTail += usSize;
			if (pstBase->usTail >= pstOwner->usSize) pstBase->usTail -= pstOwner->usSize;
			return TRUE;
		}
		else if (usAlloc <= usLength) {
			pstBase->usTail = pstBase->usHead;
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
	PIF_stRingBufferBase *pstBase = NULL;
	PIF_stRingBuffer *pstOwner;

    if (!usSize) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

	pstBase = calloc(sizeof(PIF_stRingBufferBase), 1);
	if (!pstBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstBase->pcBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstBase->pcBuffer) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstOwner = &pstBase->stOwner;
	if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
	pstOwner->usPifId = usPifId;
	pstBase->psName = NULL;
    pstOwner->btStatic = FALSE;
    pstOwner->usSize = usSize;
    pstBase->usHead = 0;
    pstBase->usTail = 0;
    return (PIF_stRingBuffer *)pstBase;

fail:
	if (pstBase) {
		if (pstBase->pcBuffer) {
			free(pstBase->pcBuffer);
		}
		free(pstBase);
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
	PIF_stRingBufferBase *pstBase = NULL;
	PIF_stRingBuffer *pstOwner;

    if (!usSize) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

	pstBase = calloc(sizeof(PIF_stRingBufferBase), 1);
	if (!pstBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pstOwner = &pstBase->stOwner;

	pstBase->pcBuffer = pcBuffer;

	if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
	pstOwner->usPifId = usPifId;
	pstBase->psName = NULL;
    pstOwner->btStatic = TRUE;
    pstOwner->usSize = usSize;
    pstBase->usHead = 0;
    pstBase->usTail = 0;
    return (PIF_stRingBuffer *)pstBase;

fail:
	if (pstBase) {
		free(pstBase);
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

	if (pstOwner->btStatic == FALSE && pstBase->pcBuffer) {
        free(pstBase->pcBuffer);
        pstBase->pcBuffer = NULL;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

    if (pstOwner->btStatic) {
		pif_enError = E_enInvalidState;
    	return FALSE;
    }

    if (pstBase->pcBuffer) {
    	free(pstBase->pcBuffer);
    	pstBase->pcBuffer = NULL;
    }

	pstBase->pcBuffer = calloc(sizeof(uint8_t), usSize);
	if (!pstBase->pcBuffer) {
		pif_enError = E_enOutOfHeap;
		return FALSE;
	}
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
	((PIF_stRingBufferBase *)pstOwner)->psName = psName;
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
	((PIF_stRingBufferBase *)pstOwner)->cChopOffChar = cChar;
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
	((PIF_stRingBufferBase *)pstOwner)->usChopOffLength = usLength;
}

/**
 * @fn pifRingBuffer_IsBuffer
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsBuffer(PIF_stRingBuffer *pstOwner)
{
	return ((PIF_stRingBufferBase *)pstOwner)->pcBuffer != NULL;
}

/**
 * @fn pifRingBuffer_IsEmpty
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifRingBuffer_IsEmpty(PIF_stRingBuffer *pstOwner)
{
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

	return pstBase->usHead == pstBase->usTail;
}

/**
 * @fn pifRingBuffer_GetFillSize
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifRingBuffer_GetFillSize(PIF_stRingBuffer *pstOwner)
{
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
	uint16_t usFill;

    if (pstBase->usHead >= pstBase->usTail) {
    	usFill = pstBase->usHead - pstBase->usTail;
    }
    else {
    	usFill = pstOwner->usSize - pstBase->usTail + pstBase->usHead;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
	uint16_t usRemain;

    if (pstBase->usHead < pstBase->usTail) {
    	usRemain = pstBase->usTail - pstBase->usHead;
    }
    else {
    	usRemain = pstOwner->usSize - pstBase->usHead + pstBase->usTail;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

	pstBase->usBackupHead = pstBase->usHead;
}

/**
 * @fn pifRingBuffer_RestoreHead
 * @brief
 * @param pstOwner
 */
void pifRingBuffer_RestoreHead(PIF_stRingBuffer *pstOwner)
{
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

	pstBase->usHead = pstBase->usBackupHead;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
    uint16_t usNext;

    usNext = pstBase->usHead + 1;
	if (usNext >= pstOwner->usSize) usNext = 0;
    if (usNext == pstBase->usTail) {
    	if (!_ChopOff(pstBase, 1)) {
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    pstBase->pcBuffer[pstBase->usHead] = ucData;
    pstBase->usHead = usNext;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner);

    if (usLength > usRemain) {
    	if (!_ChopOff(pstBase, usLength - usRemain)) {
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstBase->pcBuffer[pstBase->usHead] = pucData[i];
    	pstBase->usHead++;
    	if (pstBase->usHead >= pstOwner->usSize) pstBase->usHead = 0;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstOwner);
	uint16_t usLength = strlen(pcString);

    if (usLength > usRemain) {
    	if (!_ChopOff(pstBase, usLength - usRemain)) {
    		pif_enError = E_enOverflowBuffer;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < usLength; i++) {
    	pstBase->pcBuffer[pstBase->usHead] = pcString[i];
    	pstBase->usHead++;
    	if (pstBase->usHead >= pstOwner->usSize) pstBase->usHead = 0;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;

	if (pstBase->usTail == pstBase->usHead) return FALSE;

	*pucData = pstBase->pcBuffer[pstBase->usTail];
	pstBase->usTail++;
	if (pstBase->usTail >= pstOwner->usSize) pstBase->usTail = 0;
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
	PIF_stRingBufferBase *pstSrcBase = (PIF_stRingBufferBase *)pstSrc;
	uint16_t usTail = pstSrcBase->usTail;

	for (uint16_t i = 0; i < usCount; i++) {
		pucDst[i] = pstSrcBase->pcBuffer[usTail];
		usTail++;
		if (usTail >= pstSrc->usSize) usTail = 0;
		if (usTail == pstSrcBase->usHead) return i + 1;
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
	PIF_stRingBufferBase *pstSrcBase = (PIF_stRingBufferBase *)pstSrc;
	uint16_t usFill = pifRingBuffer_GetFillSize(pstSrc) - usPos;
	uint16_t usRemain = pifRingBuffer_GetRemainSize(pstDst);
	uint16_t usLength = usRemain < usFill ? usRemain : usFill;

	uint16_t usTail = pstSrcBase->usTail + usPos;
	if (usTail >= pstSrc->usSize) usTail -= pstSrc->usSize;
	for (uint16_t i = 0; i < usLength; i++) {
		pifRingBuffer_PutByte(pstDst, pstSrcBase->pcBuffer[usTail]);
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
	PIF_stRingBufferBase *pstSrcBase = (PIF_stRingBufferBase *)pstSrc;
	uint16_t usFill = pifRingBuffer_GetFillSize(pstSrc);

	if (usPos + usLength > usFill) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}
	if (pifRingBuffer_GetRemainSize(pstDst) < usLength) {
		pif_enError = E_enOverflowBuffer;
		return FALSE;
	}

	uint16_t usTail = (pstSrcBase->usTail + usPos) % pstSrc->usSize;
	while (usTail != pstSrcBase->usHead) {
		pifRingBuffer_PutByte(pstDst, pstSrcBase->pcBuffer[usTail]);
		usTail++;
		if (usTail >= pstSrc->usSize) usTail = 0;
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
	PIF_stRingBufferBase *pstBase = (PIF_stRingBufferBase *)pstOwner;
	uint16_t usFill = pifRingBuffer_GetFillSize(pstOwner);

	if (usSize >= usFill) {
		pstBase->usTail = pstBase->usHead;
	}
	else {
		pstBase->usTail = (pstBase->usTail + usSize) % pstOwner->usSize;
	}
}
