#include <string.h>

#include "pifLog.h"
#include "pifDutyMotorSpeedEnc.h"


static void _evtTimerDelayFinish(void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	switch (pstOwner->_enState) {
    case MS_enGained:
    case MS_enStable:
        pstOwner->__ucError = 1;
        break;

	case MS_enOverRun:
        pstOwner->_enState = MS_enReduce;
        break;

	case MS_enBreaking:
        pstOwner->_enState = MS_enStopping;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "DMSE:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState);
#endif
		break;
	}
}

static void _fnControlSpeedEnc(PIF_stDutyMotor *pstOwner)
{
	float fCtrlDuty = 0;
	uint16_t usTmpEnc = 0;
	uint16_t usTmpDuty = 0;
	uint16_t usEncAverage = 0;
    PIF_stDutyMotorSpeedEnc *pstSpeedEnc = pstOwner->_pvChild;
	const PIF_stDutyMotorSpeedEncStage *pstStage = pstSpeedEnc->__pstCurrentStage;
#ifndef __PIF_NO_LOG__
    int nLine = 0;
#endif

	usTmpEnc = pstSpeedEnc->__usMeasureEnc;
	pstSpeedEnc->__usMeasureEnc = 0;
	usTmpDuty = pstOwner->_usCurrentDuty;

	pstSpeedEnc->__unEncSampleSum -= pstSpeedEnc->__ausEncSample[pstSpeedEnc->__ucEncSampleIdx];
	pstSpeedEnc->__unEncSampleSum += usTmpEnc;
	pstSpeedEnc->__ausEncSample[pstSpeedEnc->__ucEncSampleIdx] = usTmpEnc;
	pstSpeedEnc->__ucEncSampleIdx = (pstSpeedEnc->__ucEncSampleIdx + 1) % MAX_STABLE_CNT;

	if (pstOwner->_enState == MS_enGained) {
		if (usTmpEnc >= pstSpeedEnc->__usArrivePPR) {
			pstOwner->_enState = MS_enStable;
            if (!pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usFsStableTimeout)) {
                pstOwner->__ucError = 1;
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
	else if (pstOwner->_enState == MS_enStable) {
		usEncAverage = pstSpeedEnc->__unEncSampleSum / MAX_STABLE_CNT;

        if (usEncAverage >= pstSpeedEnc->__usErrLowPPR && usEncAverage <= pstSpeedEnc->__usErrHighPPR) {
            pifPulse_StopItem(pstOwner->__pstTimerDelay);
            pstOwner->_enState = MS_enConst;

#ifndef __PIF_NO_LOG__
            nLine = __LINE__;
#endif
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

	if (pstStage->enMode & MM_SC_enMask) {
		if ((pstOwner->_enState == MS_enStable) || (pstOwner->_enState == MS_enConst)) {
			fCtrlDuty = pifPidControl_Calcurate(&pstSpeedEnc->__stPidControl, pstStage->fFsPulsesPerRange - usTmpEnc);

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
	if (nLine && pif_stLogFlag.btDutyMotor) {
		pifLog_Printf(LT_enInfo, "DMSE:%u(%u) %s E:%u D:%u", nLine, pstOwner->_usPifId, c_cMotorState[pstOwner->_enState], usTmpEnc, usTmpDuty);
	}
#endif
}

static void _evtSwitchReduceChange(PIF_usId _usPifId, uint16_t usLevel, void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

	(void)_usPifId;

	if (pstOwner->_enState >= MS_enReduce) return;

	if (usLevel) {
		pifDutyMotorSpeedEnc_Stop(pstOwner);
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
 * @fn pifDutyMotorSpeedEnc_Add
 * @brief 
 * @param _usPifId
 * @param usMaxDuty
 * @param usControlPeriod
 * @return 
 */
PIF_stDutyMotor *pifDutyMotorSpeedEnc_Add(PIF_usId _usPifId, uint16_t usMaxDuty, uint16_t usControlPeriod)
{
    PIF_stDutyMotor *pstOwner = pifDutyMotor_Add(_usPifId, usMaxDuty);
    if (!pstOwner) goto fail_clear;

    if (!pifDutyMotor_InitControl(pstOwner, usControlPeriod)) goto fail_clear;

    pstOwner->_pvChild = calloc(sizeof(PIF_stDutyMotorSpeedEnc), 1);
    if (!pstOwner->_pvChild) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->__pstTimerDelay = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) goto fail_clear;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

	pstOwner->__fnControl = _fnControlSpeedEnc;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMSE:%u(%u) M:%u P:%u EC:%u", __LINE__, _usPifId, usMaxDuty, usControlPeriod, pif_enError);
#endif
fail_clear:
    if (pstOwner) {
        if (pstOwner->_pvChild) {
            free(pstOwner->_pvChild);
            pstOwner->_pvChild = NULL;
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
    if (!pstOwner->_pvChild) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    for (int i = 0; i < ucStageSize; i++) {
    	if (pstStages[i].enMode & MM_PC_enMask) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    }

    PIF_stDutyMotorSpeedEnc *pstSpeedEnc = pstOwner->_pvChild;
    pstSpeedEnc->__ucStageSize = ucStageSize;
    pstSpeedEnc->__pstStages = pstStages;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMSE:%u(%u) SS:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageSize, pif_enError);
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
	return &((PIF_stDutyMotorSpeedEnc *)pstOwner->_pvChild)->__stPidControl;
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
    PIF_stDutyMotorSpeedEnc *pstSpeedEnc = pstOwner->_pvChild;
    const PIF_stDutyMotorSpeedEncStage *pstStage;
    uint8_t ucState;

    if (!pstOwner->__actSetDuty || !pstOwner->__actSetDirection) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    pstSpeedEnc->__pstCurrentStage = &pstSpeedEnc->__pstStages[ucStageIndex];
    pstStage = pstSpeedEnc->__pstCurrentStage;

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
    		pifLog_Printf(LT_enError, "DMSE:%u(%u) S:%u", __LINE__, pstOwner->_usPifId, ucState);
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

    pstSpeedEnc->_ucStageIndex = ucStageIndex;

    if (*pstStage->ppstReduceSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstReduceSensor, _evtSwitchReduceChange, pstOwner);
    }

    if (*pstStage->ppstStopSensor) {
        pifSensor_AttachEvtChange(*pstStage->ppstStopSensor, _evtSwitchStopChange, pstOwner);
    }

    if (pstOwner->__actSetDirection) (*pstOwner->__actSetDirection)((pstStage->enMode & MM_D_enMask) >> MM_D_enShift);

    if (pstStage->usGsCtrlDuty) {
		if (!pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usGsArriveTimeout)) return FALSE;

		pstSpeedEnc->__usArrivePPR = pstStage->fFsPulsesPerRange * pstStage->ucGsArriveRatio / 100;
		pstOwner->_usCurrentDuty = pstStage->usGsStartDuty;
		pstOwner->_enState = MS_enGained;
    }
    else {
    	pstOwner->_usCurrentDuty = pstStage->usFsHighDuty;
        pstOwner->_enState = MS_enConst;
    }
    pstSpeedEnc->__usErrLowPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrLow / 100;
    pstSpeedEnc->__usErrHighPPR = pstStage->fFsPulsesPerRange * pstStage->ucFsStableErrHigh / 100;
    pstSpeedEnc->__ucEncSampleIdx = 0;
    memset(pstSpeedEnc->__ausEncSample, 0, sizeof(pstSpeedEnc->__ausEncSample));
    pstSpeedEnc->__unEncSampleSum = 0;
    pstSpeedEnc->__usMeasureEnc = 0;
    pstOwner->__ucError = 0;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Start P/R:%d", __LINE__, pstOwner->_usPifId, (int)pstStage->fFsPulsesPerRange);
    }
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DMSE:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, ucStageIndex, pif_enError);
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
    const PIF_stDutyMotorSpeedEncStage *pstStage = ((PIF_stDutyMotorSpeedEnc *)pstOwner->_pvChild)->__pstCurrentStage;

    if (pstOwner->_enState == MS_enIdle) return;

    if (pstStage->usFsOverTime && pifPulse_StartItem(pstOwner->__pstTimerDelay, pstStage->usFsOverTime)) {
        pstOwner->_enState = MS_enOverRun;
    }
    else {
        pstOwner->_enState = MS_enReduce;
    }

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btDutyMotor) {
    	pifLog_Printf(LT_enInfo, "DMSE:%u(%u) Stop OT=%u", __LINE__, pstOwner->_usPifId, pstStage->usFsOverTime);
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
    pstOwner->_usCurrentDuty = 0;
    pstOwner->_enState = MS_enBreak;
}

/**
 * @fn pifDutyMotorSpeedEnc_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifDutyMotorSpeedEnc_sigEncoder(PIF_stDutyMotor *pstOwner)
{
    ((PIF_stDutyMotorSpeedEnc *)pstOwner->_pvChild)->__usMeasureEnc++;
}
