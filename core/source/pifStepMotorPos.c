#include "pifLog.h"
#include "pifStepMotorPos.h"


typedef struct _PIF_stStepMotorChild
{
	PIF_stStepMotorPos stPos;

    uint8_t ucStageSize;
    const PIF_stStepMotorPosStage *pstStages;
    const PIF_stStepMotorPosStage *pstCurrentStage;
} PIF_stStepMotorChild;


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	switch (pstOwner->enState) {
	case MS_enBreaking:
        pstOwner->enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "SMP:%u(%u) S:%u", __LINE__, pstOwner->usPifId, pstOwner->enState);
#endif
		break;
	}
}

static void _ControlPos(PIF_stStepMotorBase *pstBase)
{
	PIF_stStepMotor *pstOwner = &pstBase->stOwner;
	uint16_t usTmpPps = 0;
    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    const PIF_stStepMotorPosStage *pstStage = pstChild->pstCurrentStage;

	usTmpPps = pstOwner->usCurrentPps;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpPps >= pstStage->usFsHighPps) {
			usTmpPps = pstStage->usFsHighPps;
			pstOwner->enState = MS_enConst;
			if (pstBase->evtStable) (*pstBase->evtStable)(pstOwner, pstBase->pvChild);

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMP:%u(%u) Const P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlPps) {
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpPps = pstStage->usRsLowPps;
				pstOwner->enState = MS_enLowConst;
			}
			else {
				usTmpPps = 0;
				pstOwner->enState = MS_enBreak;
			}

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMP:%u(%u) LowConst P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
		}
		else if (usTmpPps > pstStage->usRsLowPps + pstStage->usRsCtrlPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = pstStage->usRsLowPps;
			pstOwner->enState = MS_enLowConst;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMP:%u(%u) LowConst P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstOwner->unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpPps = 0;
			if (pstOwner->enState < MS_enBreak) {
				pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
				if (pif_stLogFlag.btStepMotor) {
					pifLog_Printf(LT_enInfo, "SMP:%u(%u) Break D:%u S:%u", __LINE__, pstOwner->usPifId, usTmpPps, pstOwner->unCurrentPulse);
				}
#endif
			}
		}
		else if (pstOwner->unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->enState < MS_enReduce) {
				pstOwner->enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
				if (pif_stLogFlag.btStepMotor) {
					pifLog_Printf(LT_enInfo, "SMP:%u(%u) Reduce D:%u S:%u", __LINE__, pstOwner->usPifId, usTmpPps, pstOwner->unCurrentPulse);
				}
#endif
			}
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
				pifLog_Printf(LT_enInfo, "SMP:%u(%u) Breaking P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
			}
#endif
    	}
    	else {
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btStepMotor) {
				pifLog_Printf(LT_enInfo, "SMP:%u(%u) Stopping P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
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
			pifLog_Printf(LT_enInfo, "SMP:%u(%u) Stop P/S:%u", __LINE__, pstOwner->usPifId, usTmpPps);
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
		pifStepMotorPos_Stop((PIF_stStepMotor *)pstBase);
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

static void _StopStep(PIF_stStepMotorBase *pstBase)
{
    PIF_stStepMotor *pstOwner = &pstBase->stOwner;

    pstOwner->usCurrentPps = 0;
	pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "evtStopStep(%d, %d)", pstBase->stOwner.unCurrentPulse, pstOwner->enState);
    }
#endif
}

/**
 * @fn pifStepMotorPos_Add
 * @brief 
 * @param usPifId
 * @param ucResolution
 * @param enOperation
 * @param usControlPeriodMs
 * @return
 */
PIF_stStepMotor *pifStepMotorPos_Add(PIF_usId usPifId, uint8_t ucResolution, PIF_enStepMotorOperation enOperation,
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

	pstBase->fnControl = _ControlPos;
	pstBase->fnStopStep = _StopStep;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) R:%u O:%d P:%u EC:%u", __LINE__, usPifId, ucResolution, enOperation, usControlPeriodMs, pif_enError);
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
 * @fn pifStepMotorPos_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifStepMotorPos_AddStages(PIF_stStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorPosStage *pstStages)
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
    }

    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    pstChild->ucStageSize = ucStageSize;
    pstChild->pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageSize, pif_enError);
#endif
    return FALSE;
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
	PIF_stStepMotorBase *pstBase = (PIF_stStepMotorBase *)pstOwner;
    PIF_stStepMotorChild *pstChild = pstBase->pvChild;
    const PIF_stStepMotorPosStage *pstStage;
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

    pstChild->stPos.ucStageIndex = ucStageIndex;

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
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsHighPps);
        pstOwner->enState = MS_enConst;
    }
    pstBase->ucError = 0;

    if (pstStage->enMode & MM_PC_enMask) {
    	pifStepMotor_Start(pstOwner, pstStage->unTotalPulse);
    }
    else {
    	pifStepMotor_Start(pstOwner, 0);
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Start", __LINE__, pstOwner->usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageIndex, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifStepMotorPos_Stop
 * @brief 
 * @param pstOwner
 */
void pifStepMotorPos_Stop(PIF_stStepMotor *pstOwner)
{
    if (pstOwner->enState == MS_enIdle) return;

    pstOwner->enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Stop", __LINE__, pstOwner->usPifId);
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
    pstOwner->enState = MS_enBreak;
}
