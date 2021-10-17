#include "pifLog.h"
#include "pifStepMotorSpeed.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PifStepMotor *pstOwner = (PifStepMotor *)pvIssuer;

	switch (pstOwner->_state) {
	case MS_OVER_RUN:
        pstOwner->_state = MS_REDUCE;
        break;

	case MS_BREAKING:
        pstOwner->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "SMS(%u) S:%u", pstOwner->_id, pstOwner->_state);
#endif
		break;
	}
}

static void _fnControlSpeed(PifStepMotor *pstOwner)
{
	uint16_t usTmpPps = 0;
    PIF_stStepMotorSpeed* pstSpeed = (PIF_stStepMotorSpeed*)pstOwner;
    const PIF_stStepMotorSpeedStage *pstStage = pstSpeed->__pstCurrentStage;
    PifMotorState enState = pstOwner->_state;

	usTmpPps = pstOwner->_current_pps;

	if (pstOwner->_state == MS_GAINED) {
		if (usTmpPps >= pstStage->usFsFixedPps) {
			usTmpPps = pstStage->usFsFixedPps;
			pstOwner->_state = MS_CONST;
			if (pstOwner->evt_stable) (*pstOwner->evt_stable)(pstOwner);
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->_state == MS_REDUCE) {
		if (!pstStage->usRsCtrlPps) {
			usTmpPps = 0;
			pstOwner->_state = MS_BREAK;
		}
		else if (usTmpPps > pstStage->usRsCtrlPps && usTmpPps > pstStage->usRsStopPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = 0;
			pstOwner->_state = MS_BREAK;
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
		pifLog_Printf(LT_INFO, "SMS(%u) %s P/S:%u", pstOwner->_id,
				kMotorState[pstOwner->_state], usTmpPps);
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifStepMotor *pstOwner = (PifStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_REDUCE) return;

	if (usLevel) {
		pifStepMotorSpeed_Stop(pstOwner);
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
PifStepMotor *pifStepMotorSpeed_Create(PifId usPifId, PifPulse* p_timer, uint8_t ucResolution,
		PifStepMotorOperation enOperation, uint16_t usControlPeriodMs)
{
	PIF_stStepMotorSpeed* p_owner = NULL;

	p_owner = calloc(sizeof(PIF_stStepMotorSpeed), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PifStepMotor* pstOwner = (PifStepMotor*)&p_owner->parent;
    if (!pifStepMotor_Init(pstOwner, usPifId, p_timer, ucResolution, enOperation)) goto fail;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail;

    pstOwner->__p_timer_delay = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
    if (!pstOwner->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__p_timer_delay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__control = _fnControlSpeed;
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
void pifStepMotorSpeed_Destroy(PifStepMotor** pp_parent)
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
BOOL pifStepMotorSpeed_AddStages(PifStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorSpeedStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
			return FALSE;
    	}
    	if (pstStages[i].enMode & MM_PC_MASK) {
            pif_error = E_INVALID_PARAM;
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
BOOL pifStepMotorSpeed_Start(PifStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stStepMotorSpeed* pstSpeed = (PIF_stStepMotorSpeed*)pstOwner;
    const PIF_stStepMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__act_set_step) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    pstSpeed->__pstCurrentStage = &pstSpeed->__pstStages[ucStageIndex];
    pstStage = pstSpeed->__pstCurrentStage;

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
#ifndef __PIF_NO_LOG__
    		pifLog_Printf(LT_ERROR, "SMS(%u) S:%u", pstOwner->_id, ucState);
#endif
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

    pstSpeed->_ucStageIndex = ucStageIndex;

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
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsFixedPps);
        pstOwner->_state = MS_CONST;
    }
    pstOwner->__error = 0;

    pifStepMotor_Start(pstOwner, 0);
    return TRUE;
}

/**
 * @fn pifStepMotorSpeed_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorSpeed_Stop(PifStepMotor *pstOwner)
{
    const PIF_stStepMotorSpeedStage* pstStage = ((PIF_stStepMotorSpeed*)pstOwner)->__pstCurrentStage;

    if (pstOwner->_state == MS_IDLE) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usFsOverTime)) {
        pstOwner->_state = MS_OVER_RUN;
    }
    else {
        pstOwner->_state = MS_REDUCE;
    }
}

/**
 * @fn pifStepMotorSpeed_Emergency
 * @brief
 * @param pstOwner
 */
void pifStepMotorSpeed_Emergency(PifStepMotor *pstOwner)
{
    pstOwner->_state = MS_BREAK;
}
