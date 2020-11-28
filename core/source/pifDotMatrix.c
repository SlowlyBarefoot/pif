#include "pifDotMatrix.h"
#include "pifLog.h"


#define PIF_DM_CONTROL_PERIOD_DEFAULT		2


typedef struct _PIF_stDotMatrixBase
{
	// Public Member Variable
	PIF_stDotMatrix stOwner;

    uint16_t usColSize;
    uint16_t usRowSize;

	uint8_t ucPatternIndex;

	struct {
		uint8_t btRun			: 1;
		uint8_t btBlink			: 1;
	};

	union {
		PIF_enDotMatrixShift enShift;
		struct {
			uint8_t btDirection	: 4;	// 2 : Left, 3 : Right, 4 : Up, 5 : Down
			uint8_t btMethod	: 4;	// 0 : Off, 2 : Repeat Hor, 3 : Repeat Ver, 4 : PingPong Hor, 5 : PingPong Ver
		} btShift;
	};

	// Private Member Variable
    uint16_t usColBytes;
    uint16_t usTotalBytes;

    uint8_t ucPatternSize;
    uint8_t ucPatternCount;
    PIF_stDotMatrixPattern *pstPattern;
    uint8_t *pucPattern;

	uint8_t ucRowIndex;
	uint16_t usPositionX;
	uint16_t usPositionY;
	uint16_t usShiftCount;

    uint16_t usControlPeriodMs;
	uint16_t usPretimeMs;

	PIF_stPulseItem *pstTimerBlink;
	PIF_stPulseItem *pstTimerShift;

	PIF_enTaskLoop enTaskLoop;

	// Private Member Function
   	PIF_actDotMatrixDisplay actDisplay;
} PIF_stDotMatrixBase;


static PIF_stDotMatrixBase *s_pstDotMatrixBase = NULL;
static uint8_t s_ucDotMatrixBaseSize;
static uint8_t s_ucDotMatrixBasePos;

static PIF_stPulse *s_pstDotMatrixTimer;


static void _SetPattern(PIF_stDotMatrixBase *pstBase, uint16_t usPositionX, uint16_t usPositionY, uint16_t usPosition)
{
    PIF_stDotMatrixPattern *pstPattern = &pstBase->pstPattern[pstBase->ucPatternIndex];
    uint8_t *pucSrc, *pucDst, shift, col, row;

	shift = usPositionX & 7;
	pucSrc = pstPattern->pucPattern + usPositionY * pstPattern->ucColBytes + usPositionX / 8;
	pucDst = pstBase->pucPattern + usPosition;
    if (shift) {
    	uint8_t rest = 8 - shift;
    	uint8_t mask = ~((1 << rest) - 1);
    	for (row = 0; row < pstBase->usRowSize; row++) {
    		for (col = 0; col < pstBase->usColBytes; col++) {
    			pucDst[col] = (pucSrc[col] >> shift) + ((pucSrc[col + 1] << rest) & mask);
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstBase->usColBytes;
    	}
    }
    else {
    	for (row = 0; row < pstBase->usRowSize; row++) {
    		for (col = 0; col < pstBase->usColBytes; col++) {
    			pucDst[col] = pucSrc[col];
    		}
        	pucSrc += pstPattern->ucColBytes;
        	pucDst += pstBase->usColBytes;
    	}
    }
}

static void _evtTimerBlinkFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pvIssuer;
    pstBase->btBlink ^= 1;
}

static void _evtTimerShiftFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pvIssuer;

    switch (pstBase->btShift.btDirection) {
    case DMS_enLeft:
        if (pstBase->usPositionX < pstBase->pstPattern[pstBase->ucPatternIndex].ucColSize - pstBase->usColSize) {
        	pstBase->usPositionX++;
        }
        else if (pstBase->btShift.btMethod == DMS_btPingPongHor) {
        	pstBase->btShift.btDirection = DMS_enRight;
		}
        else if (pstBase->btShift.btMethod == DMS_btRepeatHor) {
        	pstBase->usPositionX = 0;
		}
		else {
			pifPulse_StopItem(pstBase->pstTimerShift);
			if (pstBase->stOwner.evtShiftFinish) {
				(*pstBase->stOwner.evtShiftFinish)(pstBase->stOwner.unDeviceCode);
			}
        }
    	break;

    case DMS_enRight:
        if (pstBase->usPositionX) {
        	pstBase->usPositionX--;
        }
        else if (pstBase->btShift.btMethod == DMS_btPingPongHor) {
        	pstBase->btShift.btDirection = DMS_enLeft;
		}
        else if (pstBase->btShift.btMethod == DMS_btRepeatHor) {
        	pstBase->usPositionX = pstBase->pstPattern[pstBase->ucPatternIndex].ucColSize - pstBase->usColSize;
		}
		else {
			pifPulse_StopItem(pstBase->pstTimerShift);
			if (pstBase->stOwner.evtShiftFinish) {
				(*pstBase->stOwner.evtShiftFinish)(pstBase->stOwner.unDeviceCode);
			}
		}
    	break;

    case DMS_enUp:
        if (pstBase->usPositionY < pstBase->pstPattern[pstBase->ucPatternIndex].ucRowSize - pstBase->usRowSize) {
        	pstBase->usPositionY++;
        }
        else if (pstBase->btShift.btMethod == DMS_btPingPongVer) {
        	pstBase->btShift.btDirection = DMS_enDown;
		}
        else if (pstBase->btShift.btMethod == DMS_btRepeatVer) {
        	pstBase->usPositionY = 0;
		}
		else {
			pifPulse_StopItem(pstBase->pstTimerShift);
			if (pstBase->stOwner.evtShiftFinish) {
				(*pstBase->stOwner.evtShiftFinish)(pstBase->stOwner.unDeviceCode);
			}
        }
    	break;

    case DMS_enDown:
        if (pstBase->usPositionY) {
        	pstBase->usPositionY--;
        }
        else if (pstBase->btShift.btMethod == DMS_btPingPongVer) {
        	pstBase->btShift.btDirection = DMS_enUp;
		}
        else if (pstBase->btShift.btMethod == DMS_btRepeatVer) {
        	pstBase->usPositionY = pstBase->pstPattern[pstBase->ucPatternIndex].ucRowSize - pstBase->usRowSize;
		}
		else {
			pifPulse_StopItem(pstBase->pstTimerShift);
			if (pstBase->stOwner.evtShiftFinish) {
				(*pstBase->stOwner.evtShiftFinish)(pstBase->stOwner.unDeviceCode);
			}
		}
    	break;
    }
    _SetPattern(pstBase, pstBase->usPositionX, pstBase->usPositionY, 0);

    if (pstBase->usShiftCount) {
    	pstBase->usShiftCount--;
		if (!pstBase->usShiftCount) {
			pifPulse_StopItem(pstBase->pstTimerShift);
		}
    }
}

static void _TaskCommon(PIF_stDotMatrixBase *pstBase)
{
	uint8_t *pucPattern;
	uint8_t ucOff = 0;

	if (!pstBase->btRun) return;

	if (pif_usTimer1ms > pstBase->usPretimeMs) {
		if (pif_usTimer1ms - pstBase->usPretimeMs < pstBase->usControlPeriodMs) return;
	}
	else if (pif_usTimer1ms < pstBase->usPretimeMs) {
		if (1000 - pstBase->usPretimeMs + pif_usTimer1ms < pstBase->usControlPeriodMs) return;
	}
	else return;

	if (!pstBase->btBlink) {
		pucPattern = pstBase->pucPattern;
		int index = pstBase->ucRowIndex * pstBase->usColBytes;
		(*pstBase->actDisplay)(pstBase->ucRowIndex, pucPattern + index);
		pucPattern += pstBase->usTotalBytes;
	}
	else {
		(*pstBase->actDisplay)(pstBase->ucRowIndex, &ucOff);
	}
	pstBase->ucRowIndex++;
	if (pstBase->ucRowIndex >= pstBase->usRowSize) pstBase->ucRowIndex = 0;

	pstBase->usPretimeMs = pif_usTimer1ms;
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

    s_pstDotMatrixBase = calloc(sizeof(PIF_stDotMatrixBase), ucSize);
    if (!s_pstDotMatrixBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucDotMatrixBaseSize = ucSize;
    s_ucDotMatrixBasePos = 0;

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
	PIF_stDotMatrixBase *pstBase;

    if (s_pstDotMatrixBase) {
		for (int i = 0; i < s_ucDotMatrixBaseSize; i++) {
			pstBase = &s_pstDotMatrixBase[i];
			if (pstBase->pucPattern) {
				free(pstBase->pucPattern);
				pstBase->pucPattern = NULL;
			}
			if (pstBase->pstPattern) {
				free(pstBase->pstPattern);
				pstBase->pstPattern = NULL;
			}
		}
    	free(s_pstDotMatrixBase);
        s_pstDotMatrixBase = NULL;
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
    if (s_ucDotMatrixBasePos >= s_ucDotMatrixBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!usColSize || !usRowSize || !actDisplay) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDotMatrixBase *pstBase = &s_pstDotMatrixBase[s_ucDotMatrixBasePos];

    pstBase->usColSize = usColSize;
    pstBase->usRowSize = usRowSize;
    pstBase->usColBytes = (pstBase->usColSize - 1) / 8 + 1;
    pstBase->usTotalBytes = pstBase->usColBytes * pstBase->usRowSize;

    pstBase->pucPattern = calloc(sizeof(uint8_t), pstBase->usTotalBytes);
    if (!pstBase->pucPattern) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    pstBase->stOwner.unDeviceCode = unDeviceCode;
    pstBase->usControlPeriodMs = PIF_DM_CONTROL_PERIOD_DEFAULT;
    pstBase->actDisplay = actDisplay;
    pstBase->pstTimerBlink = NULL;
    pstBase->pstTimerShift = NULL;

    s_ucDotMatrixBasePos = s_ucDotMatrixBasePos + 1;
    return &pstBase->stOwner;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:AddSingle(D:%u UC:%u UR:%u) EC:%d", unDeviceCode,
			usColSize, usRowSize, pif_enError);
    return NULL;
}

/**
 * @fn pifDotMatrix_GetControlPeriod
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifDotMatrix_GetControlPeriod(PIF_stDotMatrix *pstOwner)
{
    return ((PIF_stDotMatrixBase *)pstOwner)->usControlPeriodMs;
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

    ((PIF_stDotMatrixBase *)pstOwner)->usControlPeriodMs = usPeriodMs;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (pstBase->pstPattern) free(pstBase->pstPattern);

	pstBase->pstPattern = calloc(sizeof(PIF_stDotMatrixPattern), ucSize);
    if (!pstBase->pstPattern) {
		pif_enError = E_enOutOfHeap;
        goto fail;
	}
    pstBase->ucPatternSize = ucSize;
    pstBase->ucPatternCount = 0;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (pstBase->ucPatternCount >= pstBase->ucPatternSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucColSize || !ucRowSize || ucColSize < pstBase->usColSize || ucRowSize < pstBase->usRowSize || !pucPattern) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDotMatrixPattern *pstPattern = &pstBase->pstPattern[pstBase->ucPatternCount];

    pstPattern->ucColSize = ucColSize;
    pstPattern->ucColBytes = (ucColSize - 1) / 8 + 1;
    pstPattern->ucRowSize = ucRowSize;
    pstPattern->pucPattern = pucPattern;

    pstBase->ucPatternCount = pstBase->ucPatternCount + 1;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

    _SetPattern(pstBase, pstBase->usPositionX, pstBase->usPositionY, 0);
    pstBase->btRun = TRUE;
}

/**
 * @fn pifDotMatrix_Stop
 * @brief
 * @param pstOwner
 */
void pifDotMatrix_Stop(PIF_stDotMatrix *pstOwner)
{
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;
	uint16_t col, row;
	uint8_t ucOff = 0;

	for (row = 0; row < pstBase->usRowSize; row++) {
		for (col = 0; col < pstBase->usColSize; col += 8) {
			(*pstBase->actDisplay)(row, &ucOff);
		}
	}
	pstBase->btRun = FALSE;
    if (pstBase->btBlink) {
		pifPulse_StopItem(pstBase->pstTimerBlink);
		pstBase->btBlink = FALSE;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (ucPatternIndex >= pstBase->ucPatternSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	pstBase->ucPatternIndex = ucPatternIndex;
    _SetPattern(pstBase, pstBase->usPositionX, pstBase->usPositionY, 0);
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (!usPeriodMs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstBase->pstTimerBlink) {
		pstBase->pstTimerBlink = pifPulse_AddItem(s_pstDotMatrixTimer, PT_enRepeat);
		if (!pstBase->pstTimerBlink) return FALSE;
		pifPulse_AttachEvtFinish(pstBase->pstTimerBlink, _evtTimerBlinkFinish, pstBase);
	}
	if (!pifPulse_StartItem(pstBase->pstTimerBlink, usPeriodMs)) return FALSE;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	pstBase->btBlink = FALSE;
	if (pstBase->pstTimerBlink) {
		pifPulse_RemoveItem(s_pstDotMatrixTimer, pstBase->pstTimerBlink);
		pstBase->pstTimerBlink = NULL;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (pstBase->pstTimerBlink) {
		pifPulse_ResetItem(pstBase->pstTimerBlink, usPeriodMs);
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (usX >= pstBase->pstPattern[pstBase->ucPatternIndex].ucColSize - pstBase->usColSize ||
    		usY >= pstBase->pstPattern[pstBase->ucPatternIndex].ucRowSize - pstBase->usRowSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	pstBase->usPositionX = usX;
	pstBase->usPositionY = usY;
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "DotMatrix:SetPosition(X:%u Y:%u) EC:%d", usX, usY, pif_enError);
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (!usPeriodMs || enShift == DMS_enNone) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstBase->pstTimerShift) {
		pstBase->pstTimerShift = pifPulse_AddItem(s_pstDotMatrixTimer, PT_enRepeat);
		if (!pstBase->pstTimerShift) return FALSE;
		pifPulse_AttachEvtFinish(pstBase->pstTimerShift, _evtTimerShiftFinish, pstBase);
	}
	if(!pifPulse_StartItem(pstBase->pstTimerShift, usPeriodMs)) return FALSE;
	pstBase->enShift = enShift;
	pstBase->usShiftCount = usCount;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (pstBase->pstTimerShift) {
		pstBase->enShift = DMS_enNone;
		pifPulse_StopItem(pstBase->pstTimerShift);
		pstBase->usPositionX = 0;
		pstBase->usPositionY = 0;
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
	PIF_stDotMatrixBase *pstBase = (PIF_stDotMatrixBase *)pstOwner;

	if (pstBase->pstTimerShift) {
		pifPulse_ResetItem(pstBase->pstTimerShift, usPeriodMs);
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

	for (int i = 0; i < s_ucDotMatrixBasePos; i++) {
		PIF_stDotMatrixBase *pstBase = &s_pstDotMatrixBase[i];
		if (!pstBase->enTaskLoop) _TaskCommon(pstBase);
	}
}

/**
 * @fn pifDotMatrix_taskEach
 * @brief
 * @param pstTask
 */
void pifDotMatrix_taskEach(PIF_stTask *pstTask)
{
	PIF_stDotMatrixBase *pstBase = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstBase);
	}
}
