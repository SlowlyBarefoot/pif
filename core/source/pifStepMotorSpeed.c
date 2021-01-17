#include "pifLog.h"
#include "pifStepMotorSpeed.h"


typedef struct _PIF_stStepMotorChild
{
	PIF_stStepMotorSpeed stSpeed;

    uint8_t ucStageSize;
    const PIF_stStepMotorSpeedStage *pstStages;
    const PIF_stStepMotorSpeedStage *pstCurrentStage;
} PIF_stStepMotorChild;


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	switch (pstOwner->enState) {
	case MS_enOverRun:
        pstOwner->enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "SMS:%u(%u) S:%u", __LINE__, pstOwner->usPifId, pstOwner->enState);
#endif
		break;
	}
}

static void _ControlSpeed(PIF_stStepMotorBase *pstBase)
{
	PIF_stStepMotor *pstOwner = &pstBase->stOwner;
	uint16_t usTmpPps = 0;
    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    const PIF_stStepMotorSpeedStage *pstStage = pstChild->pstCurrentStage;

	usTmpPps = pstOwner->usCurrentPps;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpPps >= pstStage->usFsFixedPps) {
			usTmpPps = pstStage->usFsFixedPps;
			pstOwner->enState = MS_enConst;
			if (pstBase->evtStable) (*pstBase->evtStable)(pstOwner, pstBase->pvChild);

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMS:%u(%u) Const P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlPps) {
			usTmpPps = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMS:%u(%u) Break", __LINE__, pstOwner->usPifId);
			}
#endif
		}
		else if (usTmpPps > pstStage->usRsCtrlPps && usTmpPps > pstStage->usRsStopPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMS:%u(%u) Break", __LINE__, pstOwner->usPifId);
			}
#endif
		}
	}

	if (usTmpPps != pstOwner->usCurrentPps) {
		if (usTmpPps) pifStepMotor_SetPps(pstOwner, usTmpPps);
		pstOwner->usCurrentPps = usTmpPps;
	}

    if (pstOwner->enState == MS_enBreak) {
    	pifStepMotor_Break(pstOwner);
		if (pstStage->usRsBreakTime &&
				pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usRsBreakTime)) {
			pstOwner->enState = MS_enBreaking;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMS:%u(%u) Breaking P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
    	}
    	else {
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMS:%u(%u) Stopping P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
    	}
	}

    if (pstOwner->enState == MS_enStopping) {
		if (!(pstStage->enMode & MM_NR_enMask)) {
			pifStepMotor_Release(pstOwner);
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
		if (pif_stLogFlag.btStepMotor) {
			pifLog_Printf(LT_enInfo, "SMS:%u(%u) Stop P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
		}
#endif
    }
}

static void _SwitchReduceChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pvIssuer;

	(void)usPifId;

	if (pstBase->stOwner.enState >= MS_enReduce) return;

	if (swState) {
		pifStepMotorSpeed_Stop((PIF_stStepMotor *)pstBase);
	}
}

static void _SwitchStopChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pvIssuer;

	(void)usPifId;

	if (pstBase->stOwner.enState >= MS_enBreak) return;

	if (swState) {
		pstBase->stOwner.enState = MS_enBreak;
	}
}

/**
 * @fn pifStepMotorSpeed_Add
 * @brief 
 * @param usPifId
 * @param ucResolution
 * @param enOperation
 * @param usControlPeriodMs
 * @return 
 */
PIF_stStepMotor *pifStepMotorSpeed_Add(PIF_usId usPifId, uint8_t ucResolution, PIF_enStepMotorOperation enOperation,
		uint16_t usControlPeriodMs)
{
    PIF_stStepMotor *pstOwner = pifStepMotor_Add(usPifId, ucResolution, enOperation);
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;
    if (!pstOwner) goto fail_clear;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail_clear;

    pstBase->pvChild = calloc(sizeof(PIF_stStepMotorChild), 1);
    if (!pstBase->pvChild) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstBase->pstTimerDelay = pifPulse_AddItem(g_pstStepMotorTimer, PT_enOnce);
    if (!pstBase->pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstBase->pstTimerDelay, _TimerDelayFinish, pstBase);

	pstBase->fnControl = _ControlSpeed;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMS:%u(%u) R:%u O:%d P:%u EC:%u", __LINE__, usPifId, ucResolution, enOperation, usControlPeriodMs, pif_enError);
#endif
fail_clear:
    if (pstOwner) {
        if (pstBase->pvChild) {
            free(pstBase->pvChild);
            pstBase->pvChild = NULL;
        }
    }
    return NULL;
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
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;

    if (!pstBase->pvChild) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

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

    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    pstChild->ucStageSize = ucStageSize;
    pstChild->pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMS:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageSize, pif_enError);
#endif
    return FALSE;
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
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;
    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    const PIF_stStepMotorSpeedStage *pstStage;
    uint8_t ucState;

    if (!pstBase->actSetStep) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstChild->pstCurrentStage = &pstChild->pstStages[ucStageIndex];
    pstStage = pstChild->pstCurrentStage;

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
    		if (!pifStepMotor_SetOperatingTime(pstOwner, unOperatingTime)) goto fail;
    	}
    }

    if (!pifStepMotor_StartControl(pstOwner)) goto fail;

    pstChild->stSpeed.ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstReduceSwitch, _SwitchReduceChange, pstBase);
    }

    if (*pstStage->ppstStopSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstStopSwitch, _SwitchStopChange, pstBase);
    }

    pstOwner->ucDirection = (pstStage->enMode & MM_D_enMask) >> MM_D_enShift;

    if (pstStage->usGsCtrlPps) {
    	pifStepMotor_SetPps(pstOwner, pstStage->usGsStartPps);
        pstOwner->enState = MS_enGained;
    }
    else {
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsFixedPps);
        pstOwner->enState = MS_enConst;
    }
    pstBase->ucError = 0;

    pifStepMotor_Start(pstOwner, 0);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMS:%u(%u) Start", __LINE__, pstOwner->usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMS:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageIndex, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifStepMotorSpeed_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorSpeed_Stop(PIF_stStepMotor *pstOwner)
{
    PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;
    const PIF_stStepMotorSpeedStage *pstStage = ((PIF_stStepMotorChild *)pstBase->pvChild)->pstCurrentStage;

    if (pstOwner->enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->enState = MS_enOverRun;
    }
    else {
        pstOwner->enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMS:%u(%u) Stop OT=%u", __LINE__, pstOwner->usPifId, pstStage->usFsOverTime);
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
    pstOwner->enState = MS_enBreak;
}
