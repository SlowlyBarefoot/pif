#include "pifLog.h"
#include "pifDutyMotorSpeed.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	switch (pstOwner->_state) {
	case MS_OVER_RUN:
        pstOwner->_state = MS_REDUCE;
        break;

	case MS_BREAKING:
        pstOwner->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMS(%u) S:%u", pstOwner->_id, pstOwner->_state);
#endif
		break;
	}
}

static void _fnControlSpeed(PifDutyMotor *pstOwner)
{
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    const PIF_stDutyMotorSpeedStage *pstStage = pstSpeed->__pstCurrentStage;
    PifMotorState enState = pstOwner->_state;

	usTmpDuty = pstOwner->_current_duty;

	if (pstOwner->_state == MS_GAINED) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->_state = MS_CONST;
			if (pstOwner->evt_stable) (*pstOwner->evt_stable)(pstOwner);
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
		}
	}
	else if (pstOwner->_state == MS_REDUCE) {
		if (!pstStage->usRsCtrlDuty) {
			usTmpDuty = 0;
			pstOwner->_state = MS_BREAK;
		}
		else if (usTmpDuty > pstStage->usRsCtrlDuty && usTmpDuty > pstStage->usRsLowDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = 0;
			pstOwner->_state = MS_BREAK;
		}
	}

	if (usTmpDuty != pstOwner->_current_duty) {
		(*pstOwner->__act_set_duty)(usTmpDuty);
		pstOwner->_current_duty = usTmpDuty;
	}

    if (pstOwner->_state == MS_BREAK) {
    	if (pstOwner->__act_operate_break && pstStage->usRsBreakTime &&
    			pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usRsBreakTime)) {
			(*pstOwner->__act_operate_break)(1);
			pstOwner->_state = MS_BREAKING;
    	}
    	else {
			if (pstOwner->__act_operate_break) (*pstOwner->__act_operate_break)(1);
    		pstOwner->_state = MS_STOPPING;
    	}
	}

    if (pstOwner->_state == MS_STOPPING) {
		if (!(pstStage->enMode & MM_NR_MASK)) {
			if (pstOwner->__act_operate_break) (*pstOwner->__act_operate_break)(0);
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
	if (enState != pstOwner->_state && pif_log_flag.bt.duty_motor) {
		pifLog_Printf(LT_INFO, "DMS(%u) %s D:%u(%u%%)", pstOwner->_id,
				kMotorState[pstOwner->_state], usTmpDuty, (uint16_t)(100 * usTmpDuty / pstOwner->_max_duty));
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_REDUCE) return;

	if (usLevel) {
		pifDutyMotorSpeed_Stop(pstOwner);
	}
}

static void _evtSwitchStopChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_BREAK) return;

	if (usLevel) {
		pstOwner->_current_duty = 0;
		pstOwner->_state = MS_BREAK;
	}
}

/**
 * @fn pifDutyMotorSpeed_Create
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PifDutyMotor *pifDutyMotorSpeed_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
	PIF_stDutyMotorSpeed* p_owner = NULL;

	p_owner = calloc(sizeof(PIF_stDutyMotorSpeed), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PifDutyMotor *pstOwner = &p_owner->parent;
    if (!pifDutyMotor_Init(pstOwner, usPifId, p_timer, usMaxDuty)) goto fail;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail;

    pstOwner->__p_timer_delay = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
    if (!pstOwner->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__p_timer_delay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__control = _fnControlSpeed;
    return pstOwner;

fail:
	pifDutyMotorSpeed_Destroy(&pstOwner);
    return NULL;
}

/**
 * @fn pifDutyMotorSpeed_Destroy
 * @brief
 * @param pp_owner
 */
void pifDutyMotorSpeed_Destroy(PifDutyMotor** pp_owner)
{
    if (*pp_owner) {
    	pifDutyMotor_Clear(*pp_owner);
        free(*pp_owner);
        *pp_owner= NULL;
    }
}

/**
 * @fn pifDutyMotorSpeed_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorSpeed_AddStages(PifDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedStage *pstStages)
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

    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    pstSpeed->__ucStageSize = ucStageSize;
    pstSpeed->__pstStages = pstStages;
    return TRUE;
}

/**
 * @fn pifDutyMotorSpeed_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifDutyMotorSpeed_Start(PifDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    const PIF_stDutyMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__act_set_duty || !pstOwner->__act_set_direction) {
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
    		if (!pifDutyMotor_SetOperatingTime(pstOwner, unOperatingTime)) return FALSE;
    	}
    }

    if (!pifDutyMotor_StartControl(pstOwner)) return FALSE;

    pstSpeed->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstReduceSensor, _evtSwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstStopSensor, _evtSwitchStopChange, pstOwner);
    }

    if (pstOwner->__act_set_direction) (*pstOwner->__act_set_direction)((pstStage->enMode & MM_D_MASK) >> MM_D_SHIFT);

    if (pstStage->usGsCtrlDuty) {
    	pstOwner->_current_duty = pstStage->usGsStartDuty;
        pstOwner->_state = MS_GAINED;
    }
    else {
    	pstOwner->_current_duty = pstStage->usFsHighDuty;
        pstOwner->_state = MS_CONST;
    }
    pstOwner->__error = 0;

    (*pstOwner->__act_set_duty)(pstOwner->_current_duty);
    return TRUE;
}

/**
 * @fn pifDutyMotorSpeed_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorSpeed_Stop(PifDutyMotor *pstOwner)
{
    const PIF_stDutyMotorSpeedStage* pstStage = ((PIF_stDutyMotorSpeed*)pstOwner)->__pstCurrentStage;

    if (pstOwner->_state == MS_IDLE) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usFsOverTime)) {
        pstOwner->_state = MS_OVER_RUN;
    }
    else {
        pstOwner->_state = MS_REDUCE;
    }
}

/**
 * @fn pifDutyMotorSpeed_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorSpeed_Emergency(PifDutyMotor *pstOwner)
{
    pstOwner->_current_duty = 0;
    pstOwner->_state = MS_BREAK;
}
