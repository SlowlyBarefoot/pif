#include "pifLog.h"
#include "pifStepMotorPos.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "SMP:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _fnControlPos(PIF_stStepMotor *pstOwner)
{
	uint16_t usTmpPps = 0;
    PIF_stStepMotorPos* pstPos = (PIF_stStepMotorPos*)pstOwner;
    const PIF_stStepMotorPosStage *pstStage = pstPos->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpPps = pstOwner->_usCurrentPps;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpPps >= pstStage->usFsHighPps) {
			usTmpPps = pstStage->usFsHighPps;
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
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpPps = pstStage->usRsLowPps;
				pstOwner->_enState = MS_enLowConst;
			}
			else {
				usTmpPps = 0;
				pstOwner->_enState = MS_enBreak;
			}

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else if (usTmpPps > pstStage->usRsLowPps + pstStage->usRsCtrlPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = pstStage->usRsLowPps;
			pstOwner->_enState = MS_enLowConst;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstOwner->_unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpPps = 0;
			if (pstOwner->_enState < MS_enBreak) {
				pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
			}
		}
		else if (pstOwner->_unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_enState < MS_enReduce) {
				pstOwner->_enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
			}
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
		pifLog_Printf(LT_enInfo, "SMP:%u(%u) %s P/S:%u P:%u", nLine, pstOwner->_usPifId,
				c_cMotorState[pstOwner->_enState], usTmpPps, pstOwner->_unCurrentPulse);
	}
#endif
}

static void _evtSwitchReduceChange(PIF_usId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (usLevel) {
		pifStepMotorPos_Stop(pstOwner);
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

static void _fnStopStep(PIF_stStepMotor *pstOwner)
{
    pstOwner->_usCurrentPps = 0;
	pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.StepMotor) {
    	pifLog_Printf(LT_enInfo, "evtStopStep(%d, %d)", pstOwner->_unCurrentPulse, pstOwner->_enState);
    }
#endif
}

/**
 * @fn pifStepMotorPos_Add
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param ucResolution
 * @param enOperation
 * @param usControlPeriodMs
 * @return
 */
PIF_stStepMotor *pifStepMotorPos_Create(PIF_usId usPifId, PIF_stPulse* p_timer, uint8_t ucResolution,
		PIF_enStepMotorOperation enOperation, uint16_t usControlPeriodMs)
{
	PIF_stStepMotorPos* p_owner = NULL;

    p_owner = calloc(sizeof(PIF_stStepMotorPos), 1);
    if (!p_owner) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    PIF_stStepMotor* pstOwner = &p_owner->parent;
    if (!pifStepMotor_Init(pstOwner, usPifId, p_timer, ucResolution, enOperation)) goto fail;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail;

    pstOwner->__pstTimerDelay = pifPulse_AddItem(pstOwner->_p_timer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _fnControlPos;
	pstOwner->__fnStopStep = _fnStopStep;
    return pstOwner;

fail:
	pifStepMotorPos_Destroy(&pstOwner);
    return NULL;
}

/**
 * @fn pifStepMotorPos_Destroy
 * @brief
 * @param pp_parent
 */
void pifStepMotorPos_Destroy(PIF_stStepMotor** pp_parent)
{
    if (*pp_parent) {
    	pifStepMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

/**
 * @fn pifStepMotorPos_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifStepMotorPos_AddStages(PIF_stStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorPosStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_enMask) {
            pif_enError = E_enInvalidParam;
		    return FALSE;
    	}
    }

    PIF_stStepMotorPos* pstPos = (PIF_stStepMotorPos*)pstOwner;
    pstPos->__ucStageSize = ucStageSize;
    pstPos->__pstStages = pstStages;
    return TRUE;
}

/**
 * @fn pifStepMotorPos_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifStepMotorPos_Start(PIF_stStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stStepMotorPos* pstPos = (PIF_stStepMotorPos*)pstOwner;
    const PIF_stStepMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetStep) {
    	pif_enError = E_enInvalidParam;
	    return FALSE;
    }

    pstPos->__pstCurrentStage = &pstPos->__pstStages[ucStageIndex];
    pstStage = pstPos->__pstCurrentStage;

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

    pstPos->_ucStageIndex = ucStageIndex;

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
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsHighPps);
        pstOwner->_enState = MS_enConst;
    }
    pstOwner->__ucError = 0;

    if (pstStage->enMode & MM_PC_enMask) {
    	pifStepMotor_Start(pstOwner, pstStage->unTotalPulse);
    }
    else {
    	pifStepMotor_Start(pstOwner, 0);
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.StepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Start", __LINE__, pstOwner->_usPifId);
    }
#endif
    return TRUE;
}

/**
 * @fn pifStepMotorPos_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorPos_Stop(PIF_stStepMotor *pstOwner)
{
    if (pstOwner->_enState == MS_enIdle) return;

    pstOwner->_enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.StepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Stop", __LINE__, pstOwner->_usPifId);
    }
#endif
}

/**
 * @fn pifStepMotorPos_Emergency
 * @brief
 * @param pstOwner
 */
void pifStepMotorPos_Emergency(PIF_stStepMotor *pstOwner)
{
    pstOwner->_enState = MS_enBreak;
}
