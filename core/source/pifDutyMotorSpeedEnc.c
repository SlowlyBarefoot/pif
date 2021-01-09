#include <string.h>

#include "pifLog.h"
#include "pifDutyMotorSpeedEnc.h"


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
    PIF_stDutyMotorSpeedEncInfo *pstInfo = pstBase->pvInfo;
	const PIF_stDutyMotorSpeedEncStage *pstStage = pstInfo->pstCurrentStage;

	usTmpEnc = pstInfo->usMeasureEnc;
	pstInfo->usMeasureEnc = 0;
	usTmpDuty = pstOwner->usCurrentDuty;

    pstInfo->unEncSampleSum -= pstInfo->ausEncSample[pstInfo->ucEncSampleIdx];
	pstInfo->unEncSampleSum += usTmpEnc;
    pstInfo->ausEncSample[pstInfo->ucEncSampleIdx] = usTmpEnc;
    pstInfo->ucEncSampleIdx = (pstInfo->ucEncSampleIdx + 1) % MAX_STABLE_CNT;

	if (pstOwner->enState == MS_enGained) {
		if (usTmpEnc >= pstInfo->usArrivePPR) {
			pstOwner->enState = MS_enStable;
            if (!pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usFsStableTimeout)) {
                pstBase->ucError = 1;
            }

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Stable E:%u D:%u", __LINE__, pstOwner->usPifId, usTmpEnc, usTmpDuty);
			}
#endif
		}
		else {
			usTmpDuty += pstStage->usGsCtrlDuty;
			if (usTmpDuty >= pstStage->usFsHighDuty) usTmpDuty = pstStage->usFsHighDuty;
		}
	}
	else if (pstOwner->enState == MS_enStable) {
		usEncAverage = pstInfo->unEncSampleSum / MAX_STABLE_CNT;

        if (usEncAverage >= pstInfo->usErrLowPPR && usEncAverage <= pstInfo->usErrHighPPR) {
            pifPulse_StopItem(pstBase->pstTimerDelay);
            pstOwner->enState = MS_enConst;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Const E:%u D:%u", __LINE__, pstOwner->usPifId, usTmpEnc, usTmpDuty);
			}
#endif
        }
    }
	else if (pstOwner->enState == MS_enReduce) {
		if (!pstStage->usRsCtrlDuty) {
			usTmpDuty = 0;
			pstOwner->enState = MS_enBreak;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Break E:%u D:%u", __LINE__, pstOwner->usPifId, usTmpEnc, usTmpDuty);
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
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Break E:%u D:%u", __LINE__, pstOwner->usPifId, usTmpEnc, usTmpDuty);
			}
#endif
		}
	}

	if ((pstOwner->enState == MS_enStable) || (pstOwner->enState == MS_enConst)) {
		fCtrlDuty = pifPidControl_Calcurate(&pstInfo->stPidControl, pstStage->fFsPulsesPerRange - usTmpEnc);

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
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Breaking D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
			}
#endif
    	}
    	else {
			if (pstBase->actOperateBreak) (*pstBase->actOperateBreak)(1);
    		pstOwner->enState = MS_enStopping;

#ifndef __PIF_NO_LOG__
			if (pif_stLogFlag.btDutyMotor) {
				pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Stopping D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
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
			pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Stop D:%u", __LINE__, pstOwner->usPifId, usTmpDuty);
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

    pstBase->pvInfo = calloc(sizeof(PIF_stDutyMotorSpeedEncInfo), 1);
    if (!pstBase->pvInfo) {
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
        if (pstBase->pvInfo) {
            free(pstBase->pvInfo);
            pstBase->pvInfo = NULL;
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

    if (!pstBase->pvInfo) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stDutyMotorSpeedEncInfo *pstInfo = pstBase->pvInfo;
    pstInfo->ucStageSize = ucStageSize;
    pstInfo->pstStages = pstStages;
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
	return &((PIF_stDutyMotorSpeedEncInfo *)((PIF_stDutyMotorBase *)pstOwner)->pvInfo)->stPidControl;
}

/**
 * @fn pifDutyMotorSpeedEnc_Start
 * @brief 
 * @param pstOwner
 * @param ucStageIndex
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;
    PIF_stDutyMotorSpeedEncInfo *pstInfo = pstBase->pvInfo;
    const PIF_stDutyMotorSpeedEncStage *pstStage;
    BOOL bState;

    if (!pstBase->actSetDuty) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstInfo->pstCurrentStage = &pstInfo->pstStages[ucStageIndex];
    pstStage = pstInfo->pstCurrentStage;

    if (pstStage->enMode & MM_CIAS_enMask) {
    	bState = ON;
    	if (*pstStage->ppstStartSwitch) {
    		if ((*pstStage->ppstStartSwitch)->swCurrState != ON) {
    	    	bState = OFF;
    		}
    	}
    	if (*pstStage->ppstReduceSwitch) {
    		if ((*pstStage->ppstReduceSwitch)->swCurrState != OFF) {
    	    	bState = OFF;
    		}
    	}
    	if (*pstStage->ppstStopSwitch) {
    		if ((*pstStage->ppstStopSwitch)->swCurrState != OFF) {
    	    	bState = OFF;
    		}
    	}
    	if (!bState) {
        	pif_enError = E_enInvalidState;
    		goto fail;
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

    if (!pifPulse_StartItem(pstBase->pstTimerDelay, pstStage->usGsArriveTimeout)) return FALSE;

    pstInfo->usArrivePPR = pstStage->fFsPulsesPerRange * pstStage->ucGsArriveRatio / 100;
    pstInfo->usErrLowPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrLow / 100;
    pstInfo->usErrHighPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrHigh / 100;
    pstInfo->ucEncSampleIdx = 0;
    memset(pstInfo->ausEncSample, 0, sizeof(pstInfo->ausEncSample));
    pstInfo->unEncSampleSum = 0;
    pstOwner->usCurrentDuty = pstStage->usGsStartDuty;
    pstInfo->usMeasureEnc = 0;
    pstOwner->enState = MS_enGained;
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
    const PIF_stDutyMotorSpeedEncStage *pstStage = ((PIF_stDutyMotorSpeedEncInfo *)pstBase->pvInfo)->pstCurrentStage;

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
    PIF_stDutyMotorSpeedEncInfo *pstInfo = ((PIF_stDutyMotorBase *)pstOwner)->pvInfo;

    pstInfo->usMeasureEnc++;
}
