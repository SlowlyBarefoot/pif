#include "pifLog.h"
#include "pifDutyMotorPos.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	switch (pstOwner->_state) {
	case MS_BREAKING:
        pstOwner->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMP(%u) S:%u", pstOwner->_id, pstOwner->_state);
#endif
		break;
	}
}

static void _fnControlPos(PifDutyMotor *pstOwner)
{
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;
    const PIF_stDutyMotorPosStage *pstStage = pstPos->__pstCurrentStage;
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
			if (pstStage->enMode & MM_PC_MASK) {
				usTmpDuty = pstStage->usRsLowDuty;
				pstOwner->_state = MS_LOW_CONST;
			}
			else {
				usTmpDuty = 0;
				pstOwner->_state = MS_BREAK;
			}
		}
		else if (usTmpDuty > pstStage->usRsLowDuty + pstStage->usRsCtrlDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = pstStage->usRsLowDuty;
			pstOwner->_state = MS_LOW_CONST;
		}
	}

	if (pstStage->enMode & MM_PC_MASK) {
		if (pstPos->_unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpDuty = 0;
			if (pstOwner->_state < MS_BREAK) {
				pstOwner->_state = MS_BREAK;
			}
		}
		else if (pstPos->_unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_state < MS_REDUCE) {
				pstOwner->_state = MS_REDUCE;
			}
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
		pifLog_Printf(LT_INFO, "DMP(%u) %s D:%u(%u%%) CP:%u", pstOwner->_id,
				kMotorState[pstOwner->_state], usTmpDuty, (uint16_t)(100 * usTmpDuty / pstOwner->_max_duty),
				pstPos->_unCurrentPulse);
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_REDUCE) return;

	if (usLevel) {
		pifDutyMotorPos_Stop(pstOwner);
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
 * @fn pifDutyMotorPos_Create
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PifDutyMotor *pifDutyMotorPos_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
    PIF_stDutyMotorPos *p_owner = NULL;

    p_owner = calloc(sizeof(PIF_stDutyMotorPos), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PifDutyMotor* pstOwner = &p_owner->parent;
    if (!pifDutyMotor_Init(pstOwner, usPifId, p_timer, usMaxDuty)) goto fail;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail;

    pstOwner->__p_timer_delay = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
    if (!pstOwner->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__p_timer_delay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__control = _fnControlPos;
    return pstOwner;

fail:
	pifDutyMotorPos_Destroy(&pstOwner);
    return NULL;
}

/**
 * @fn pifDutyMotorPos_Destroy
 * @brief
 * @param pp_parent
 */
void pifDutyMotorPos_Destroy(PifDutyMotor** pp_parent)
{
    if (*pp_parent) {
    	pifDutyMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

/**
 * @fn pifDutyMotorPos_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorPos_AddStages(PifDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorPosStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;
    pstPos->__ucStageSize = ucStageSize;
    pstPos->__pstStages = pstStages;
    return TRUE;
}

/**
 * @fn pifDutyMotorPos_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifDutyMotorPos_Start(PifDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;
    const PIF_stDutyMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__act_set_duty || !pstOwner->__act_set_direction) {
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
    		if (!pifDutyMotor_SetOperatingTime(pstOwner, unOperatingTime)) return FALSE;
    	}
    }

    if (!pifDutyMotor_StartControl(pstOwner)) return FALSE;

    pstPos->_ucStageIndex = ucStageIndex;

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
    pstPos->_unCurrentPulse = 0;
    pstOwner->__error = 0;

    (*pstOwner->__act_set_duty)(pstOwner->_current_duty);
    return TRUE;
}

/**
 * @fn pifDutyMotorPos_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorPos_Stop(PifDutyMotor *pstOwner)
{
    if (pstOwner->_state == MS_IDLE) return;

    pstOwner->_state = MS_REDUCE;
}

/**
 * @fn pifDutyMotorPos_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorPos_Emergency(PifDutyMotor *pstOwner)
{
    pstOwner->_current_duty = 0;
    pstOwner->_state = MS_REDUCE;
}

/**
 * @fn pifDutyMotorPos_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorPos_sigEncoder(PifDutyMotor *pstOwner)
{
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;

    pstPos->_unCurrentPulse++;
	if (pstPos->__pstCurrentStage->enMode & MM_PC_MASK) {
		if (pstOwner->_state && pstOwner->_state < MS_STOP) {
			if (pstOwner->_current_duty && pstPos->_unCurrentPulse >= pstPos->__pstCurrentStage->unTotalPulse) {
				pstOwner->_current_duty = 0;
				(*pstOwner->__act_set_duty)(pstOwner->_current_duty);
			}
		}
	}
}
