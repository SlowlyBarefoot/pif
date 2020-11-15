#include "pifDotMatrix.h"
#include "pifLog.h"


static PIF_stDotMatrix *s_pstDotMatrixArray = NULL;
static uint8_t s_ucDotMatrixArraySize;
static uint8_t s_ucDotMatrixArrayPos;

static PIF_stPulse *s_pstDotMatrixTimer;


static void _SetPattern(PIF_stDotMatrix *pstOwner, uint16_t usPositionX, uint16_t usPositionY, uint16_t usPosition)
{
    PIF_stDotMatrixPattern *pstPattern = &pstOwner->__pstPattern[pstOwner->ucPatternIndex];
    uint8_t *pucSrc, *pucDst, shift, col, row;

	shift = usPositionX & 7;
	pucSrc = pstPattern->pucPattern + usPositionY * pstPattern->ucColBytes + usPositionX / 8;
	pucDst = pstOwner->__pucPattern + usPosition;
    if (shift) {
    	uint8_t rest = 8 - shift;
    	uint8_t mask = ~((1 << rest) - 1);
    	for (row = 0; row < pstOwner->usRowSize; row++) {
    		for (col = 0; col < pstOwner->__usColBytes; col++) {
    			pucDst[col] = (pucSrc[col] >> shift) + ((pucSrc[col + 1] << rest) & mask);
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstOwner->__usColBytes;
    	}
    }
    else {
    	for (row = 0; row < pstOwner->usRowSize; row++) {
    		for (col = 0; col < pstOwner->__usColBytes; col++) {
    			pucDst[col] = pucSrc[col];
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstOwner->__usColBytes;
    	}
    }
}

static void _TimerDisplayFinish(PIF_stDotMatrix *pstOwner)
{
	uint8_t *pucPattern;
	uint8_t ucOff = 0;

	if (!pstOwner->btBlink) {
		pucPattern = pstOwner->__pucPattern;
		int index = pstOwner->__ucRowIndex * pstOwner->__usColBytes;
		(*pstOwner->__actDisplay)(pstOwner->__ucRowIndex, pucPattern + index);
		pucPattern += pstOwner->__usTotalBytes;
	}
	else {
		(*pstOwner->__actDisplay)(pstOwner->__ucRowIndex, &ucOff);
	}
	pstOwner->__ucRowIndex++;
	if (pstOwner->__ucRowIndex >= pstOwner->usRowSize) pstOwner->__ucRowIndex = 0;
}

static void _TimerBlinkFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrix *pstOwner = (PIF_stDotMatrix *)pvIssuer;
    pstOwner->btBlink ^= 1;
}

static void _TimerShiftFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrix *pstOwner = (PIF_stDotMatrix *)pvIssuer;

    switch (pstOwner->btShift.btDirection) {
    case DMS_enLeft:
        if (pstOwner->__usPositionX < pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucColSize - pstOwner->usColSize) {
        	pstOwner->__usPositionX++;
        }
        else if (pstOwner->btShift.btMethod == DMS_btPingPongHor) {
        	pstOwner->btShift.btDirection = DMS_enRight;
		}
        else if (pstOwner->btShift.btMethod == DMS_btRepeatHor) {
        	pstOwner->__usPositionX = 0;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->__evtShiftFinish) {
				(*pstOwner->__evtShiftFinish)(pstOwner);
			}
        }
    	break;

    case DMS_enRight:
        if (pstOwner->__usPositionX) {
        	pstOwner->__usPositionX--;
        }
        else if (pstOwner->btShift.btMethod == DMS_btPingPongHor) {
        	pstOwner->btShift.btDirection = DMS_enLeft;
		}
        else if (pstOwner->btShift.btMethod == DMS_btRepeatHor) {
        	pstOwner->__usPositionX = pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucColSize - pstOwner->usColSize;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->__evtShiftFinish) {
				(*pstOwner->__evtShiftFinish)(pstOwner);
			}
		}
    	break;

    case DMS_enUp:
        if (pstOwner->__usPositionY < pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucRowSize - pstOwner->usRowSize) {
        	pstOwner->__usPositionY++;
        }
        else if (pstOwner->btShift.btMethod == DMS_btPingPongVer) {
        	pstOwner->btShift.btDirection = DMS_enDown;
		}
        else if (pstOwner->btShift.btMethod == DMS_btRepeatVer) {
        	pstOwner->__usPositionY = 0;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->__evtShiftFinish) {
				(*pstOwner->__evtShiftFinish)(pstOwner);
			}
        }
    	break;

    case DMS_enDown:
        if (pstOwner->__usPositionY) {
        	pstOwner->__usPositionY--;
        }
        else if (pstOwner->btShift.btMethod == DMS_btPingPongVer) {
        	pstOwner->btShift.btDirection = DMS_enUp;
		}
        else if (pstOwner->btShift.btMethod == DMS_btRepeatVer) {
        	pstOwner->__usPositionY = pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucRowSize - pstOwner->usRowSize;
		}
		else {
			pifPulse_StopItem(pstOwner->__pstTimerShift);
			if (pstOwner->__evtShiftFinish) {
				(*pstOwner->__evtShiftFinish)(pstOwner);
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

static void _TaskCommon(PIF_stDotMatrix *pstOwner)
{
	if (!pstOwner->btRun) return;

	if (pif_usTimer1ms > pstOwner->__usPretimeMs) {
		if (pif_usTimer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return;
	}
	else if (pif_usTimer1ms < pstOwner->__usPretimeMs) {
		if (1000 - pstOwner->__usPretimeMs + pif_usTimer1ms < pstOwner->__usControlPeriodMs) return;
	}
	else return;

	_TimerDisplayFinish(pstOwner);

	pstOwner->__usPretimeMs = pif_usTimer1ms;
}

/**
 * @fn pifDotMatrix_Init
 * @brief
 * @param pstTimer 1ms Timer를 사용하여야 한다.
 * @param ucSize
 * @return
 */
BOOL pifDotMatrix_Init(PIF_stPulse *pstTimer1ms, uint8_t ucSize)
{
    if (!pstTimer1ms || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstDotMatrixArray = calloc(sizeof(PIF_stDotMatrix), ucSize);
    if (!s_pstDotMatrixArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucDotMatrixArraySize = ucSize;
    s_ucDotMatrixArrayPos = 0;

    s_pstDotMatrixTimer = pstTimer1ms;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifDotMatrix_Exit
 * @brief
 */
void pifDotMatrix_Exit()
{
	PIF_stDotMatrix *pstOwner;

    if (s_pstDotMatrixArray) {
		for (int i = 0; i < s_ucDotMatrixArraySize; i++) {
			pstOwner = &s_pstDotMatrixArray[i];
			if (pstOwner->__pucPattern) {
				free(pstOwner->__pucPattern);
				pstOwner->__pucPattern = NULL;
			}
			if (pstOwner->__pstPattern) {
				free(pstOwner->__pstPattern);
				pstOwner->__pstPattern = NULL;
			}
		}
    	free(s_pstDotMatrixArray);
        s_pstDotMatrixArray = NULL;
    }
}

/**
 * @fn pifDotMatrix_Add
 * @brief
 * @param unDeviceCode
 * @param ucColSize
 * @param ucRowSize
 * @param actDisplay
 * @return
 */
PIF_stDotMatrix *pifDotMatrix_Add(PIF_unDeviceCode unDeviceCode, uint16_t usColSize, uint16_t usRowSize,
		PIF_actDotMatrixDisplay actDisplay)
{
    if (s_ucDotMatrixArrayPos >= s_ucDotMatrixArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!usColSize || !usRowSize || !actDisplay) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDotMatrix *pstOwner = &s_pstDotMatrixArray[s_ucDotMatrixArrayPos];

	pstOwner->usColSize = usColSize;
	pstOwner->usRowSize = usRowSize;
	pstOwner->__usColBytes = (pstOwner->usColSize - 1) / 8 + 1;
	pstOwner->__usTotalBytes = pstOwner->__usColBytes * pstOwner->usRowSize;

	pstOwner->__pucPattern = calloc(sizeof(uint8_t), pstOwner->__usTotalBytes);
    if (!pstOwner->__pucPattern) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    pstOwner->unDeviceCode = unDeviceCode;
    pstOwner->__usControlPeriodMs = PIF_DM_CONTROL_PERIOD_DEFAULT;
	pstOwner->__actDisplay = actDisplay;
	pstOwner->__pstTimerBlink = NULL;
	pstOwner->__pstTimerShift = NULL;

    s_ucDotMatrixArrayPos = s_ucDotMatrixArrayPos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:AddSingle(D:%u UC:%u UR:%u) EC:%d", unDeviceCode,
			usColSize, usRowSize, pif_enError);
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
	pifLog_Printf(LT_enError, "DotMatrix:SetControlPeriod(P:%u) EC:%d", usPeriodMs, pif_enError);
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
	pifLog_Printf(LT_enError, "DotMatrix:SetPatternSize(S:%u) EC:%d", ucSize, pif_enError);
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

    if (!ucColSize || !ucRowSize || ucColSize < pstOwner->usColSize || ucRowSize < pstOwner->usRowSize || !pucPattern) {
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
	pifLog_Printf(LT_enError, "DotMatrix:AddPattern(C:%u R:%u) EC:%d", ucColSize, ucRowSize, pif_enError);
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
	pstOwner->btRun = TRUE;
}

/**
 * @fn pifDotMatrix_Stop
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_Stop(PIF_stDotMatrix *pstOwner)
{
	int col, row;
	uint8_t ucOff = 0;

	for (row = 0; row < pstOwner->usRowSize; row++) {
		for (col = 0; col < pstOwner->usColSize; col += 8) {
			(*pstOwner->__actDisplay)(row, &ucOff);
		}
	}
	pstOwner->btRun = FALSE;
    if (pstOwner->btBlink) {
		pifPulse_StopItem(pstOwner->__pstTimerBlink);
		pstOwner->btBlink = FALSE;
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

    pstOwner->ucPatternIndex = ucPatternIndex;
    _SetPattern(pstOwner, pstOwner->__usPositionX, pstOwner->__usPositionY, 0);
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:SelectPattern(P:%u) EC:%d", ucPatternIndex, pif_enError);
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
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _TimerBlinkFinish, pstOwner);
	}
	if (!pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs)) return FALSE;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:BlinkOn(P:%u) EC:%d", usPeriodMs, pif_enError);
    return FALSE;
}

/**
 * @fn pifDotMatrix_BlinkOff
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_BlinkOff(PIF_stDotMatrix *pstOwner)
{
	pstOwner->btBlink = FALSE;
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
		pifPulse_ResetItem(pstOwner->__pstTimerBlink, usPeriodMs);
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
    if (usX >= pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucColSize - pstOwner->usColSize ||
    		usY >= pstOwner->__pstPattern[pstOwner->ucPatternIndex].ucRowSize - pstOwner->usRowSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    pstOwner->__usPositionX = usX;
    pstOwner->__usPositionY = usY;
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:SetPosition(X:%u Y:%u) EC:%d", usX, usY, pif_enError);
	return FALSE;
}

/**
 * @fn pifDotMatrix_AttachEventShiftFinish
 * @brief
 * @param pstOwner
 * @param evtShiftFinish
 */
void pifDotMatrix_AttachEvtShiftFinish(PIF_stDotMatrix *pstOwner, PIF_evtDotMatrixShiftFinish evtShiftFinish)
{
	pstOwner->__evtShiftFinish = evtShiftFinish;
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
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerShift, _TimerShiftFinish, pstOwner);
	}
	if(!pifPulse_StartItem(pstOwner->__pstTimerShift, usPeriodMs)) return FALSE;
	pstOwner->enShift = enShift;
	pstOwner->__usShiftCount = usCount;
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:ShiftOn(S:%u P:%u) EC:%d", enShift, usPeriodMs, pif_enError);
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
		pstOwner->enShift = DMS_enNone;
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
		pifPulse_ResetItem(pstOwner->__pstTimerShift, usPeriodMs);
	}
}

/**
 * @fn pifDotMatrix_taskAll
 * @brief
 * @param pstTask
 */
void pifDotMatrix_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucDotMatrixArrayPos; i++) {
		PIF_stDotMatrix *pstOwner = &s_pstDotMatrixArray[i];
		if (!pstOwner->__enTaskLoop) _TaskCommon(pstOwner);
	}
}

/**
 * @fn pifDotMatrix_taskEach
 * @brief
 * @param pstTask
 */
void pifDotMatrix_taskEach(PIF_stTask *pstTask)
{
	PIF_stDotMatrix *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}
