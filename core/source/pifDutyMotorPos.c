#include "pifLog.h"
#include "pifDutyMotorPos.h"


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->_enState) {
	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMP:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _ControlPos(PIF_stDutyMotor *pstOwner)
{
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorPos *pstPos = pstOwner->__pvChild;
    const PIF_stDutyMotorPosStage *pstStage = pstPos->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpDuty = pstOwner->_usCurrentDuty;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->_enState = MS_enConst;
			if (pstOwner->__evtStable) (*pstOwner->__evtStable)(pstOwner, pstOwner->__pvChild);

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
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpDuty = pstStage->usRsLowDuty;
				pstOwner->_enState = MS_enLowConst;
			}
			else {
				usTmpDuty = 0;
				pstOwner->_enState = MS_enBreak;
			}

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
		else if (usTmpDuty > pstStage->usRsLowDuty + pstStage->usRsCtrlDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = pstStage->usRsLowDuty;
			pstOwner->_enState = MS_enLowConst;

#ifndef __PIF_NO_LOG__
			nLine = __LINE__;
#endif
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstPos->_unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpDuty = 0;
			if (pstOwner->_enState < MS_enBreak) {
				pstOwner->_enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
			}
		}
		else if (pstPos->_unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->_enState < MS_enReduce) {
				pstOwner->_enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
				nLine = __LINE__;
#endif
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
	if (nLine && pif_stLogFlag.btDutyMotor) {
		pifLog_Printf(LT_enInfo, "DMP:%u(%u) %s D:%u P:%u", nLine, pstOwner->_usPifId, c_cMotorState[pstOwner->_enState], usTmpDuty, pstPos->_unCurrentPulse);
	}
#endif
}

static void _SwitchReduceChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (swState) {
		pifDutyMotorPos_Stop(pstOwner);
	}
}

static void _SwitchStopChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)usPifId;

	if (pstOwner->_enState >= MS_enBreak) return;

	if (swState) {
		pstOwner->_usCurrentDuty = 0;
		pstOwner->_enState = MS_enBreak;
	}
}

/**
 * @fn pifDutyMotorPos_Add
 * @brief 
 * @param usPifId
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PIF_stDutyMotor *pifDutyMotorPos_Add(PIF_usId usPifId, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
    PIF_stDutyMotor *pstOwner = pifDutyMotor_Add(usPifId, usMaxDuty);
    if (!pstOwner) goto fail_clear;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail_clear;

    pstOwner->__pvChild = calloc(sizeof(PIF_stDutyMotorPos), 1);
    if (!pstOwner->__pvChild) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->__pstTimerDelay = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _TimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _ControlPos;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) MD:%u P:%u EC:%u", __LINE__, usPifId, usMaxDuty, usControlPeriod, pif_enError);
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
 * @fn pifDutyMotorPos_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorPos_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorPosStage *pstStages)
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

    PIF_stDutyMotorPos *pstPos = pstOwner->__pvChild;
    pstPos->__ucStageSize = ucStageSize;
    pstPos->__pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageSize, pif_enError);
#endif
    return FALSE;
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
    PIF_stDutyMotorPos *pstPos = pstOwner->__pvChild;
    const PIF_stDutyMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetDuty || !pstOwner->__actSetDirection) {
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
    		pifLog_Printf(LT_enError, "DMP:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, ucState);
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

    pstPos->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstReduceSwitch, _SwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstStopSwitch, _SwitchStopChange, pstOwner);
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

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Start", __LINE__, pstOwner->_usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageIndex, pif_enError);
#endif
    return FALSE;
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

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Stop", __LINE__, pstOwner->_usPifId);
    }
#endif
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
    PIF_stDutyMotorPos *pstPos = pstOwner->__pvChild;

    pstPos->_unCurrentPulse++;
	if (pstPos->__pstCurrentStage->enMode & MM_PC_enMask) {
		if (pstOwner->_enState && pstOwner->_enState < MS_enStop) {
			if (pstOwner->_usCurrentDuty && pstPos->_unCurrentPulse >= pstPos->__pstCurrentStage->unTotalPulse) {
				pstOwner->_usCurrentDuty = 0;
				(*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
		    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Signal", __LINE__, pstOwner->_usPifId);
			}
		}
	}
}
