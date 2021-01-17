#include "pifLog.h"
#include "pifDutyMotorPos.h"


typedef struct _PIF_stDutyMotorChild
{
	PIF_stDutyMotorPos stPos;

    uint8_t ucStageSize;
    const PIF_stDutyMotorPosStage *pstStages;
    const PIF_stDutyMotorPosStage *pstCurrentStage;
} PIF_stDutyMotorChild;


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->enState) {
	case MS_enBreaking:
        pstOwner->enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMP:%u(%u) S:%u", __LINE__, pstOwner->usPifId, pstOwner->enState);
#endif
		break;
	}
}

static void _ControlPos(PIF_stDutyMotorBase *pstBase)
{
	PIF_stDutyMotor *pstOwner = &pstBase->stOwner;
	uint16_t usTmpDuty = 0;
    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;
    const PIF_stDutyMotorPosStage *pstStage = pstChild->pstCurrentStage;

	usTmpDuty = pstOwner->usCurrentDuty;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpDuty >= pstStage->usFsHighDuty) {
			usTmpDuty = pstStage->usFsHighDuty;
			pstOwner->enState = MS_enConst;
			if (pstBase->evtStable) (*pstBase->evtStable)(pstOwner, pstBase->pvChild);

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMP:%u(%u) Const D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
			}
#endif
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
		}
	}
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			if (pstStage->enMode & MM_PC_enMask) {
				usTmpDuty = pstStage->usRsLowDuty;
				pstOwner->enState = MS_enLowConst;
			}
			else {
				usTmpDuty = 0;
				pstOwner->enState = MS_enBreak;
			}

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMP:%u(%u) LowConst/Break D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
			}
#endif
		}
		else if (usTmpDuty > pstStage->usRsLowDuty + pstStage->usRsCtrlDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = pstStage->usRsLowDuty;
			pstOwner->enState = MS_enLowConst;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMP:%u(%u) LowConst D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
			}
#endif
		}
	}

	if (pstStage->enMode & MM_PC_enMask) {
		if (pstChild->stPos.unCurrentPulse >= pstStage->unTotalPulse) {
			usTmpDuty = 0;
			if (pstOwner->enState < MS_enBreak) {
				pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
				if (pif_stLogFlag.btDutyMotor) {
					pifLog_Printf(LT_enInfo, "DMP:%u(%u) Break D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
				}
#endif
			}
		}
		else if (pstChild->stPos.unCurrentPulse >= pstStage->unFsPulseCount) {
			if (pstOwner->enState < MS_enReduce) {
				pstOwner->enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
				if (pif_stLogFlag.btDutyMotor) {
					pifLog_Printf(LT_enInfo, "DMP:%u(%u) Reduce D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
				}
#endif
			}
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
				pifLog_Printf(LT_enInfo, "DMP:%u(%u) Breaking D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
			}
#endif
    	}
    	else {
			if (pstBase->actOperateBreak) (*pstBase->actOperateBreak)(1);
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMP:%u(%u) Stopping D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
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
			pifLog_Printf(LT_enInfo, "DMP:%u(%u) Stop D:%u P:%u", __LINE__, pstOwner->usPifId, usTmpDuty, pstChild->stPos.unCurrentPulse);
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
		pifDutyMotorPos_Stop((PIF_stDutyMotor *)pstBase);
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
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    if (!pstOwner) goto fail_clear;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail_clear;

    pstBase->pvChild = calloc(sizeof(PIF_stDutyMotorChild), 1);
    if (!pstBase->pvChild) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstBase->pstTimerDelay = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
    if (!pstBase->pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstBase->pstTimerDelay, _TimerDelayFinish, pstBase);

	pstBase->fnControl = _ControlPos;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) MD:%u P:%u EC:%u", __LINE__, usPifId, usMaxDuty, usControlPeriod, pif_enError);
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
 * @fn pifDutyMotorPos_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorPos_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorPosStage *pstStages)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

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

    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;
    pstChild->ucStageSize = ucStageSize;
    pstChild->pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageSize, pif_enError);
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
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;
    const PIF_stDutyMotorPosStage *pstStage;
    uint8_t ucState;

    if (!pstBase->actSetDuty || !pstBase->actSetDirection) {
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
    		pifLog_Printf(LT_enError, "DMP:%u(%u) S:%u", __LINE__, pstOwner->usPifId, ucState);
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

    pstChild->stPos.ucStageIndex = ucStageIndex;

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
    pstChild->stPos.unCurrentPulse = 0;
    pstBase->ucError = 0;

    (*pstBase->actSetDuty)(pstOwner->usCurrentDuty);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Start", __LINE__, pstOwner->usPifId);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMP:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageIndex, pif_enError);
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
    if (pstOwner->enState == MS_enIdle) return;

    pstOwner->enState = MS_enReduce;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Stop", __LINE__, pstOwner->usPifId);
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
    pstOwner->usCurrentDuty = 0;
    pstOwner->enState = MS_enReduce;
}

/**
 * @fn pifDutyMotorPos_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorPos_sigEncoder(PIF_stDutyMotor *pstOwner)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;

    pstChild->stPos.unCurrentPulse++;
	if (pstChild->pstCurrentStage->enMode & MM_PC_enMask) {
		if (pstOwner->enState && pstOwner->enState < MS_enStop) {
			if (pstOwner->usCurrentDuty && pstChild->stPos.unCurrentPulse >= pstChild->pstCurrentStage->unTotalPulse) {
				pstOwner->usCurrentDuty = 0;
				(*pstBase->actSetDuty)(pstOwner->usCurrentDuty);
		    	pifLog_Printf(LT_enInfo, "DMP:%u(%u) Signal", __LINE__, pstOwner->usPifId);
			}
		}
	}
}
