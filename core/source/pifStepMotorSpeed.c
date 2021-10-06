#include "pifLog.h"
#include "pifStepMotorSpeed.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enOverRun:
        pstOwner->_enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "SMS:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _fnControlSpeed(PIF_stStepMotor *pstOwner)
{
	uint16_t usTmpPps = 0;
    PIF_stStepMotorSpeed* pstSpeed = (PIF_stStepMotorSpeed*)pstOwner;
    const PIF_stStepMotorSpeedStage *pstStage = pstSpeed->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpPps = pstOwner->_usCurrentPps;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpPps >= pstStage->usFsFixedPps) {
			usTmpPps = pstStage->usFsFixedPps;
			pstOwner->_enState = MS_enConst;
			if (pstOwner->evtStable) (*pstOwner->evtStable)(pstOwner);

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->_enState == MS_enReduce) {
		if (!pstStage->usRsCtrlPps) {
			usTmpPps = 0;
			pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else if (usTmpPps > pstStage->usRsCtrlPps && usTmpPps > pstStage->usRsStopPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = 0;
			pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
	}

	if (usTmpPps != pstOwner->_usCurrentPps) {
		if (usTmpPps) pifStepMotor_SetPps(pstOwner, usTmpPps);
		pstOwner->_usCurrentPps = usTmpPps;
	}

    if (pstOwner->_enState == MS_enBreak) {
    	pifStepMotor_Break(pstOwner);
		if (pstStage->usRsBreakTime &&
				pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usRsBreakTime)) {
			pstOwner->_enState = MS_enBreaking;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
    	}
    	else {
    		pstOwner->_enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
    	}
	}

    if (pstOwner->_enState == MS_enStopping) {
		if (!(pstStage->enMode & MM_NR_enMask)) {
			pifStepMotor_Release(pstOwner);
		}
		pstOwner->_enState = MS_enStop;

		if (pstStage->enMode & MM_CFPS_enMask) {
	    	if (*pstStage->ppstStopSensor) {
	    		if ((*pstStage->ppstStopSensor)->_swCurrState == OFF) {
	    			pstOwner->__ucError = 1;
	    		}
	    	}
	    }

		if (*pstStage->ppstReduceSensor) {
			pifSensor_DetachEvtChange(*pstStage->ppstReduceSensor);
		}
		if (*pstStage->ppstStopSensor) {
			pifSensor_DetachEvtChange(*pstStage->ppstStopSensor);
		}

#ifndef __PIF_NO_LOG__
		nLine = __LINE__;
#endif
    }

#ifndef __PIF_NO_LOG__
	if (nLine && pif_stLogFlag.bt.StepMotor) {
		pifLog_Printf(LT_enInfo, "SMS:%u(%u) %s P/S:%u", nLine, pstOwner->_usPifId, c_cMotorState[pstOwner->_enState], usTmpPps);
	}
#endif
}

static void _evtSwitchReduceChange(PIF_usId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (usLevel) {
		pifStepMotorSpeed_Stop(pstOwner);
	}
}

static void _evtSwitchStopChange(PIF_usId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enBreak) return;

	if (usLevel) {
		pstOwner->_enState = MS_enBreak;
	}
}

/**
 * @fn pifStepMotorSpeed_Create
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param ucResolution
 * @param enOperation
 * @param usControlPeriodMs
 * @return 
 */
PIF_stStepMotor *pifStepMotorSpeed_Create(PIF_usId usPifId, PIF_stPulse* p_timer, uint8_t ucResolution,
		PIF_enStepMotorOperation enOperation, uint16_t usControlPeriodMs)
{
	PIF_stStepMotorSpeed* p_owner = NULL;

	p_owner = calloc(sizeof(PIF_stStepMotorSpeed), 1);
    if (!p_owner) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    PIF_stStepMotor* pstOwner = (PIF_stStepMotor*)&p_owner->parent;
    if (!pifStepMotor_Init(pstOwner, usPifId, p_timer, ucResolution, enOperation)) goto fail;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail;

    pstOwner->__pstTimerDelay = pifPulse_AddItem(pstOwner->_p_timer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _fnControlSpeed;
    return pstOwner;

fail:
	pifStepMotorSpeed_Destroy(&pstOwner);
    return NULL;
}

/**
 * @fn pifStepMotorSpeed_Destroy
 * @brief
 * @param pp_parent
 */
void pifStepMotorSpeed_Destroy(PIF_stStepMotor** pp_parent)
{
    if (*pp_parent) {
    	pifStepMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

/**
 * @fn pifStepMotorSpeed_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifStepMotorSpeed_AddStages(PIF_stStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorSpeedStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_enMask) {
            pif_enError = E_enInvalidParam;
			return FALSE;
    	}
    	if (pstStages[i].enMode & MM_PC_enMask) {
            pif_enError = E_enInvalidParam;
		    return FALSE;
    	}
    }

    PIF_stStepMotorSpeed* pstSpeed = (PIF_stStepMotorSpeed*)pstOwner;
    pstSpeed->__ucStageSize = ucStageSize;
    pstSpeed->__pstStages = pstStages;
    return TRUE;
}

/**
 * @fn pifStepMotorSpeed_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifStepMotorSpeed_Start(PIF_stStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stStepMotorSpeed* pstSpeed = (PIF_stStepMotorSpeed*)pstOwner;
    const PIF_stStepMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetStep) {
    	pif_enError = E_enInvalidParam;
	    return FALSE;
    }

    pstSpeed->__pstCurrentStage = &pstSpeed->__pstStages[ucStageIndex];
    pstStage = pstSpeed->__pstCurrentStage;

    if (pstStage->enMode & MM_CIAS_enMask) {
    	ucState = 0;
    	if (*pstStage->ppstStartSensor) {
    		if ((*pstStage->ppstStartSensor)->_swCurrState != ON) {
    			ucState |= 1;
    		}
    	}
    	if (*pstStage->ppstReduceSensor) {
    		if ((*pstStage->ppstReduceSensor)->_swCurrState != OFF) {
    			ucState |= 2;
    		}
    	}
    	if (*pstStage->ppstStopSensor) {
    		if ((*pstStage->ppstStopSensor)->_swCurrState != OFF) {
    			ucState |= 4;
    		}
    	}
    	if (ucState) {
#ifndef __PIF_NO_LOG__
    		pifLog_Printf(LT_enError, "DMS:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, ucState);
#endif
        	pif_enError = E_enInvalidState;
		    return FALSE;
    	}
    }

    if ((pstStage->enMode & MM_RT_enMask) == MM_RT_enTime) {
    	if (!unOperatingTime) {
        	pif_enError = E_enInvalidParam;
		    return FALSE;
    	}
    	else {
    		if (!pifStepMotor_SetOperatingTime(pstOwner, unOperatingTime)) return FALSE;
    	}
    }

    if (!pifStepMotor_StartControl(pstOwner)) return FALSE;

    pstSpeed->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstReduceSensor, _evtSwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstStopSensor, _evtSwitchStopChange, pstOwner);
    }

    pstOwner->_ucDirection = (pstStage->enMode & MM_D_enMask) >> MM_D_enShift;

    if (pstStage->usGsCtrlPps) {
    	pifStepMotor_SetPps(pstOwner, pstStage->usGsStartPps);
        pstOwner->_enState = MS_enGained;
    }
    else {
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsFixedPps);
        pstOwner->_enState = MS_enConst;
    }
    pstOwner->__ucError = 0;

    pifStepMotor_Start(pstOwner, 0);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.StepMotor) {
    	pifLog_Printf(LT_enInfo, "SMS:%u(%u) Start", __LINE__, pstOwner->_usPifId);
    }
#endif
    return TRUE;
}

/**
 * @fn pifStepMotorSpeed_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorSpeed_Stop(PIF_stStepMotor *pstOwner)
{
    const PIF_stStepMotorSpeedStage* pstStage = ((PIF_stStepMotorSpeed*)pstOwner)->__pstCurrentStage;

    if (pstOwner->_enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->_enState = MS_enOverRun;
    }
    else {
        pstOwner->_enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.StepMotor) {
    	pifLog_Printf(LT_enInfo, "SMS:%u(%u) Stop OT=%u", __LINE__, pstOwner->_usPifId, pstStage->usFsOverTime);
    }
#endif
}

/**
 * @fn pifStepMotorSpeed_Emergency
 * @brief
 * @param pstOwner
 */
void pifStepMotorSpeed_Emergency(PIF_stStepMotor *pstOwner)
{
    pstOwner->_enState = MS_enBreak;
}
