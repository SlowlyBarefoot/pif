#include <string.h>

#include "pifLog.h"
#include "pifDutyMotorSpeedEnc.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
	PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	switch (pstOwner->_state) {
    case MS_GAINED:
    case MS_STABLE:
        pstOwner->__error = 1;
        break;

	case MS_OVER_RUN:
        pstOwner->_state = MS_REDUCE;
        break;

	case MS_BREAKING:
        pstOwner->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMSE(%u) S:%u", pstOwner->_id, pstOwner->_state);
#endif
		break;
	}
}

static void _fnControlSpeedEnc(PifDutyMotor *pstOwner)
{
	float fCtrlDuty = 0;
	uint16_t usTmpEnc = 0;
	uint16_t usTmpDuty = 0;
	uint16_t usEncAverage = 0;
    PIF_stDutyMotorSpeedEnc* pstSpeedEnc = (PIF_stDutyMotorSpeedEnc*)pstOwner;
	const PIF_stDutyMotorSpeedEncStage *pstStage = pstSpeedEnc->__pstCurrentStage;
	PifMotorState enState = pstOwner->_state;

	usTmpEnc = pstSpeedEnc->__usMeasureEnc;
	pstSpeedEnc->__usMeasureEnc = 0;
	usTmpDuty = pstOwner->_current_duty;

	pstSpeedEnc->__unEncSampleSum -= pstSpeedEnc->__ausEncSample[pstSpeedEnc->__ucEncSampleIdx];
	pstSpeedEnc->__unEncSampleSum += usTmpEnc;
	pstSpeedEnc->__ausEncSample[pstSpeedEnc->__ucEncSampleIdx] = usTmpEnc;
	pstSpeedEnc->__ucEncSampleIdx = (pstSpeedEnc->__ucEncSampleIdx + 1) % MAX_STABLE_CNT;

	if (pstOwner->_state == MS_GAINED) {
		if (usTmpEnc >= pstSpeedEnc->__usArrivePPR) {
			pstOwner->_state = MS_STABLE;
            if (!pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usFsStableTimeout)) {
                pstOwner->__error = 1;
            }
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
			if (usTmpDuty >= pstStage->usFsHighDuty) usTmpDuty = pstStage->usFsHighDuty;
		}
	}
	else if (pstOwner->_state == MS_STABLE) {
		usEncAverage = pstSpeedEnc->__unEncSampleSum / MAX_STABLE_CNT;

        if (usEncAverage >= pstSpeedEnc->__usErrLowPPR && usEncAverage <= pstSpeedEnc->__usErrHighPPR) {
            pifPulse_StopItem(pstOwner->__p_timer_delay);
            pstOwner->_state = MS_CONST;
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

	if (pstStage->enMode & MM_SC_MASK) {
		if ((pstOwner->_state == MS_STABLE) || (pstOwner->_state == MS_CONST)) {
			fCtrlDuty = pifPidControl_Calcurate(&pstSpeedEnc->__stPidControl, pstStage->fFsPulsesPerRange - usTmpEnc);

			if (fCtrlDuty > 0) {
				if (usTmpDuty + fCtrlDuty < pstStage->usFsHighDuty) {
					usTmpDuty += fCtrlDuty;
				}
				else {
					usTmpDuty += (pstStage->usFsHighDuty - usTmpDuty) / 2;
					if (usTmpDuty > pstStage->usFsHighDuty) {
						usTmpDuty = pstStage->usFsHighDuty;
					}
				}
			}
			else if (fCtrlDuty < 0) {
				if (usTmpDuty + fCtrlDuty >= 0) {
					usTmpDuty += fCtrlDuty;
				}
				else {
					usTmpDuty /= 2;
				}
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
		pifLog_Printf(LT_INFO, "DMSE(%u) %s D:%u(%u%%) E:%u", pstOwner->_id,
				kMotorState[pstOwner->_state], usTmpDuty, (uint16_t)(100 * usTmpDuty / pstOwner->_max_duty), usTmpEnc);
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PifDutyMotor *pstOwner = (PifDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_state >= MS_REDUCE) return;

	if (usLevel) {
		pifDutyMotorSpeedEnc_Stop(pstOwner);
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
 * @fn pifDutyMotorSpeedEnc_Create
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PifDutyMotor *pifDutyMotorSpeedEnc_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
	PIF_stDutyMotorSpeedEnc* p_owner = NULL;

	p_owner = calloc(sizeof(PIF_stDutyMotorSpeedEnc), 1);
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

	pstOwner->__control = _fnControlSpeedEnc;
    return pstOwner;

fail:
	pifDutyMotorSpeedEnc_Destroy(&pstOwner);
    return NULL;
}

void pifDutyMotorSpeedEnc_Destroy(PifDutyMotor** pp_owner)
{
    if (*pp_owner) {
    	pifDutyMotor_Clear(*pp_owner);
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

/**
 * @fn pifDutyMotorSpeedEnc_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedEncStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_PC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    PIF_stDutyMotorSpeedEnc* pstSpeedEnc = (PIF_stDutyMotorSpeedEnc*)pstOwner;
    pstSpeedEnc->__ucStageSize = ucStageSize;
    pstSpeedEnc->__pstStages = pstStages;
    return TRUE;
}

/**
 * @fn pifDutyMotorSpeedEnc_GetPidControl
 * @brief
 * @param pstOwner
 * @return
 */
PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotor *pstOwner)
{
	return &((PIF_stDutyMotorSpeedEnc*)pstOwner)->__stPidControl;
}

/**
 * @fn pifDutyMotorSpeedEnc_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stDutyMotorSpeedEnc* pstSpeedEnc = (PIF_stDutyMotorSpeedEnc*)pstOwner;
    const PIF_stDutyMotorSpeedEncStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__act_set_duty || !pstOwner->__act_set_direction) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    pstSpeedEnc->__pstCurrentStage = &pstSpeedEnc->__pstStages[ucStageIndex];
    pstStage = pstSpeedEnc->__pstCurrentStage;

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

    pstSpeedEnc->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstReduceSensor, _evtSwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstStopSensor, _evtSwitchStopChange, pstOwner);
    }

    if (pstOwner->__act_set_direction) (*pstOwner->__act_set_direction)((pstStage->enMode & MM_D_MASK) >> MM_D_SHIFT);

    if (pstStage->usGsCtrlDuty) {
		if (!pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usGsArriveTimeout)) return FALSE;

		pstSpeedEnc->__usArrivePPR = pstStage->fFsPulsesPerRange * pstStage->ucGsArriveRatio / 100;
		pstOwner->_current_duty = pstStage->usGsStartDuty;
		pstOwner->_state = MS_GAINED;
    }
    else {
    	pstOwner->_current_duty = pstStage->usFsHighDuty;
        pstOwner->_state = MS_CONST;
    }
    pstSpeedEnc->__usErrLowPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrLow / 100;
    pstSpeedEnc->__usErrHighPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrHigh / 100;
    pstSpeedEnc->__ucEncSampleIdx = 0;
    memset(pstSpeedEnc->__ausEncSample, 0, sizeof(pstSpeedEnc->__ausEncSample));
    pstSpeedEnc->__unEncSampleSum = 0;
    pstSpeedEnc->__usMeasureEnc = 0;
    pstOwner->__error = 0;

    (*pstOwner->__act_set_duty)(pstOwner->_current_duty);
    return TRUE;
}

/**
 * @fn pifDutyMotorSpeedEnc_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_Stop(PifDutyMotor *pstOwner)
{
    const PIF_stDutyMotorSpeedEncStage* pstStage = ((PIF_stDutyMotorSpeedEnc*)pstOwner)->__pstCurrentStage;

    if (pstOwner->_state == MS_IDLE) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__p_timer_delay, pstStage->usFsOverTime)) {
        pstOwner->_state = MS_OVER_RUN;
    }
    else {
        pstOwner->_state = MS_REDUCE;
    }
}

/**
 * @fn pifDutyMotorSpeedEnc_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotor *pstOwner)
{
    pstOwner->_current_duty = 0;
    pstOwner->_state = MS_BREAK;
}

/**
 * @fn pifDutyMotorSpeedEnc_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_sigEncoder(PifDutyMotor *pstOwner)
{
    ((PIF_stDutyMotorSpeedEnc*)pstOwner)->__usMeasureEnc++;
}
