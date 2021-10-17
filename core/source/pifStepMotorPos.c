#include "pifLog.h"
#include "pifStepMotorPos.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PifStepMotor *pstOwner = (PifStepMotor *)pvIssuer;

	switch (pstOwner->_state) {
	case MS_BREAKING:
        pstOwner->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "SMP(%u) S:%u", pstOwner->_id, pstOwner->_state);
#endif
		break;
	}
}

static void _fnControlPos(PifStepMotor *pstOwner)
{
	uint16_t usTmpPps = 0;
    PIF_stStepMotorPos* pstPos = (PIF_stStepMotorPos*)pstOwner;
    const PIF_stStepMotorPosStage *pstStage = pstPos->__pstCurrentStage;
    PifMotorState enState = pstOwner->_state;

	usTmpPps = pstOwner->_current_pps;

	if (pstOwner->_state == MS_GAINED) {
		if (usTmpPps >= pstStage->usFsHighPps) {
			usTmpPps = pstStage->usFsHighPps;
			pstOwner->_state = MS_CONST;
			if (pstOwner->evt_stable) (*pstOwner->evt_stable)(pstOwner);
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->_state == MS_REDUCE) {
		if (!pstStage->usRsCtrlPps) {
			if (pstStage->enMode & MM_PC_MASK) {
				usTmpPps = pstStage->usRsLowPps;
				pstOwner->_state = MS_LOW_CONST;
			}
			else {
				usTmpPps = 0;
				pstOwner->_state = MS_BREAK;
			}
		}
		else if (usTmpPps > pstStage->usRsLowPps + pstStage->usRsCtrlPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = pstStage->usRsLowPps;
			pstOwner->_state = MS_LOW_CONST;
		}
	}

	if (pstStage->enMode & MM_PC_MASK) {
		if (pstOwner->_current_pulse >= pstStage->unTotalPulse) {
			usTmpPps = 0;
			if (pstOwner->_state < MS_BREAK) {
				pstOwner->_state = MS_BREAK;
			}
		}
		else if (pstOwner->_current_pulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_state < MS_REDUCE) {
				pstOwner->_state = MS_REDUCE;
			}
		}
	}

	if (usTmpPps != pstOwner->_current_pps) {
		if (usTmpPps) pifStepMotor_SetPps(pstOwner, usTmpPps);
		pstOwner->_current_pps = usTmpPps;
	}

    if (pstOwner->_state == MS_BREAK) {
    	pifStepMotor_Break(pstOwner);
		if (pstStage->usRsBreakTime &&
				pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usRsBreakTime)) {
			pstOwner->_state = MS_BREAKING;
    	}
    	else {
    		pstOwner->_state = MS_STOPPING;
    	}
	}

    if (pstOwner->_state == MS_STOPPING) {
		if (!(pstStage->enMode & MM_NR_MASK)) {
			pifStepMotor_Release(pstOwner);
		}
		pstOwner->_state = MS_STOP;

		if (pstStage->enMode & MM_CFPS_MASK) {
	    	if (*pstStage->ppstStopSensor) {
	    		if ((*pstStage->ppstStopSensor)->_curr_state == OFF) {
	    			pstOwner->__error = 1;
	    		}
	    	}
	    }

		if (*pstStage->ppstReduceSensor) {
			pifSensor_DetachEvtChange(*pstStage->ppstReduceSensor);
		}
		if (*pstStage->ppstStopSensor) {
			pifSensor_DetachEvtChange(*pstStage->ppstStopSensor);
		}
    }

#ifndef __PIF_NO_LOG__
	if (enState != pstOwner->_state && pif_log_flag.bt.step_motor) {
		pifLog_Printf(LT_INFO, "SMP(%u) %s P/S:%u CP:%u", pstOwner->_id,
				kMotorState[pstOwner->_state], usTmpPps, pstOwner->_current_pulse);
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifStepMotor *pstOwner = (PifStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_REDUCE) return;

	if (usLevel) {
		pifStepMotorPos_Stop(pstOwner);
	}
}

static void _evtSwitchStopChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifStepMotor *pstOwner = (PifStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_BREAK) return;

	if (usLevel) {
		pstOwner->_state = MS_BREAK;
	}
}

static void _fnStopStep(PifStepMotor *pstOwner)
{
    pstOwner->_current_pps = 0;
	pstOwner->_state = MS_BREAK;

#ifndef __PIF_NO_LOG__
    if (pif_log_flag.bt.step_motor) {
    	pifLog_Printf(LT_INFO, "SMP(%u) CP:%lu S:%d", pstOwner->_id,
				pstOwner->_current_pulse, pstOwner->_state);
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
PifStepMotor *pifStepMotorPos_Create(PifId usPifId, PifPulse* p_timer, uint8_t ucResolution,
		PifStepMotorOperation enOperation, uint16_t usControlPeriodMs)
{
	PIF_stStepMotorPos* p_owner = NULL;

    p_owner = calloc(sizeof(PIF_stStepMotorPos), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PifStepMotor* pstOwner = &p_owner->parent;
    if (!pifStepMotor_Init(pstOwner, usPifId, p_timer, ucResolution, enOperation)) goto fail;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail;

    pstOwner->__p_timer_delay = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
    if (!pstOwner->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__p_timer_delay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__control = _fnControlPos;
	pstOwner->__stop_step = _fnStopStep;
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
void pifStepMotorPos_Destroy(PifStepMotor** pp_parent)
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
BOOL pifStepMotorPos_AddStages(PifStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorPosStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
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
BOOL pifStepMotorPos_Start(PifStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stStepMotorPos* pstPos = (PIF_stStepMotorPos*)pstOwner;
    const PIF_stStepMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__act_set_step) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    pstPos->__pstCurrentStage = &pstPos->__pstStages[ucStageIndex];
    pstStage = pstPos->__pstCurrentStage;

    if (pstStage->enMode & MM_CIAS_MASK) {
    	ucState = 0;
    	if (*pstStage->ppstStartSensor) {
    		if ((*pstStage->ppstStartSensor)->_curr_state != ON) {
    			ucState |= 1;
    		}
    	}
    	if (*pstStage->ppstReduceSensor) {
    		if ((*pstStage->ppstReduceSensor)->_curr_state != OFF) {
    			ucState |= 2;
    		}
    	}
    	if (*pstStage->ppstStopSensor) {
    		if ((*pstStage->ppstStopSensor)->_curr_state != OFF) {
    			ucState |= 4;
    		}
    	}
    	if (ucState) {
        	pif_error = E_INVALID_STATE;
		    return FALSE;
    	}
    }

    if ((pstStage->enMode & MM_RT_MASK) == MM_RT_TIME) {
    	if (!unOperatingTime) {
        	pif_error = E_INVALID_PARAM;
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

    pstOwner->_direction = (pstStage->enMode & MM_D_MASK) >> MM_D_SHIFT;

    if (pstStage->usGsCtrlPps) {
    	pifStepMotor_SetPps(pstOwner, pstStage->usGsStartPps);
        pstOwner->_state = MS_GAINED;
    }
    else {
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsHighPps);
        pstOwner->_state = MS_CONST;
    }
    pstOwner->__error = 0;

    if (pstStage->enMode & MM_PC_MASK) {
    	pifStepMotor_Start(pstOwner, pstStage->unTotalPulse);
    }
    else {
    	pifStepMotor_Start(pstOwner, 0);
    }
    return TRUE;
}

/**
 * @fn pifStepMotorPos_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorPos_Stop(PifStepMotor *pstOwner)
{
    if (pstOwner->_state == MS_IDLE) return;

    pstOwner->_state = MS_REDUCE;
}

/**
 * @fn pifStepMotorPos_Emergency
 * @brief
 * @param pstOwner
 */
void pifStepMotorPos_Emergency(PifStepMotor *pstOwner)
{
    pstOwner->_state = MS_BREAK;
}
