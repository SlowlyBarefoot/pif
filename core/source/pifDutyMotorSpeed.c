#include "pifLog.h"
#include "pifDutyMotorSpeed.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enOverRun:
        pstOwner->_enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMS:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _fnControlSpeed(PIF_stDutyMotor *pstOwner)
{
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    const PIF_stDutyMotorSpeedStage *pstStage = pstSpeed->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpDuty = pstOwner->_usCurrentDuty;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->_enState = MS_enConst;
			if (pstOwner->evtStable) (*pstOwner->evtStable)(pstOwner);

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
		}
	}
	else if (pstOwner->_enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			usTmpDuty = 0;
			pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else if (usTmpDuty > pstStage->usRsCtrlDuty && usTmpDuty > pstStage->usRsLowDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = 0;
			pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
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

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
    	}
    	else {
			if (pstOwner->__actOperateBreak) (*pstOwner->__actOperateBreak)(1);
    		pstOwner->_enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
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

#ifndef __PIF_NO_LOG__
		nLine = __LINE__;
#endif
    }

#ifndef __PIF_NO_LOG__
	if (nLine && pif_stLogFlag.bt.DutyMotor) {
		pifLog_Printf(LT_enInfo, "DMS:%u(%u) %s D:%u", nLine, pstOwner->_usPifId, c_cMotorState[pstOwner->_enState], usTmpDuty);
	}
#endif
}

static void _evtSwitchReduceChange(PIF_usId _usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)_usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (usLevel) {
		pifDutyMotorSpeed_Stop(pstOwner);
	}
}

static void _evtSwitchStopChange(PIF_usId _usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)_usPifId;

	if (pstOwner->_enState >= MS_enBreak) return;

	if (usLevel) {
		pstOwner->_usCurrentDuty = 0;
		pstOwner->_enState = MS_enBreak;
	}
}

/**
 * @fn pifDutyMotorSpeed_Create
 * @brief 
 * @param _usPifId
 * @param p_timer
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PIF_stDutyMotor *pifDutyMotorSpeed_Create(PIF_usId _usPifId, PIF_stPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
	PIF_stDutyMotorSpeed* p_owner = NULL;

	p_owner = calloc(sizeof(PIF_stDutyMotorSpeed), 1);
    if (!p_owner) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    PIF_stDutyMotor *pstOwner = &p_owner->parent;
    if (!pifDutyMotor_Init(pstOwner, _usPifId, p_timer, usMaxDuty)) goto fail_clear;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail_clear;

    pstOwner->__pstTimerDelay = pifPulse_AddItem(pstOwner->_p_timer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _fnControlSpeed;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) MD:%u P:%u EC:%u", __LINE__, _usPifId, usMaxDuty, usControlPeriod, pif_enError);
#endif
fail_clear:
	pifDutyMotorSpeed_Destroy(&pstOwner);
    return NULL;
}

/**
 * @fn pifDutyMotorSpeed_Destroy
 * @brief
 * @param pp_owner
 */
void pifDutyMotorSpeed_Destroy(PIF_stDutyMotor** pp_owner)
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
BOOL pifDutyMotorSpeed_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedStage *pstStages)
{
    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_enMask) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    	if (pstStages[i].enMode & MM_PC_enMask) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    }

    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    pstSpeed->__ucStageSize = ucStageSize;
    pstSpeed->__pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDutyMotorSpeed_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifDutyMotorSpeed_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
    PIF_stDutyMotorSpeed* pstSpeed = (PIF_stDutyMotorSpeed*)pstOwner;
    const PIF_stDutyMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetDuty || !pstOwner->__actSetDirection) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
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
    		goto fail;
    	}
    }

    if ((pstStage->enMode & MM_RT_enMask) == MM_RT_enTime) {
    	if (!unOperatingTime) {
        	pif_enError = E_enInvalidParam;
    		goto fail;
    	}
    	else {
    		if (!pifDutyMotor_SetOperatingTime(pstOwner, unOperatingTime)) goto fail;
    	}
    }

    if (!pifDutyMotor_StartControl(pstOwner)) goto fail;

    pstSpeed->_ucStageIndex = ucStageIndex;

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
    pstOwner->__ucError = 0;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.DutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMS:%u(%u) Start", __LINE__, pstOwner->_usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageIndex, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDutyMotorSpeed_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorSpeed_Stop(PIF_stDutyMotor *pstOwner)
{
    const PIF_stDutyMotorSpeedStage* pstStage = ((PIF_stDutyMotorSpeed*)pstOwner)->__pstCurrentStage;

    if (pstOwner->_enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->_enState = MS_enOverRun;
    }
    else {
        pstOwner->_enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.bt.DutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMS:%u(%u) Stop OT=%u", __LINE__, pstOwner->_usPifId, pstStage->usFsOverTime);
    }
#endif
}

/**
 * @fn pifDutyMotorSpeed_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorSpeed_Emergency(PIF_stDutyMotor *pstOwner)
{
    pstOwner->_usCurrentDuty = 0;
    pstOwner->_enState = MS_enBreak;
}
