#include "pifLog.h"
#include "pifStepMotorBase.h"


static PIF_stStepMotorBase *s_pstStepMotorBase;
static uint8_t s_ucStepMotorBaseSize;
static uint8_t s_ucStepMotorBasePos;

PIF_stPulse *g_pstStepMotorTimer;
uint32_t g_unStepMotorTimerUnit;

const uint16_t c_ausOperation_2P_BP[] = {
		0x02,		// 10 : B
		0x03,		// 11 : B A
		0x01,		// 01 :   A
		0x00		// 00 :
};

const uint16_t c_ausOperation_4P_UP_1Phase[] = {
		0x01,		// 00 01 :     A
		0x04,		// 01 00 :  B
		0x02,		// 00 10 :    ~A
		0x08		// 10 00 : ~B
};

const uint16_t c_ausOperation_4P_UP_2Phase[] = {
		0x05,		// 01 01 :  B  A
		0x06,		// 01 10 :  B ~A
		0x0A,		// 10 10 : ~B ~A
		0x09		// 10 01 : ~B  A
};

const uint16_t c_ausOperation_4P_UP_12Phase[] = {
		0x01,		// 00 01 :     A
		0x05,		// 01 01 :  B  A
		0x04,		// 01 00 :  B
		0x06,		// 01 10 :  B ~A
		0x02,		// 00 10 :    ~A
		0x0A,		// 10 10 : ~B ~A
		0x08,		// 10 00 : ~B
		0x09		// 10 01 : ~B  A
};

const uint16_t c_ausOperation_5P_PG[] = {
		0x16,		// 1 0 1 1 0 : E   C B
		0x12,		// 1 0 0 1 0 : E     B
		0x1A,		// 1 1 0 1 0 : E D   B
		0x0A,		// 0 1 0 1 0 :   D   B
		0x0B,		// 0 1 0 1 1 :   D   B A
		0x09,		// 0 1 0 0 1 :   D     A
		0x0D,		// 0 1 1 0 1 :   D C   A
		0x05,		// 0 0 1 0 1 :     C   A
		0x15,		// 1 0 1 0 1 : E   C   A
		0x14		// 1 0 1 0 0 : E   C
};


static void _SetStep(PIF_stStepMotorBase *pstBase)
{
	if (((PIF_stStepMotor *)pstBase)->ucDirection == 0) {
		pstBase->usCurrentStep++;
		if (pstBase->usCurrentStep == pstBase->ucStepSize) {
			pstBase->usCurrentStep = 0;
		}
	}
	else{
		if (pstBase->usCurrentStep == 0) {
			pstBase->usCurrentStep = pstBase->ucStepSize;
		}
		pstBase->usCurrentStep--;
	}

	(*pstBase->actSetStep)(pstBase->pusPhaseOperation[pstBase->usCurrentStep]);
}

static void _TimerStepFinish(void *pvIssuer)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pvIssuer;

    _SetStep(pstBase);

	if (pstBase->unStepTarget) {
		pstBase->unStepCount++;
		if (pstBase->unStepCount >= pstBase->unStepTarget) {
			pifPulse_StopItem(pstBase->pstTimerStep);
			if (pstBase->fnStopStep) (*pstBase->fnStopStep)(pstBase);
		}
	}
}

static void _TimerControlFinish(void *pvIssuer)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pvIssuer;
    PIF_stStepMotor *pstOwner = &pstBase->stOwner;

	(*pstBase->fnControl)(pstBase);

	if (pstOwner->enState == MS_enStop) {
		pifPulse_StopItem(pstBase->pstTimerControl);
		pstOwner->enState = MS_enIdle;
		if (pstBase->evtStop) (*pstBase->evtStop)(pstOwner, pstBase->pvInfo);
	}
}

static void _TimerBreakFinish(void *pvIssuer)
{
	pifStepMotor_Release(&((PIF_stStepMotorBase *)pvIssuer)->stOwner);
}

static void _TaskCommon(PIF_stTask *pstTask, PIF_stStepMotorBase *pstBase)
{
	static uint16_t usnStepPeriodUs = 0;

	_SetStep(pstBase);

	if (pstBase->unStepTarget) {
		pstBase->unStepCount++;
		if (pstBase->unStepCount >= pstBase->unStepTarget) {
			if (pstBase->fnStopStep) (*pstBase->fnStopStep)(pstBase);
		}
	}

	if (usnStepPeriodUs != pstBase->usStepPeriodUs) {
		pifTask_SetPeriod(pstTask, pstBase->usStepPeriodUs);
		usnStepPeriodUs = pstBase->usStepPeriodUs;
	}
}

/**
 * @fn pifStepMotor_Init
 * @brief 
 * @param pstTimer
 * @param ucSize
 * @return 
 */
BOOL pifStepMotor_Init(PIF_stPulse *pstTimer, uint32_t unTimerUnit, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstStepMotorBase = calloc(sizeof(PIF_stStepMotorBase), ucSize);
    if (!s_pstStepMotorBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucStepMotorBaseSize = ucSize;
    s_ucStepMotorBasePos = 0;

    g_pstStepMotorTimer = pstTimer;
    g_unStepMotorTimerUnit = unTimerUnit;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u S:%u EC:%d", __LINE__, ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifStepMotor_Exit
 * @brief 
 */
void pifStepMotor_Exit()
{
    if (s_pstStepMotorBase) {
		for (int i = 0; i < s_ucStepMotorBaseSize; i++) {
			PIF_stStepMotorBase *pstBase = &s_pstStepMotorBase[i];
			if (pstBase->pvInfo) {
				free(pstBase->pvInfo);
				pstBase->pvInfo = NULL;
			}
		}
        free(s_pstStepMotorBase);
        s_pstStepMotorBase = NULL;
    }
}

/**
 * @fn pifStepMotor_Add
 * @brief 
 * @param usPifId
 * @param usResolution
 * @param enOperation
 * @return 
 */
PIF_stStepMotor *pifStepMotor_Add(PIF_usId usPifId, uint16_t usResolution, PIF_enStepMotorOperation enOperation)
{
    PIF_stStepMotorBase *pstBase = NULL;

    if (s_ucStepMotorBasePos >= s_ucStepMotorBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    pstBase = &s_pstStepMotorBase[s_ucStepMotorBasePos];
    PIF_stStepMotor *pstOwner = &pstBase->stOwner;

	pstBase->pstTimerStep = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
    if (!pstBase->pstTimerStep) return FALSE;
    pifPulse_AttachEvtFinish(pstBase->pstTimerStep, _TimerStepFinish, pstBase);

	pifStepMotor_SetOperation(pstOwner, enOperation);

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->usPifId = usPifId;
    pstOwner->usResolution = usResolution;
    pstOwner->ucReductionGearRatio = 1;
    pstBase->usStepPeriodUs = 1000;
    pstBase->pstTimerControl = NULL;
    pstOwner->enState = MS_enIdle;

    s_ucStepMotorBasePos = s_ucStepMotorBasePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) R:%u O:%u EC:%d", __LINE__, usPifId, usResolution, enOperation, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifStepMotor_AttachAction
 * @brief
 * @param pstOwner
 * @param actSetStep
 */
void pifStepMotor_AttachAction(PIF_stStepMotor *pstOwner, PIF_actStepMotorSetStep actSetStep)
{
    ((PIF_stStepMotorBase *)pstOwner)->actSetStep = actSetStep;
}

/**
 * @fn pifStepMotor_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtStable
 * @param evtStop
 * @param evtError
 */
void pifStepMotor_AttachEvent(PIF_stStepMotor *pstOwner, PIF_evtStepMotorStable evtStable, PIF_evtStepMotorStop evtStop,
		PIF_evtStepMotorError evtError)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    pstBase->evtStable = evtStable;
    pstBase->evtStop = evtStop;
    pstBase->evtError = evtError;
}

/**
 * @fn pifStepMotor_AttachTask
 * @brief
 * @param pstOwner
 * @param pstTask
 */
void pifStepMotor_AttachTask(PIF_stStepMotor *pstOwner, PIF_stTask *pstTask)
{
    pifTask_Pause(pstTask);
    ((PIF_stStepMotorBase *)pstOwner)->pstTask = pstTask;
}

/**
 * @fn pifStepMotor_SetDirection
 * @brief
 * @param pstOwner
 * @param enMethod
 * @return
 */
void pifStepMotor_SetDirection(PIF_stStepMotor *pstOwner, uint8_t ucDirection)
{
    pstOwner->ucDirection = ucDirection;
}

/**
 * @fn pifStepMotor_SetMethod
 * @brief
 * @param pstOwner
 * @param enMethod
 * @return
 */
BOOL pifStepMotor_SetMethod(PIF_stStepMotor *pstOwner, PIF_enStepMotorMethod enMethod)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

	if (pstOwner->enMethod == enMethod) return TRUE;

	if (pstOwner->enMethod == SMM_enTimer) {
	    if (pstBase->pstTimerStep) {
	    	pifPulse_RemoveItem(g_pstStepMotorTimer, pstBase->pstTimerStep);
	    	pstBase->pstTimerStep = NULL;
	    }
	}
	if (enMethod == SMM_enTimer) {
		pstBase->pstTimerStep = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
	    if (!pstBase->pstTimerStep) return FALSE;
	    pifPulse_AttachEvtFinish(pstBase->pstTimerStep, _TimerStepFinish, pstBase);
	}

	pstOwner->enMethod = enMethod;
	return TRUE;
}

/**
 * @fn pifStepMotor_SetOperation
 * @brief
 * @param pstOwner
 * @param enOperation
 * @return
 */
BOOL pifStepMotor_SetOperation(PIF_stStepMotor *pstOwner, PIF_enStepMotorOperation enOperation)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

	if (pifPulse_GetStep(pstBase->pstTimerStep) != PS_enStop) {
        pif_enError = E_enInvalidState;
        goto fail;
	}

	switch (enOperation) {
	case SMO_en2P_BP:
		pstBase->ucStepSize = 2;
		pstBase->pusPhaseOperation = c_ausOperation_2P_BP;
		break;

	case SMO_en4P_UP_1Phase:
		pstBase->ucStepSize = 4;
		pstBase->pusPhaseOperation = c_ausOperation_4P_UP_1Phase;
		break;

	case SMO_en4P_UP_2Phase:
		pstBase->ucStepSize = 4;
		pstBase->pusPhaseOperation = c_ausOperation_4P_UP_2Phase;
		break;

	case SMO_en4P_UP_12Phase:
		pstBase->ucStepSize = 8;
		pstBase->pusPhaseOperation = c_ausOperation_4P_UP_12Phase;
		break;

	case SMO_en5P_PG:
		pstBase->ucStepSize = 10;
		pstBase->pusPhaseOperation = c_ausOperation_5P_PG;
		break;

	default:
        pif_enError = E_enInvalidParam;
		return FALSE;
	}
	pstOwner->enOperation = enOperation;
	pstOwner->fCurrentRpm = 0;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "StepMotor:SetOperation(O:%d) EC:%d", enOperation, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_SetPps
 * @brief
 * @param pstOwner
 * @param usPps
 * @return
 */
BOOL pifStepMotor_SetPps(PIF_stStepMotor *pstOwner, uint16_t usPps)
{
	return pifStepMotor_SetRpm(pstOwner, 60.0 * usPps / pstOwner->usResolution);
}

/**
 * @fn pifStepMotor_SetRpm
 * @brief
 * @param pstOwner
 * @param unRpm
 * @return
 */
BOOL pifStepMotor_SetRpm(PIF_stStepMotor *pstOwner, float fRpm)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    if (!fRpm) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	uint16_t period = 60.0 * 1000 * 1000 / (pstOwner->usResolution * fRpm * pstOwner->ucReductionGearRatio) + 0.5;
	if (pstOwner->enOperation == SMO_en4P_UP_12Phase) {
		period /= 2;
	}

	switch (pstOwner->enMethod) {
	case SMM_enTimer:
	    if (period < 2 * g_unStepMotorTimerUnit) {
	        pif_enError = E_enWrongData;
	        goto fail;
	    }
		pifPulse_ResetItem(pstBase->pstTimerStep, period / g_unStepMotorTimerUnit);
		break;

	case SMM_enTask:
	    if (!period) {
	        pif_enError = E_enWrongData;
	        goto fail;
	    }
		pifTask_SetPeriod(pstBase->pstTask, period);
		break;
	}

	pstOwner->fCurrentRpm = fRpm;
	pstBase->usStepPeriodUs = period;

	if (pif_stLogFlag.btStepMotor) {
		pifLog_Printf(LT_enInfo, "StepMotor:SetRpm DC:%u RPM:%2f Period:%u", pstOwner->usPifId,
				pstOwner->fCurrentRpm, pstBase->usStepPeriodUs);
	}
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "StepMotor:SetRpm(R:%2f) EC:%d", fRpm, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_SetTargetStep
 * @brief
 * @param pstOwner
 * @param unTargetStep
 */
void pifStepMotor_SetTargetStep(PIF_stStepMotor *pstOwner, uint32_t unTargetStep)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    pstBase->unStepTarget = unTargetStep;
    pstBase->unStepCount = 0;
}

/**
 * @fn pifStepMotor_Start
 * @brief
 * @param pstOwner
 * @param unTargetStep
 * @return
 */
BOOL pifStepMotor_Start(PIF_stStepMotor *pstOwner, uint32_t unTargetStep)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    switch (pstOwner->enMethod) {
    case SMM_enTimer:
        if (pstBase->usStepPeriodUs < 2 * g_unStepMotorTimerUnit) {
            pif_enError = E_enInvalidParam;
            goto fail;
        }
    	if (!pifPulse_StartItem(pstBase->pstTimerStep, pstBase->usStepPeriodUs / g_unStepMotorTimerUnit)) goto fail;
    	break;

    case SMM_enTask:
    	if (!pstBase->usStepPeriodUs || !pstBase->pstTask) {
            pif_enError = E_enInvalidParam;
        	goto fail;
    	}
		pifTask_Restart(pstBase->pstTask);
    	break;
	}
    pstBase->unStepTarget = unTargetStep;
    pstBase->unStepCount = 0;
	return TRUE;

fail:
	pifLog_Printf(LT_enError, "StepMotor:Start(T:%u) EC:%d", unTargetStep, pif_enError);
	return FALSE;
}

/**
 * @fn pifStepMotor_Break
 * @brief
 * @param pstOwner
 */
void pifStepMotor_Break(PIF_stStepMotor *pstOwner)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    switch (pstOwner->enMethod) {
    case SMM_enTimer:
    	pifPulse_StopItem(pstBase->pstTimerStep);
    	break;

    case SMM_enTask:
        pifTask_Pause(pstBase->pstTask);
        break;
    }
}

/**
 * @fn pifStepMotor_Release
 * @brief
 * @param pstOwner
 */
void pifStepMotor_Release(PIF_stStepMotor *pstOwner)
{
	(*((PIF_stStepMotorBase *)pstOwner)->actSetStep)(0);
}

/**
 * @fn pifStepMotor_BreakRelease
 * @brief
 * @param pstOwner
 * @param usBreakTime
 */
void pifStepMotor_BreakRelease(PIF_stStepMotor *pstOwner, uint16_t usBreakTime)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

	pifStepMotor_Break(pstOwner);

	if (usBreakTime) {
	    if (!pstBase->pstTimerBreak) {
	    	pstBase->pstTimerBreak = pifPulse_AddItem(g_pstStepMotorTimer, PT_enOnce);
	    }
	    if (pstBase->pstTimerBreak) {
	    	pifPulse_AttachEvtFinish(pstBase->pstTimerBreak, _TimerBreakFinish, pstBase);
			if (pifPulse_StartItem(pstBase->pstTimerBreak, usBreakTime)) return;
	    }
	}

	pifStepMotor_Release(pstOwner);
}

/**
 * @fn pifStepMotor_InitControl
 * @brief
 * @param pstOwner
 * @param usControlPeriodMs
 * @return
 */
BOOL pifStepMotor_InitControl(PIF_stStepMotor *pstOwner, uint16_t usControlPeriodMs)
{
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

	pstBase->pstTimerControl = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
    if (!pstBase->pstTimerControl) return FALSE;

    pstBase->usControlPeriodMs = usControlPeriodMs;

	pifPulse_AttachEvtFinish(pstBase->pstTimerControl, _TimerControlFinish, pstBase);
	return TRUE;
}

/**
 * @fn pifStepMotor_StartControl
 * @brief 
 * @param pstOwner
 * @return 
 */
BOOL pifStepMotor_StartControl(PIF_stStepMotor *pstOwner)
{
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

	if (!pstBase->usStepPeriodUs || !pstBase->fnControl) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (pstOwner->enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

    return pifPulse_StartItem(pstBase->pstTimerControl, pstBase->usControlPeriodMs);

fail:
#ifndef __PIF_NO_LOG__
    pifLog_Printf(LT_enError, "SM:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, pstOwner->enState, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifStepMotor_taskAll
 * @brief
 * @param pstTask
 */
void pifStepMotor_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucStepMotorBasePos; i++) {
		PIF_stStepMotorBase *pstBase = &s_pstStepMotorBase[i];
		if (!pstBase->enTaskLoop) _TaskCommon(pstTask, pstBase);
	}
}

/**
 * @fn pifStepMotor_taskEach
 * @brief
 * @param pstTask
 */
void pifStepMotor_taskEach(PIF_stTask *pstTask)
{
	PIF_stStepMotorBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstTask, pstBase);
	}
}
