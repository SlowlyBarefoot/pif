#include "pifLog.h"
#include "pifStepMotor.h"


static PIF_stStepMotor *s_pstStepMotor;
static uint8_t s_ucStepMotorSize;
static uint8_t s_ucStepMotorPos;

PIF_stPulse *g_pstStepMotorTimer;

const uint16_t c_ausOperation_2P_2W[] = {
		0x02,		// 10 : B
		0x03,		// 11 : B A
		0x01,		// 01 :   A
		0x00		// 00 :
};

const uint16_t c_ausOperation_2P_4W_1S[] = {
		0x01,		// 00 01 :     A
		0x04,		// 01 00 :  B
		0x02,		// 00 10 :    ~A
		0x08		// 10 00 : ~B
};

const uint16_t c_ausOperation_2P_4W_2S[] = {
		0x05,		// 01 01 :  B  A
		0x06,		// 01 10 :  B ~A
		0x0A,		// 10 10 : ~B ~A
		0x09		// 10 01 : ~B  A
};

const uint16_t c_ausOperation_2P_4W_1_2S[] = {
		0x01,		// 00 01 :     A
		0x05,		// 01 01 :  B  A
		0x04,		// 01 00 :  B
		0x06,		// 01 10 :  B ~A
		0x02,		// 00 10 :    ~A
		0x0A,		// 10 10 : ~B ~A
		0x08,		// 10 00 : ~B
		0x09		// 10 01 : ~B  A
};

#if 0
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
#endif


static void _SetStep(PIF_stStepMotor *pstOwner)
{
    if (pstOwner->_ucDirection == 0) {
		pstOwner->__ucCurrentStep++;
		if (pstOwner->__ucCurrentStep == pstOwner->__ucStepSize) {
			pstOwner->__ucCurrentStep = 0;
		}
	}
	else{
		if (pstOwner->__ucCurrentStep == 0) {
			pstOwner->__ucCurrentStep = pstOwner->__ucStepSize;
		}
		pstOwner->__ucCurrentStep--;
	}

	pstOwner->_unCurrentPulse++;

	(*pstOwner->__actSetStep)(pstOwner->__pusPhaseOperation[pstOwner->__ucCurrentStep]);
}

static void _evtTimerStepFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

    _SetStep(pstOwner);

	if (pstOwner->__unTargetPulse) {
		if (pstOwner->_unCurrentPulse >= pstOwner->__unTargetPulse) {
			pifPulse_StopItem(pstOwner->__pstTimerStep);
			if (pstOwner->__fnStopStep) (*pstOwner->__fnStopStep)(pstOwner);
			else if (pstOwner->evtStop) (*pstOwner->evtStop)(pstOwner);
		}
	}
}

static void _evtTimerControlFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(*pstOwner->__fnControl)(pstOwner);

	if (pstOwner->_enState == MS_enStop) {
		pifPulse_StopItem(pstOwner->__pstTimerControl);
		pstOwner->_enState = MS_enIdle;
		if (pstOwner->evtStop) (*pstOwner->evtStop)(pstOwner);
	}
}

static void _evtTimerBreakFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

    if (pstOwner->_enState > MS_enIdle && pstOwner->_enState < MS_enReduce) {
    	pstOwner->_enState = MS_enReduce;
    }
    else {
    	pifStepMotor_Release(pstOwner);
    }
}

static void _taskCommon(PIF_stTask *pstTask, PIF_stStepMotor *pstOwner)
{
	static uint16_t usnStepPeriodUs = 0;

	_SetStep(pstOwner);

	if (pstOwner->__unTargetPulse) {
		if (pstOwner->_unCurrentPulse >= pstOwner->__unTargetPulse) {
			pstOwner->__pstTask->bPause = TRUE;
			if (pstOwner->__fnStopStep) (*pstOwner->__fnStopStep)(pstOwner);
			else if (pstOwner->evtStop) (*pstOwner->evtStop)(pstOwner);
		}
	}

	if (usnStepPeriodUs != pstOwner->__usStepPeriodUs) {
		pifTask_SetPeriod(pstTask, pstOwner->__usStepPeriodUs);
		usnStepPeriodUs = pstOwner->__usStepPeriodUs;
	}
}

/**
 * @fn pifStepMotor_Init
 * @brief 
 * @param pstTimer
 * @param ucSize
 * @return 
 */
BOOL pifStepMotor_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstStepMotor = calloc(sizeof(PIF_stStepMotor), ucSize);
    if (!s_pstStepMotor) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucStepMotorSize = ucSize;
    s_ucStepMotorPos = 0;

    g_pstStepMotorTimer = pstTimer;
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
    if (s_pstStepMotor) {
		for (int i = 0; i < s_ucStepMotorSize; i++) {
			PIF_stStepMotor *pstOwner = &s_pstStepMotor[i];
			if (pstOwner->_pvChild) {
				free(pstOwner->_pvChild);
				pstOwner->_pvChild = NULL;
			}
			if (pstOwner->__pstTimerStep) {
				pifPulse_RemoveItem(g_pstStepMotorTimer, pstOwner->__pstTimerStep);
			}
			if (pstOwner->__pstTimerControl) {
				pifPulse_RemoveItem(g_pstStepMotorTimer, pstOwner->__pstTimerControl);
			}
			if (pstOwner->__pstTimerBreak) {
				pifPulse_RemoveItem(g_pstStepMotorTimer, pstOwner->__pstTimerBreak);
			}
			if (pstOwner->__pstTimerDelay) {
				pifPulse_RemoveItem(g_pstStepMotorTimer, pstOwner->__pstTimerDelay);
			}
		}
        free(s_pstStepMotor);
        s_pstStepMotor = NULL;
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
    PIF_stStepMotor *pstOwner = NULL;

    if (s_ucStepMotorPos >= s_ucStepMotorSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    pstOwner = &s_pstStepMotor[s_ucStepMotorPos];

	pstOwner->__pstTimerStep = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
    if (!pstOwner->__pstTimerStep) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerStep, _evtTimerStepFinish, pstOwner);

	pifStepMotor_SetOperation(pstOwner, enOperation);

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->_usResolution = usResolution;
    pstOwner->_ucReductionGearRatio = 1;
    pstOwner->__usStepPeriodUs = 1000;
    pstOwner->__pstTimerControl = NULL;
    pstOwner->_enState = MS_enIdle;

    s_ucStepMotorPos = s_ucStepMotorPos + 1;
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
    pstOwner->__actSetStep = actSetStep;
}

/**
 * @fn pifStepMotor_AttachTask
 * @brief
 * @param pstOwner
 * @param pstTask
 */
void pifStepMotor_AttachTask(PIF_stStepMotor *pstOwner, PIF_stTask *pstTask)
{
    pstTask->bPause = TRUE;
    pstOwner->__pstTask = pstTask;
}

/**
 * @fn pifStepMotor_SetDirection
 * @brief
 * @param pstOwner
 * @param ucDirection
 * @return
 */
BOOL pifStepMotor_SetDirection(PIF_stStepMotor *pstOwner, uint8_t ucDirection)
{
	if (pstOwner->_enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

	pstOwner->_ucDirection = ucDirection;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) S:%u EC:%d", __LINE__, pstOwner->_usPifId, pstOwner->_enState, pif_enError);
#endif
	return FALSE;
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
	if (pstOwner->_enMethod == enMethod) return TRUE;

	if (pstOwner->_enMethod == SMM_enTimer) {
	    if (pstOwner->__pstTimerStep) {
	    	pifPulse_RemoveItem(g_pstStepMotorTimer, pstOwner->__pstTimerStep);
	    	pstOwner->__pstTimerStep = NULL;
	    }
	}
	if (enMethod == SMM_enTimer) {
		pstOwner->__pstTimerStep = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
	    if (!pstOwner->__pstTimerStep) return FALSE;
	    pifPulse_AttachEvtFinish(pstOwner->__pstTimerStep, _evtTimerStepFinish, pstOwner);
	}

	pstOwner->_enMethod = enMethod;
	return TRUE;
}

/**
 * @fn pifStepMotor_SetOperatingTime
 * @brief
 * @param pstOwner
 * @param unOperatingTime
 * @return
 */
BOOL pifStepMotor_SetOperatingTime(PIF_stStepMotor *pstOwner, uint32_t unOperatingTime)
{
	if (!pstOwner->__pstTimerBreak) {
		pstOwner->__pstTimerBreak = pifPulse_AddItem(g_pstStepMotorTimer, PT_enOnce);
	}
	if (pstOwner->__pstTimerBreak) {
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _evtTimerBreakFinish, pstOwner);
		if (pifPulse_StartItem(pstOwner->__pstTimerBreak, unOperatingTime)) {
			return TRUE;
		}
	}
	return FALSE;
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
    if (pstOwner->_enMethod == SMM_enTimer) {
		if (pstOwner->__pstTimerStep->_enStep != PS_enStop) {
			pif_enError = E_enInvalidState;
			goto fail;
		}
    }

	switch (enOperation) {
	case SMO_en2P_2W:
		pstOwner->__ucStepSize = 4;
		pstOwner->__pusPhaseOperation = c_ausOperation_2P_2W;
		break;

	case SMO_en2P_4W_1S:
		pstOwner->__ucStepSize = 4;
		pstOwner->__pusPhaseOperation = c_ausOperation_2P_4W_1S;
		break;

	case SMO_en2P_4W_2S:
		pstOwner->__ucStepSize = 4;
		pstOwner->__pusPhaseOperation = c_ausOperation_2P_4W_2S;
		break;

	case SMO_en2P_4W_1_2S:
		pstOwner->__ucStepSize = 8;
		pstOwner->__pusPhaseOperation = c_ausOperation_2P_4W_1_2S;
		break;

#if 0
	case SMO_en5P_PG:
		pstOwner->__ucStepSize = 10;
		pstOwner->__pusPhaseOperation = c_ausOperation_5P_PG;
		break;
#endif

	default:
        pif_enError = E_enInvalidParam;
		return FALSE;
	}
	pstOwner->_enOperation = enOperation;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) O:%u EC:%d", __LINE__, pstOwner->_usPifId, enOperation, pif_enError);
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
    if (!usPps) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	uint16_t period = 1000000.0 / (usPps * pstOwner->_ucReductionGearRatio) + 0.5;
	if (pstOwner->_enOperation == SMO_en2P_4W_1_2S) {
		period /= 2;
	}

	switch (pstOwner->_enMethod) {
	case SMM_enTimer:
	    if (period < 2 * g_pstStepMotorTimer->_unPeriodUs) {
	        pif_enError = E_enWrongData;
	        goto fail;
	    }
		pstOwner->__pstTimerStep->unTarget = period / g_pstStepMotorTimer->_unPeriodUs;
		break;

	case SMM_enTask:
	    if (!period) {
	        pif_enError = E_enWrongData;
	        goto fail;
	    }
		pifTask_SetPeriod(pstOwner->__pstTask, period);
		break;
	}

	pstOwner->_usCurrentPps = usPps;
	pstOwner->__usStepPeriodUs = period;

#ifndef __PIF_NO_LOG__
	if (pif_stLogFlag.btStepMotor) {
		pifLog_Printf(LT_enInfo, "SM:%u(%u) P/S:%d SP:%uus", __LINE__, pstOwner->_usPifId,
				pstOwner->_usCurrentPps, pstOwner->__usStepPeriodUs);
	}
#endif
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) P/S:%d EC:%d", __LINE__, pstOwner->_usPifId, usPps, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_SetReductionGearRatio
 * @brief
 * @param pstOwner
 * @param ucReductionGearRatio
 * @return
 */
BOOL pifStepMotor_SetReductionGearRatio(PIF_stStepMotor *pstOwner, uint8_t ucReductionGearRatio)
{
	if (pstOwner->_enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

	pstOwner->_ucReductionGearRatio = ucReductionGearRatio;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) S:%u EC:%d", __LINE__, pstOwner->_usPifId, pstOwner->_enState, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_SetRpm
 * @brief
 * @param pstOwner
 * @param fRpm
 * @return
 */
BOOL pifStepMotor_SetRpm(PIF_stStepMotor *pstOwner, float fRpm)
{
	return pifStepMotor_SetPps(pstOwner, fRpm * pstOwner->_usResolution / 60 + 0.5);
}

/**
 * @fn pifStepMotor_GetRpm
 * @brief
 * @param pstOwner
 * @return
 */
float pifStepMotor_GetRpm(PIF_stStepMotor *pstOwner)
{
	return 60.0 * pstOwner->_usCurrentPps / pstOwner->_usResolution;
}

/**
 * @fn pifStepMotor_SetTargetPulse
 * @brief
 * @param pstOwner
 * @param unTargetPulse
 * @return
 */
BOOL pifStepMotor_SetTargetPulse(PIF_stStepMotor *pstOwner, uint32_t unTargetPulse)
{
	if (pstOwner->_enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

    pstOwner->__unTargetPulse = unTargetPulse;
    pstOwner->_unCurrentPulse = 0;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) S:%u EC:%d", __LINE__, pstOwner->_usPifId, pstOwner->_enState, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_Start
 * @brief
 * @param pstOwner
 * @param unTargetPulse
 * @return
 */
BOOL pifStepMotor_Start(PIF_stStepMotor *pstOwner, uint32_t unTargetPulse)
{
    switch (pstOwner->_enMethod) {
    case SMM_enTimer:
        if (pstOwner->__usStepPeriodUs < 2 * g_pstStepMotorTimer->_unPeriodUs) {
            pif_enError = E_enInvalidParam;
            goto fail;
        }
    	if (!pifPulse_StartItem(pstOwner->__pstTimerStep, pstOwner->__usStepPeriodUs / g_pstStepMotorTimer->_unPeriodUs)) goto fail;
    	break;

    case SMM_enTask:
    	if (!pstOwner->__usStepPeriodUs || !pstOwner->__pstTask) {
            pif_enError = E_enInvalidParam;
        	goto fail;
    	}
		pifTask_SetPeriod(pstOwner->__pstTask, pstOwner->__usStepPeriodUs);
		pstOwner->__pstTask->bPause = FALSE;
    	break;
	}
    pstOwner->__unTargetPulse = unTargetPulse;
    pstOwner->_unCurrentPulse = 0;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SM:%u(%u) TS:%u EC:%d", __LINE__, pstOwner->_usPifId, unTargetPulse, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifStepMotor_Break
 * @brief
 * @param pstOwner
 */
void pifStepMotor_Break(PIF_stStepMotor *pstOwner)
{
    switch (pstOwner->_enMethod) {
    case SMM_enTimer:
    	pifPulse_StopItem(pstOwner->__pstTimerStep);
    	break;

    case SMM_enTask:
        pstOwner->__pstTask->bPause = TRUE;
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
	(*pstOwner->__actSetStep)(0);
}

/**
 * @fn pifStepMotor_BreakRelease
 * @brief
 * @param pstOwner
 * @param usBreakTime
 */
void pifStepMotor_BreakRelease(PIF_stStepMotor *pstOwner, uint16_t usBreakTime)
{
	pifStepMotor_Break(pstOwner);

	if (usBreakTime) {
	    if (!pstOwner->__pstTimerBreak) {
	    	pstOwner->__pstTimerBreak = pifPulse_AddItem(g_pstStepMotorTimer, PT_enOnce);
	    }
	    if (pstOwner->__pstTimerBreak) {
	    	pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _evtTimerBreakFinish, pstOwner);
			if (pifPulse_StartItem(pstOwner->__pstTimerBreak, usBreakTime)) return;
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
	pstOwner->__pstTimerControl = pifPulse_AddItem(g_pstStepMotorTimer, PT_enRepeat);
    if (!pstOwner->__pstTimerControl) return FALSE;

    pstOwner->__usControlPeriodMs = usControlPeriodMs;

	pifPulse_AttachEvtFinish(pstOwner->__pstTimerControl, _evtTimerControlFinish, pstOwner);
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
	if (!pstOwner->__usStepPeriodUs || !pstOwner->__fnControl) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (pstOwner->_enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

    return pifPulse_StartItem(pstOwner->__pstTimerControl, pstOwner->__usControlPeriodMs * 1000 / g_pstStepMotorTimer->_unPeriodUs);

fail:
#ifndef __PIF_NO_LOG__
    pifLog_Printf(LT_enError, "SM:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState, pif_enError);
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

	for (int i = 0; i < s_ucStepMotorPos; i++) {
		PIF_stStepMotor *pstOwner = &s_pstStepMotor[i];
		if (!pstOwner->__enTaskLoop) _taskCommon(pstTask, pstOwner);
	}
}

/**
 * @fn pifStepMotor_taskEach
 * @brief
 * @param pstTask
 */
void pifStepMotor_taskEach(PIF_stTask *pstTask)
{
	PIF_stStepMotor *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_taskCommon(pstTask, pstOwner);
	}
}
