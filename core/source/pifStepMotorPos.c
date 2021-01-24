#include "pifLog.h"
#include "pifStepMotorPos.h"


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "SMP:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _ControlPos(PIF_stStepMotor *pstOwner)
{
	uint16_t usTmpPps = 0;
    PIF_stStepMotorPos *pstPos = pstOwner->__pvChild;
    const PIF_stStepMotorPosStage *pstStage = pstPos->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpPps = pstOwner->_usCurrentPps;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpPps >= pstStage->usFsHighPps) {
			usTmpPps = pstStage->usFsHighPps;
			pstOwner->_enState = MS_enConst;
			if (pstOwner->evtStable) (*pstOwner->evtStable)(pstOwner, pstOwner->__pvChild);

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else {
			usTmpPps += pstStage->usGsCtrlPps;
		}
	}
	else if (pstOwner->_enState == MS_enReduce) {
		if (!pstStage->usRsCtrlPps) {
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpPps = pstStage->usRsLowPps;
				pstOwner->_enState = MS_enLowConst;
			}
			else {
				usTmpPps = 0;
				pstOwner->_enState = MS_enBreak;
			}

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else if (usTmpPps > pstStage->usRsLowPps + pstStage->usRsCtrlPps) {
			usTmpPps -= pstStage->usRsCtrlPps;
		}
		else if (usTmpPps) {
			usTmpPps = pstStage->usRsLowPps;
			pstOwner->_enState = MS_enLowConst;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstOwner->_unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpPps = 0;
			if (pstOwner->_enState < MS_enBreak) {
				pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
			}
		}
		else if (pstOwner->_unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_enState < MS_enReduce) {
				pstOwner->_enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
			}
		}
	}

	if (usTmpPps != pstOwner->_usCurrentPps) {
		if (usTmpPps) pifStepMotor_SetPps(pstOwner, usTmpPps);
		pstOwner->_usCurrentPps = usTmpPps;
	}

    if (pstOwner->_enState == MS_enBreak) {
    	pifStepMotor_Break(pstOwner);
		if (pstStage->usRsBreakTime &&
				pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usRsBreakTime)) {
			pstOwner->_enState = MS_enBreaking;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
    	}
    	else {
    		pstOwner->_enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
    	}
	}

    if (pstOwner->_enState == MS_enStopping) {
		if (!(pstStage->enMode & MM_NR_enMask)) {
			pifStepMotor_Release(pstOwner);
		}
		pstOwner->_enState = MS_enStop;

		if (pstStage->enMode & MM_CFPS_enMask) {
	    	if (*pstStage->ppstStopSwitch) {
	    		if ((*pstStage->ppstStopSwitch)->_swCurrState == OFF) {
	    			pstOwner->__ucError = 1;
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
		nLine = __LINE__;
#endif
    }

#ifndef __PIF_NO_LOG__
	if (nLine && pif_stLogFlag.btStepMotor) {
		pifLog_Printf(LT_enInfo, "SMP:%u(%u) %s P/S:%u P:%u", nLine, pstOwner->_usPifId,
				c_cMotorState[pstOwner->_enState], usTmpPps, pstOwner->_unCurrentPulse);
	}
#endif
}

static void _SwitchReduceChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (swState) {
		pifStepMotorPos_Stop(pstOwner);
	}
}

static void _SwitchStopChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stStepMotor *pstOwner = (PIF_stStepMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enBreak) return;

	if (swState) {
		pstOwner->_enState = MS_enBreak;
	}
}

static void _StopStep(PIF_stStepMotor *pstOwner)
{
    pstOwner->_usCurrentPps = 0;
	pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "evtStopStep(%d, %d)", pstOwner->_unCurrentPulse, pstOwner->_enState);
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
    if (!pstOwner) goto fail_clear;

    if (!pifStepMotor_InitControl(pstOwner, usControlPeriodMs)) goto fail_clear;

    pstOwner->__pvChild = calloc(sizeof(PIF_stStepMotorPos), 1);
    if (!pstOwner->__pvChild) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->__pstTimerDelay = pifPulse_AddItem(g_pstStepMotorTimer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _TimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _ControlPos;
	pstOwner->__fnStopStep = _StopStep;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) R:%u O:%d P:%u EC:%u", __LINE__, usPifId, ucResolution, enOperation, usControlPeriodMs, pif_enError);
#endif
fail_clear:
    if (pstOwner) {
        if (pstOwner->__pvChild) {
            free(pstOwner->__pvChild);
            pstOwner->__pvChild = NULL;
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
    if (!pstOwner->__pvChild) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_SC_enMask) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    }

    PIF_stStepMotorPos *pstPos = pstOwner->__pvChild;
    pstPos->__ucStageSize = ucStageSize;
    pstPos->__pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageSize, pif_enError);
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
    PIF_stStepMotorPos *pstPos = pstOwner->__pvChild;
    const PIF_stStepMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetStep) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstPos->__pstCurrentStage = &pstPos->__pstStages[ucStageIndex];
    pstStage = pstPos->__pstCurrentStage;

    if (pstStage->enMode & MM_CIAS_enMask) {
    	ucState = 0;
    	if (*pstStage->ppstStartSwitch) {
    		if ((*pstStage->ppstStartSwitch)->_swCurrState != ON) {
    			ucState |= 1;
    		}
    	}
    	if (*pstStage->ppstReduceSwitch) {
    		if ((*pstStage->ppstReduceSwitch)->_swCurrState != OFF) {
    			ucState |= 2;
    		}
    	}
    	if (*pstStage->ppstStopSwitch) {
    		if ((*pstStage->ppstStopSwitch)->_swCurrState != OFF) {
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
    		if (!pifStepMotor_SetOperatingTime(pstOwner, unOperatingTime)) goto fail;
    	}
    }

    if (!pifStepMotor_StartControl(pstOwner)) goto fail;

    pstPos->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstReduceSwitch, _SwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstStopSwitch, _SwitchStopChange, pstOwner);
    }

    pstOwner->_ucDirection = (pstStage->enMode & MM_D_enMask) >> MM_D_enShift;

    if (pstStage->usGsCtrlPps) {
    	pifStepMotor_SetPps(pstOwner, pstStage->usGsStartPps);
        pstOwner->_enState = MS_enGained;
    }
    else {
    	pifStepMotor_SetPps(pstOwner, pstStage->usFsHighPps);
        pstOwner->_enState = MS_enConst;
    }
    pstOwner->__ucError = 0;

    if (pstStage->enMode & MM_PC_enMask) {
    	pifStepMotor_Start(pstOwner, pstStage->unTotalPulse);
    }
    else {
    	pifStepMotor_Start(pstOwner, 0);
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Start", __LINE__, pstOwner->_usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SMP:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageIndex, pif_enError);
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
    if (pstOwner->_enState == MS_enIdle) return;

    pstOwner->_enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btStepMotor) {
    	pifLog_Printf(LT_enInfo, "SMP:%u(%u) Stop", __LINE__, pstOwner->_usPifId);
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
    pstOwner->_enState = MS_enBreak;
}
