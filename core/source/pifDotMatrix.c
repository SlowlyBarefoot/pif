#include "pifDotMatrix.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define PIF_DM_CONTROL_PERIOD_DEFAULT		2


static PIF_stDotMatrix *s_pstDotMatrix = NULL;
static uint8_t s_ucDotMatrixSize;
static uint8_t s_ucDotMatrixPos;

static PIF_stPulse *s_pstDotMatrixTimer;


static void _SetPattern(PIF_stDotMatrix *pstOwner, uint16_t usPositionX, uint16_t usPositionY, uint16_t usPosition)
{
    PIF_stDotMatrixPattern *pstPattern = &pstOwner->__pstPattern[pstOwner->__ucPatternIndex];
    uint8_t *pucSrc, *pucDst, shift, col, row;

	shift = usPositionX & 7;
	pucSrc = pstPattern->pucPattern + usPositionY * pstPattern->ucColBytes + usPositionX / 8;
	pucDst = pstOwner->__pucPattern + usPosition;
    if (shift) {
    	uint8_t rest = 8 - shift;
    	uint8_t mask = ~((1 << rest) - 1);
    	for (row = 0; row < pstOwner->__usRowSize; row++) {
    		for (col = 0; col < pstOwner->__usColBytes; col++) {
    			pucDst[col] = (pucSrc[col] >> shift) + ((pucSrc[col + 1] << rest) & mask);
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstOwner->__usColBytes;
    	}
    }
    else {
    	for (row = 0; row < pstOwner->__usRowSize; row++) {
    		for (col = 0; col < pstOwner->__usColBytes; col++) {
    			pucDst[col] = pucSrc[col];
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstOwner->__usColBytes;
    	}
    }
}

static void _evtTimerBlinkFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrix *pstOwner = (PIF_stDotMatrix *)pvIssuer;
    pstOwner->__btBlink ^= 1;
}

static void _evtTimerShiftFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrix *pstOwner = (PIF_stDotMatrix *)pvIssuer;

    switch (pstOwner->__btShift.btDirection) {
    case DMS_enLeft:
        if (pstOwner->__usPositionX < pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucColSize - pstOwner->__usColSize) {
        	pstOwner->__usPositionX++;
        }
        else if (pstOwner->__btShift.btMethod == DMS_btPingPongHor) {
        	pstOwner->__btShift.btDirection = DMS_enRight;
		}
        else if (pstOwner->__btShift.btMethod == DMS_btRepeatHor) {
        	pstOwner->__usPositionX = 0;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->evtShiftFinish) {
				(*pstOwner->evtShiftFinish)(pstOwner->_usPifId);
			}
        }
    	break;

    case DMS_enRight:
        if (pstOwner->__usPositionX) {
        	pstOwner->__usPositionX--;
        }
        else if (pstOwner->__btShift.btMethod == DMS_btPingPongHor) {
        	pstOwner->__btShift.btDirection = DMS_enLeft;
		}
        else if (pstOwner->__btShift.btMethod == DMS_btRepeatHor) {
        	pstOwner->__usPositionX = pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucColSize - pstOwner->__usColSize;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->evtShiftFinish) {
				(*pstOwner->evtShiftFinish)(pstOwner->_usPifId);
			}
		}
    	break;

    case DMS_enUp:
        if (pstOwner->__usPositionY < pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucRowSize - pstOwner->__usRowSize) {
        	pstOwner->__usPositionY++;
        }
        else if (pstOwner->__btShift.btMethod == DMS_btPingPongVer) {
        	pstOwner->__btShift.btDirection = DMS_enDown;
		}
        else if (pstOwner->__btShift.btMethod == DMS_btRepeatVer) {
        	pstOwner->__usPositionY = 0;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->evtShiftFinish) {
				(*pstOwner->evtShiftFinish)(pstOwner->_usPifId);
			}
        }
    	break;

    case DMS_enDown:
        if (pstOwner->__usPositionY) {
        	pstOwner->__usPositionY--;
        }
        else if (pstOwner->__btShift.btMethod == DMS_btPingPongVer) {
        	pstOwner->__btShift.btDirection = DMS_enUp;
		}
        else if (pstOwner->__btShift.btMethod == DMS_btRepeatVer) {
        	pstOwner->__usPositionY = pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucRowSize - pstOwner->__usRowSize;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->evtShiftFinish) {
				(*pstOwner->evtShiftFinish)(pstOwner->_usPifId);
			}
		}
    	break;
    }
    _SetPattern(pstOwner, pstOwner->__usPositionX, pstOwner->__usPositionY, 0);

    if (pstOwner->__usShiftCount) {
    	pstOwner->__usShiftCount--;
		if (!pstOwner->__usShiftCount) {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
		}
    }
}

static void _taskCommon(PIF_stDotMatrix *pstOwner)
{
	uint8_t *pucPattern;
	uint8_t ucOff = 0;

	if (!pstOwner->__btRun) return;

	if (pif_usTimer1ms > pstOwner->__usPretimeMs) {
		if (pif_usTimer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return;
	}
	else if (pif_usTimer1ms < pstOwner->__usPretimeMs) {
		if (1000 - pstOwner->__usPretimeMs + pif_usTimer1ms < pstOwner->__usControlPeriodMs) return;
	}
	else return;

	if (!pstOwner->__btBlink) {
		pucPattern = pstOwner->__pucPattern;
		int index = pstOwner->__ucRowIndex * pstOwner->__usColBytes;
		(*pstOwner->__actDisplay)(pstOwner->__ucRowIndex, pucPattern + index);
		pucPattern += pstOwner->__usTotalBytes;
	}
	else {
		(*pstOwner->__actDisplay)(pstOwner->__ucRowIndex, &ucOff);
	}
	pstOwner->__ucRowIndex++;
	if (pstOwner->__ucRowIndex >= pstOwner->__usRowSize) pstOwner->__ucRowIndex = 0;

	pstOwner->__usPretimeMs = pif_usTimer1ms;
}

/**
 * @fn pifDotMatrix_Init
 * @brief
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifDotMatrix_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstDotMatrix = calloc(sizeof(PIF_stDotMatrix), ucSize);
    if (!s_pstDotMatrix) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucDotMatrixSize = ucSize;
    s_ucDotMatrixPos = 0;

    s_pstDotMatrixTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDotMatrix_Exit
 * @brief
 */
void pifDotMatrix_Exit()
{
	PIF_stDotMatrix *pstOwner;

    if (s_pstDotMatrix) {
		for (int i = 0; i < s_ucDotMatrixSize; i++) {
			pstOwner = &s_pstDotMatrix[i];
			if (pstOwner->__pucPattern) {
				free(pstOwner->__pucPattern);
				pstOwner->__pucPattern = NULL;
			}
			if (pstOwner->__pstPattern) {
				free(pstOwner->__pstPattern);
				pstOwner->__pstPattern = NULL;
			}
			if (pstOwner->__pstTimerBlink) {
				pifPulse_RemoveItem(s_pstDotMatrixTimer, pstOwner->__pstTimerBlink);
			}
			if (pstOwner->__pstTimerShift) {
				pifPulse_RemoveItem(s_pstDotMatrixTimer, pstOwner->__pstTimerShift);
			}
		}
    	free(s_pstDotMatrix);
        s_pstDotMatrix = NULL;
    }
}

/**
 * @fn pifDotMatrix_Add
 * @brief
 * @param usPifId
 * @param ucColSize
 * @param ucRowSize
 * @param actDisplay
 * @return
 */
PIF_stDotMatrix *pifDotMatrix_Add(PIF_usId usPifId, uint16_t usColSize, uint16_t usRowSize,
		PIF_actDotMatrixDisplay actDisplay)
{
    if (s_ucDotMatrixPos >= s_ucDotMatrixSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!usColSize || !usRowSize || !actDisplay) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDotMatrix *pstOwner = &s_pstDotMatrix[s_ucDotMatrixPos];

    pstOwner->__usColSize = usColSize;
    pstOwner->__usRowSize = usRowSize;
    pstOwner->__usColBytes = (pstOwner->__usColSize - 1) / 8 + 1;
    pstOwner->__usTotalBytes = pstOwner->__usColBytes * pstOwner->__usRowSize;

    pstOwner->__pucPattern = calloc(sizeof(uint8_t), pstOwner->__usTotalBytes);
    if (!pstOwner->__pucPattern) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->__usControlPeriodMs = PIF_DM_CONTROL_PERIOD_DEFAULT;
    pstOwner->__actDisplay = actDisplay;
    pstOwner->__pstTimerBlink = NULL;
    pstOwner->__pstTimerShift = NULL;

    s_ucDotMatrixPos = s_ucDotMatrixPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:AddSingle(D:%u UC:%u UR:%u) EC:%d", usPifId,
			usColSize, usRowSize, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifDotMatrix_SetControlPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifDotMatrix_SetControlPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs)
{
    if (!usPeriodMs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    pstOwner->__usControlPeriodMs = usPeriodMs;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:SetControlPeriod(P:%u) EC:%d", usPeriodMs, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDotMatrix_SetPatternSize
 * @brief
 * @param pstOwner
 * @param ucSize
 * @return
 */
BOOL pifDotMatrix_SetPatternSize(PIF_stDotMatrix *pstOwner, uint8_t ucSize)
{
	if (pstOwner->__pstPattern) free(pstOwner->__pstPattern);

	pstOwner->__pstPattern = calloc(sizeof(PIF_stDotMatrixPattern), ucSize);
    if (!pstOwner->__pstPattern) {
		pif_enError = E_enOutOfHeap;
        goto fail;
	}
    pstOwner->__ucPatternSize = ucSize;
    pstOwner->__ucPatternCount = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:SetPatternSize(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDotMatrix_AddPattern
 * @brief
 * @param pstOwner
 * @param ucColSize
 * @param ucRowSize
 * @param pucPattern
 * @return
 */
BOOL pifDotMatrix_AddPattern(PIF_stDotMatrix *pstOwner, uint8_t ucColSize, uint8_t ucRowSize, uint8_t *pucPattern)
{
	if (pstOwner->__ucPatternCount >= pstOwner->__ucPatternSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucColSize || !ucRowSize || ucColSize < pstOwner->__usColSize || ucRowSize < pstOwner->__usRowSize || !pucPattern) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDotMatrixPattern *pstPattern = &pstOwner->__pstPattern[pstOwner->__ucPatternCount];

    pstPattern->ucColSize = ucColSize;
    pstPattern->ucColBytes = (ucColSize - 1) / 8 + 1;
    pstPattern->ucRowSize = ucRowSize;
    pstPattern->pucPattern = pucPattern;

    pstOwner->__ucPatternCount = pstOwner->__ucPatternCount + 1;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:AddPattern(C:%u R:%u) EC:%d", ucColSize, ucRowSize, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifDotMatrix_Start
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_Start(PIF_stDotMatrix *pstOwner)
{
    _SetPattern(pstOwner, pstOwner->__usPositionX, pstOwner->__usPositionY, 0);
    pstOwner->__btRun = TRUE;
}

/**
 * @fn pifDotMatrix_Stop
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_Stop(PIF_stDotMatrix *pstOwner)
{
	uint16_t col, row;
	uint8_t ucOff = 0;

	for (row = 0; row < pstOwner->__usRowSize; row++) {
		for (col = 0; col < pstOwner->__usColSize; col += 8) {
			(*pstOwner->__actDisplay)(row, &ucOff);
		}
	}
	pstOwner->__btRun = FALSE;
    if (pstOwner->__btBlink) {
		pifPulse_StopItem(pstOwner->__pstTimerBlink);
		pstOwner->__btBlink = FALSE;
    }
}

/**
 * @fn pifDotMatrix_SelectPattern
 * @brief
 * @param pstOwner
 * @param ucPatternIndex
 * @return
 */
BOOL pifDotMatrix_SelectPattern(PIF_stDotMatrix *pstOwner, uint8_t ucPatternIndex)
{
	if (ucPatternIndex >= pstOwner->__ucPatternSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	pstOwner->__ucPatternIndex = ucPatternIndex;
    _SetPattern(pstOwner, pstOwner->__usPositionX, pstOwner->__usPositionY, 0);
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:SelectPattern(P:%u) EC:%d", ucPatternIndex, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDotMatrix_BlinkOn
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifDotMatrix_BlinkOn(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs)
{
	if (!usPeriodMs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink = pifPulse_AddItem(s_pstDotMatrixTimer, PT_enRepeat);
		if (!pstOwner->__pstTimerBlink) return FALSE;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _evtTimerBlinkFinish, pstOwner);
	}
	if (!pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs * 1000L / s_pstDotMatrixTimer->_unPeriodUs)) return FALSE;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:BlinkOn(P:%u) EC:%d", usPeriodMs, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDotMatrix_BlinkOff
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_BlinkOff(PIF_stDotMatrix *pstOwner)
{
	pstOwner->__btBlink = FALSE;
	if (pstOwner->__pstTimerBlink) {
		pifPulse_RemoveItem(s_pstDotMatrixTimer, pstOwner->__pstTimerBlink);
		pstOwner->__pstTimerBlink = NULL;
	}
}

/**
 * @fn pifDotMatrix_ChangeBlinkPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 */
void pifDotMatrix_ChangeBlinkPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs)
{
	if (pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink->unTarget = usPeriodMs * 1000 / s_pstDotMatrixTimer->_unPeriodUs;
	}
}

/**
 * @fn pifDotMatrix_SetPosition
 * @brief
 * @param pstOwner
 * @param usX
 * @param usY
 * @return
 */
BOOL pifDotMatrix_SetPosition(PIF_stDotMatrix *pstOwner, uint16_t usX, uint16_t usY)
{
	if (usX >= pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucColSize - pstOwner->__usColSize ||
    		usY >= pstOwner->__pstPattern[pstOwner->__ucPatternIndex].ucRowSize - pstOwner->__usRowSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	pstOwner->__usPositionX = usX;
	pstOwner->__usPositionY = usY;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:SetPosition(X:%u Y:%u) EC:%d", usX, usY, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifDotMatrix_ShiftOn
 * @brief
 * @param pstOwner
 * @param enShift
 * @param usPeriod
 * @param usCount
 * @return
 */
BOOL pifDotMatrix_ShiftOn(PIF_stDotMatrix *pstOwner, PIF_enDotMatrixShift enShift, uint16_t usPeriodMs, uint16_t usCount)
{
	if (!usPeriodMs || enShift == DMS_enNone) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstOwner->__pstTimerShift) {
		pstOwner->__pstTimerShift = pifPulse_AddItem(s_pstDotMatrixTimer, PT_enRepeat);
		if (!pstOwner->__pstTimerShift) return FALSE;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerShift, _evtTimerShiftFinish, pstOwner);
	}
	if(!pifPulse_StartItem(pstOwner->__pstTimerShift, usPeriodMs * 1000L / s_pstDotMatrixTimer->_unPeriodUs)) return FALSE;
	pstOwner->__enShift = enShift;
	pstOwner->__usShiftCount = usCount;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DotMatrix:ShiftOn(S:%u P:%u) EC:%d", enShift, usPeriodMs, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifDotMatrix_ShiftOff
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_ShiftOff(PIF_stDotMatrix *pstOwner)
{
	if (pstOwner->__pstTimerShift) {
		pstOwner->__enShift = DMS_enNone;
		pifPulse_StopItem(pstOwner->__pstTimerShift);
		pstOwner->__usPositionX = 0;
		pstOwner->__usPositionY = 0;
	}
}

/**
 * @fn pifDotMatrix_ChnageShiftPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 */
void pifDotMatrix_ChangeShiftPeriod(PIF_stDotMatrix *pstOwner, uint16_t usPeriodMs)
{
	if (pstOwner->__pstTimerShift) {
		pstOwner->__pstTimerShift->unTarget = usPeriodMs * 1000 / s_pstDotMatrixTimer->_unPeriodUs;
	}
}

/**
 * @fn pifDotMatrix_taskAll
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifDotMatrix_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucDotMatrixPos; i++) {
		PIF_stDotMatrix *pstOwner = &s_pstDotMatrix[i];
		if (!pstOwner->__enTaskLoop) _taskCommon(pstOwner);
	}
	return 0;
}

/**
 * @fn pifDotMatrix_taskEach
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifDotMatrix_taskEach(PIF_stTask *pstTask)
{
	PIF_stDotMatrix *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_taskCommon(pstOwner);
	}
	return 0;
}
