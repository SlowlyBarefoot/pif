#include "pifLog.h"
#include "pifDutyMotorPos.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMP(%u) S:%u", pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _fnControlPos(PIF_stDutyMotor *pstOwner)
{
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;
    const PIF_stDutyMotorPosStage *pstStage = pstPos->__pstCurrentStage;
    PIF_enMotorState enState = pstOwner->_enState;

	usTmpDuty = pstOwner->_usCurrentDuty;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->_enState = MS_enConst;
			if (pstOwner->evtStable) (*pstOwner->evtStable)(pstOwner);
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
		}
	}
	else if (pstOwner->_enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpDuty = pstStage->usRsLowDuty;
				pstOwner->_enState = MS_enLowConst;
			}
			else {
				usTmpDuty = 0;
				pstOwner->_enState = MS_enBreak;
			}
		}
		else if (usTmpDuty > pstStage->usRsLowDuty + pstStage->usRsCtrlDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = pstStage->usRsLowDuty;
			pstOwner->_enState = MS_enLowConst;
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstPos->_unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpDuty = 0;
			if (pstOwner->_enState < MS_enBreak) {
				pstOwner->_enState = MS_enBreak;
			}
		}
		else if (pstPos->_unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_enState < MS_enReduce) {
				pstOwner->_enState = MS_enReduce;
			}
		}
	}

	if (usTmpDuty != pstOwner->_usCurrentDuty) {
		(*pstOwner->__actSetDuty)(usTmpDuty);
		pstOwner->_usCurrentDuty = usTmpDuty;
	}

    if (pstOwner->_enState == MS_enBreak) {
    	if (pstOwner->__actOperateBreak && pstStage->usRsBreakTime &&
    			pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usRsBreakTime)) {
			(*pstOwner->__actOperateBreak)(1);
			pstOwner->_enState = MS_enBreaking;
    	}
    	else {
			if (pstOwner->__actOperateBreak) (*pstOwner->__actOperateBreak)(1);
    		pstOwner->_enState = MS_enStopping;
    	}
	}

    if (pstOwner->_enState == MS_enStopping) {
		if (!(pstStage->enMode & MM_NR_enMask)) {
			if (pstOwner->__actOperateBreak) (*pstOwner->__actOperateBreak)(0);
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
    }

#ifndef __PIF_NO_LOG__
	if (enState != pstOwner->_enState && pif_log_flag.bt.duty_motor) {
		pifLog_Printf(LT_INFO, "DMP(%u) %s D:%u(%u%%) CP:%u", pstOwner->_usPifId,
				c_cMotorState[pstOwner->_enState], usTmpDuty, (uint16_t)(100 * usTmpDuty / pstOwner->_usMaxDuty),
				pstPos->_unCurrentPulse);
	}
#endif
}

static void _evtSwitchReduceChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (usLevel) {
		pifDutyMotorPos_Stop(pstOwner);
	}
}

static void _evtSwitchStopChange(PifId usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enBreak) return;

	if (usLevel) {
		pstOwner->_usCurrentDuty = 0;
		pstOwner->_enState = MS_enBreak;
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
PIF_stDutyMotor *pifDutyMotorPos_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
    PIF_stDutyMotorPos *p_owner = NULL;

    p_owner = calloc(sizeof(PIF_stDutyMotorPos), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PIF_stDutyMotor* pstOwner = &p_owner->parent;
    if (!pifDutyMotor_Init(pstOwner, usPifId, p_timer, usMaxDuty)) goto fail;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail;

    pstOwner->__pstTimerDelay = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
    if (!pstOwner->__pstTimerDelay) goto fail;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _fnControlPos;
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
void pifDutyMotorPos_Destroy(PIF_stDutyMotor** pp_parent)
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
BOOL pifDutyMotorPos_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorPosStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_enMask) {
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
BOOL pifDutyMotorPos_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;
    const PIF_stDutyMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetDuty || !pstOwner->__actSetDirection) {
    	pif_error = E_INVALID_PARAM;
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
        	pif_error = E_INVALID_STATE;
		    return FALSE;
    	}
    }

    if ((pstStage->enMode & MM_RT_enMask) == MM_RT_enTime) {
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

    if (pstOwner->__actSetDirection) (*pstOwner->__actSetDirection)((pstStage->enMode & MM_D_enMask) >> MM_D_enShift);

    if (pstStage->usGsCtrlDuty) {
    	pstOwner->_usCurrentDuty = pstStage->usGsStartDuty;
        pstOwner->_enState = MS_enGained;
    }
    else {
    	pstOwner->_usCurrentDuty = pstStage->usFsHighDuty;
        pstOwner->_enState = MS_enConst;
    }
    pstPos->_unCurrentPulse = 0;
    pstOwner->__ucError = 0;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
    return TRUE;
}

/**
 * @fn pifDutyMotorPos_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorPos_Stop(PIF_stDutyMotor *pstOwner)
{
    if (pstOwner->_enState == MS_enIdle) return;

    pstOwner->_enState = MS_enReduce;
}

/**
 * @fn pifDutyMotorPos_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorPos_Emergency(PIF_stDutyMotor *pstOwner)
{
    pstOwner->_usCurrentDuty = 0;
    pstOwner->_enState = MS_enReduce;
}

/**
 * @fn pifDutyMotorPos_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorPos_sigEncoder(PIF_stDutyMotor *pstOwner)
{
    PIF_stDutyMotorPos* pstPos = (PIF_stDutyMotorPos*)pstOwner;

    pstPos->_unCurrentPulse++;
	if (pstPos->__pstCurrentStage->enMode & MM_PC_enMask) {
		if (pstOwner->_enState && pstOwner->_enState < MS_enStop) {
			if (pstOwner->_usCurrentDuty && pstPos->_unCurrentPulse >= pstPos->__pstCurrentStage->unTotalPulse) {
				pstOwner->_usCurrentDuty = 0;
				(*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
			}
		}
	}
}
