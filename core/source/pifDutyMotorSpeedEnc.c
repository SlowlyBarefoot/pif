#include <string.h>

#include "pifLog.h"
#include "pifDutyMotorSpeedEnc.h"


typedef struct _PIF_stDutyMotorChild
{
	PIF_stDutyMotorSpeedEnc stSpeedEnc;

    uint8_t ucStageSize;
    const PIF_stDutyMotorSpeedEncStage *pstStages;
    const PIF_stDutyMotorSpeedEncStage *pstCurrentStage;

    PIF_stPidControl stPidControl;

	uint16_t usArrivePPR;
	uint16_t usErrLowPPR;
	uint16_t usErrHighPPR;
	volatile uint16_t usMeasureEnc;	// pulse
	uint8_t ucEncSampleIdx;
	uint16_t ausEncSample[MAX_STABLE_CNT];
	uint32_t unEncSampleSum;
} PIF_stDutyMotorChild;


static void _TimerDelayFinish(void *pvIssuer)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;
    PIF_stDutyMotor *pstOwner = &pstBase->stOwner;

	switch (pstOwner->enState) {
    case MS_enGained:
    case MS_enStable:
        pstBase->ucError = 1;
        break;

	case MS_enOverRun:
        pstOwner->enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMSE:%u(%u) S:%u", __LINE__, pstOwner->usPifId, pstOwner->enState);
#endif
		break;
	}
}

static void _ControlSpeedEnc(PIF_stDutyMotorBase *pstBase)
{
	PIF_stDutyMotor *pstOwner = &pstBase->stOwner;
	float fCtrlDuty = 0;
	uint16_t usTmpEnc = 0;
	uint16_t usTmpDuty = 0;
	uint16_t usEncAverage = 0;
    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;
	const PIF_stDutyMotorSpeedEncStage *pstStage = pstChild->pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpEnc = pstChild->usMeasureEnc;
	pstChild->usMeasureEnc = 0;
	usTmpDuty = pstOwner->usCurrentDuty;

	pstChild->unEncSampleSum -= pstChild->ausEncSample[pstChild->ucEncSampleIdx];
	pstChild->unEncSampleSum += usTmpEnc;
	pstChild->ausEncSample[pstChild->ucEncSampleIdx] = usTmpEnc;
	pstChild->ucEncSampleIdx = (pstChild->ucEncSampleIdx + 1) % MAX_STABLE_CNT;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpEnc >= pstChild->usArrivePPR) {
			pstOwner->enState = MS_enStable;
            if (!pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usFsStableTimeout)) {
                pstBase->ucError = 1;
            }

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
#endif
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
			if (usTmpDuty >= pstStage->usFsHighDuty) usTmpDuty = pstStage->usFsHighDuty;
		}
	}
	else if (pstOwner->enState == MS_enStable) {
		usEncAverage = pstChild->unEncSampleSum / MAX_STABLE_CNT;

        if (usEncAverage >= pstChild->usErrLowPPR && usEncAverage <= pstChild->usErrHighPPR) {
            pifPulse_StopItem(pstBase->pstTimerDelay);
            pstOwner->enState = MS_enConst;

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
#endif
        }
    }
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			usTmpDuty = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
#endif
		}
		else if (usTmpDuty > pstStage->usRsCtrlDuty && usTmpDuty > pstStage->usRsLowDuty) {
			usTmpDuty -= pstStage->usRsCtrlDuty;
		}
		else if (usTmpDuty) {
			usTmpDuty = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
#endif
		}
	}

	if (pstStage->enMode & MM_SC_enMask) {
		if ((pstOwner->enState == MS_enStable) || (pstOwner->enState == MS_enConst)) {
			fCtrlDuty = pifPidControl_Calcurate(&pstChild->stPidControl, pstStage->fFsPulsesPerRange - usTmpEnc);

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
            nLine = __LINE__;
#endif
    	}
    	else {
			if (pstBase->actOperateBreak) (*pstBase->actOperateBreak)(1);
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
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
	    		if ((*pstStage->ppstStopSwitch)->_swCurrState == OFF) {
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
        nLine = __LINE__;
#endif
    }

#ifndef __PIF_NO_LOG__
	if (nLine && pif_stLogFlag.btDutyMotor) {
		pifLog_Printf(LT_enInfo, "DMSE:%u(%u) %s E:%u D:%u", nLine, pstOwner->usPifId, c_cMotorState[pstOwner->enState], usTmpEnc, usTmpDuty);
	}
#endif
}

static void _SwitchReduceChange(PIF_usId usPifId, SWITCH swState, void *pvIssuer)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;

	(void)usPifId;

	if (pstBase->stOwner.enState >= MS_enReduce) return;

	if (swState) {
		pifDutyMotorSpeedEnc_Stop((PIF_stDutyMotor *)pstBase);
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
 * @fn pifDutyMotorSpeedEnc_Add
 * @brief 
 * @param usPifId
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PIF_stDutyMotor *pifDutyMotorSpeedEnc_Add(PIF_usId usPifId, uint16_t usMaxDuty, uint16_t usControlPeriod)
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

	pstBase->fnControl = _ControlSpeedEnc;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMSE:%u(%u) M:%u P:%u EC:%u", __LINE__, usPifId, usMaxDuty, usControlPeriod, pif_enError);
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
 * @fn pifDutyMotorSpeedEnc_AddStages
 * @brief 
 * @param pstOwner
 * @param ucStageSize
 * @param pstStages
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedEncStage *pstStages)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    if (!pstBase->pvChild) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_PC_enMask) {
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
	pifLog_Printf(LT_enError, "DMSE:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDutyMotorSpeedEnc_GetPidControl
 * @brief
 * @param pstOwner
 * @return
 */
PIF_stPidControl *pifDutyMotorSpeedEnc_GetPidControl(PIF_stDutyMotor *pstOwner)
{
	return &((PIF_stDutyMotorChild *)((PIF_stDutyMotorBase *)pstOwner)->pvChild)->stPidControl;
}

/**
 * @fn pifDutyMotorSpeedEnc_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @param unOperatingTime
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    PIF_stDutyMotorChild *pstChild = pstBase->pvChild;
    const PIF_stDutyMotorSpeedEncStage *pstStage;
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
    		pifLog_Printf(LT_enError, "DMSE:%u(%u) S:%u", __LINE__, pstOwner->usPifId, ucState);
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

    pstChild->stSpeedEnc.ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstReduceSwitch, _SwitchReduceChange, pstBase);
    }

    if (*pstStage->ppstStopSwitch) {
        pifSwitch_AttachEvtChange(*pstStage->ppstStopSwitch, _SwitchStopChange, pstBase);
    }

    if (pstBase->actSetDirection) (*pstBase->actSetDirection)((pstStage->enMode & MM_D_enMask) >> MM_D_enShift);

    if (pstStage->usGsCtrlDuty) {
		if (!pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usGsArriveTimeout)) return FALSE;

		pstChild->usArrivePPR = pstStage->fFsPulsesPerRange * pstStage->ucGsArriveRatio / 100;
		pstOwner->usCurrentDuty = pstStage->usGsStartDuty;
		pstOwner->enState = MS_enGained;
    }
    else {
    	pstOwner->usCurrentDuty = pstStage->usFsHighDuty;
        pstOwner->enState = MS_enConst;
    }
    pstChild->usErrLowPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrLow / 100;
    pstChild->usErrHighPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrHigh / 100;
    pstChild->ucEncSampleIdx = 0;
    memset(pstChild->ausEncSample, 0, sizeof(pstChild->ausEncSample));
    pstChild->unEncSampleSum = 0;
    pstChild->usMeasureEnc = 0;
    pstBase->ucError = 0;

    (*pstBase->actSetDuty)(pstOwner->usCurrentDuty);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Start P/R:%d", __LINE__, pstOwner->usPifId, (int)pstStage->fFsPulsesPerRange);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMSE:%u(%u) S:%u EC:%u", __LINE__, pstOwner->usPifId, ucStageIndex, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDutyMotorSpeedEnc_Stop
 * @brief 
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_Stop(PIF_stDutyMotor *pstOwner)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    const PIF_stDutyMotorSpeedEncStage *pstStage = ((PIF_stDutyMotorChild *)pstBase->pvChild)->pstCurrentStage;

    if (pstOwner->enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->enState = MS_enOverRun;
    }
    else {
        pstOwner->enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Stop OT=%u", __LINE__, pstOwner->usPifId, pstStage->usFsOverTime);
    }
#endif
}

/**
 * @fn pifDutyMotorSpeedEnc_Emergency
 * @brief
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_Emergency(PIF_stDutyMotor *pstOwner)
{
    pstOwner->usCurrentDuty = 0;
    pstOwner->enState = MS_enBreak;
}

/**
 * @fn pifDutyMotorSpeedEnc_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_sigEncoder(PIF_stDutyMotor *pstOwner)
{
    ((PIF_stDutyMotorChild *)((PIF_stDutyMotorBase *)pstOwner)->pvChild)->usMeasureEnc++;
}
