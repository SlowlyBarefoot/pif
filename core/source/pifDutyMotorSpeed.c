#include "pifLog.h"
#include "pifDutyMotorSpeed.h"


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->enState) {
	case MS_enOverRun:
        pstOwner->enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMS:%u(%u) S:%u", __LINE__, pstOwner->usPifId, pstOwner->enState);
#endif
		break;
	}
}

static void _ControlSpeed(PIF_stDutyMotorBase *pstBase)
{
	PIF_stDutyMotor *pstOwner = &pstBase->stOwner;
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorSpeedInfo *pstInfo = pstBase->pvInfo;
    const PIF_stDutyMotorSpeedStage *pstStage = pstInfo->pstCurrentStage;

	usTmpDuty = pstOwner->usCurrentDuty;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->enState = MS_enConst;
			if (pstBase->evtStable) (*pstBase->evtStable)(pstOwner, pstBase->pvInfo);

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMS:%u(%u) Const D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
		}
	}
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			usTmpDuty = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMS:%u(%u) Break D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
		}
		else if (usTmpDuty > pstStage->usRsCtrlDuty && usTmpDuty > pstStage->usRsLowDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMS:%u(%u) Break D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
		}
	}

	if (usTmpDuty != pstOwner->usCurrentDuty) {
		(*pstBase->actSetDuty)(usTmpDuty);
		pstOwner->usCurrentDuty = usTmpDuty;
	}

    if (pstOwner->enState == MS_enBreak) {
    	if (pstBase->actOperateBreak && pstStage->usRsBreakTime &&
    			pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usRsBreakTime)) {
			(*pstBase->actOperateBreak)(1);
			pstOwner->enState = MS_enBreaking;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMS:%u(%u) Breaking D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
    	}
    	else {
			if (pstBase->actOperateBreak) (*pstBase->actOperateBreak)(1);
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMS:%u(%u) Stopping D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
    	}
	}

    if (pstOwner->enState == MS_enStopping) {
		if (!(pstStage->enMode & MM_NR_enMask)) {
			if (pstBase->actOperateBreak) (*pstBase->actOperateBreak)(0);
		}
		pstOwner->enState = MS_enStop;

		if (pstStage->enMode & MM_CFPS_enMask) {
	    	if (*pstStage->ppstStopSwitch) {
	    		if ((*pstStage->ppstStopSwitch)->swCurrState == OFF) {
	    			pstBase->ucError = 1;
	    		}
	    	}
	    }

		if (*pstStage->ppstReduceSwitch) {
			pifSwitch_DetachEvtChange(*pstStage->ppstReduceSwitch);
		}
		if (*pstStage->ppstStopSwitch) {
			pifSwitch_DetachEvtChange(*pstStage->ppstStopSwitch);
		}

#ifndef __PIF_NO_LOG__
		if (pif_stLogFlag.btDutyMotor) {
			pifLog_Printf(LT_enInfo, "DMS:%u(%u) Stop D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
		}
#endif
    }
}

static void _SwitchReduceChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;

	(void)usPifId;

	if (pstBase->stOwner.enState >= MS_enReduce) return;

	if (swState) {
		pifDutyMotorSpeed_Stop((PIF_stDutyMotor *)pstBase);
	}
}

static void _SwitchStopChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;

	(void)usPifId;

	if (pstBase->stOwner.enState >= MS_enBreak) return;

	if (swState) {
		((PIF_stDutyMotor *)pstBase)->usCurrentDuty = 0;
		pstBase->stOwner.enState = MS_enBreak;
	}
}

/**
 * @fn pifDutyMotorSpeed_Add
 * @brief 
 * @param usPifId
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PIF_stDutyMotor *pifDutyMotorSpeed_Add(PIF_usId usPifId, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
    PIF_stDutyMotor *pstOwner = pifDutyMotor_Add(usPifId, usMaxDuty);
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    if (!pstOwner) goto fail_clear;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail_clear;

    pstBase->pvInfo = calloc(sizeof(PIF_stDutyMotorSpeedInfo), 1);
    if (!pstBase->pvInfo) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstBase->pstTimerDelay = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
    if (!pstBase->pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstBase->pstTimerDelay, _TimerDelayFinish, pstBase);

	pstBase->fnControl = _ControlSpeed;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) MD:%u P:%u EC:%u", __LINE__, usPifId, usMaxDuty, usControlPeriod, pif_enError);
#endif
fail_clear:
    if (pstOwner) {
        if (pstBase->pvInfo) {
            free(pstBase->pvInfo);
            pstBase->pvInfo = NULL;
        }
    }
    return NULL;
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
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    if (!pstBase->pvInfo) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    for (int i = 0; i < ucStageSize; i++) {
    	if ((pstStages[i].enMode & MM_RT_enMask) == MM_RT_enPulse) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    }

    PIF_stDutyMotorSpeedInfo *pstInfo = pstBase->pvInfo;
    pstInfo->ucStageSize = ucStageSize;
    pstInfo->pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageSize, pif_enError);
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
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    PIF_stDutyMotorSpeedInfo *pstInfo = pstBase->pvInfo;
    const PIF_stDutyMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstBase->actSetDuty || !pstBase->actSetDirection) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstInfo->pstCurrentStage = &pstInfo->pstStages[ucStageIndex];
    pstStage = pstInfo->pstCurrentStage;

    if (pstStage->enMode & MM_CIAS_enMask) {
    	ucState = 0;
    	if (*pstStage->ppstStartSwitch) {
    		if ((*pstStage->ppstStartSwitch)->swCurrState != ON) {
    			ucState |= 1;
    		}
    	}
    	if (*pstStage->ppstReduceSwitch) {
    		if ((*pstStage->ppstReduceSwitch)->swCurrState != OFF) {
    			ucState |= 2;
    		}
    	}
    	if (*pstStage->ppstStopSwitch) {
    		if ((*pstStage->ppstStopSwitch)->swCurrState != OFF) {
    			ucState |= 4;
    		}
    	}
    	if (ucState) {
#ifndef __PIF_NO_LOG__
    		pifLog_Printf(LT_enError, "DMS:%u(%u) S:%u", __LINE__, pstOwner->usPifId, ucState);
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

    if (*pstStage->ppstReduceSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstReduceSwitch, _SwitchReduceChange, pstBase);
    }

    if (*pstStage->ppstStopSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstStopSwitch, _SwitchStopChange, pstBase);
    }

    if (pstBase->actSetDirection) (*pstBase->actSetDirection)((pstStage->enMode & MM_D_enMask) >> MM_D_enShift);

    if (pstStage->usGsCtrlDuty) {
    	pstOwner->usCurrentDuty = pstStage->usGsStartDuty;
        pstOwner->enState = MS_enGained;
    }
    else {
    	pstOwner->usCurrentDuty = pstStage->usFsHighDuty;
        pstOwner->enState = MS_enConst;
    }
    pstBase->ucError = 0;

    (*pstBase->actSetDuty)(pstOwner->usCurrentDuty);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMS:%u(%u) Start", __LINE__, pstOwner->usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMS:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageIndex, pif_enError);
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
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    const PIF_stDutyMotorSpeedStage *pstStage = ((PIF_stDutyMotorSpeedInfo *)pstBase->pvInfo)->pstCurrentStage;

    if (pstOwner->enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->enState = MS_enOverRun;
    }
    else {
        pstOwner->enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMS:%u(%u) Stop OT=%u", __LINE__, pstOwner->usPifId, pstStage->usFsOverTime);
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
    pstOwner->usCurrentDuty = 0;
    pstOwner->enState = MS_enReduce;
}
