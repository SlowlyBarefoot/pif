#include "pifLog.h"
#include "pifDutyMotorBase.h"


static PIF_stDutyMotorBase *s_pstDutyMotorBase;
static uint8_t s_ucDutyMotorBaseSize;
static uint8_t s_ucDutyMotorBasePos;

PIF_stPulse *g_pstDutyMotorTimer;


static void _TimerControlFinish(void *pvIssuer)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;
    PIF_stDutyMotor *pstOwner = &pstBase->stOwner;

    if (pstBase->fnControl) (*pstBase->fnControl)(pstBase);

	if (pstOwner->enState == MS_enStop) {
		pifPulse_StopItem(pstBase->pstTimerControl);
		pstOwner->enState = MS_enIdle;
		if (pstBase->evtStop) (*pstBase->evtStop)(pstOwner, pstBase->pvInfo);
	}
}

static void _TimerBreakFinish(void *pvIssuer)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pvIssuer;

	(*pstBase->actOperateBreak)(0);
}

/**
 * @fn pifDutyMotor_Init
 * @brief 
 * @param pstTimer
 * @param ucSize
 * @return 
 */
BOOL pifDutyMotor_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstDutyMotorBase = calloc(sizeof(PIF_stDutyMotorBase), ucSize);
    if (!s_pstDutyMotorBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucDutyMotorBaseSize = ucSize;
    s_ucDutyMotorBasePos = 0;

    g_pstDutyMotorTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DM:%u S:%u EC:%d", __LINE__, ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifDutyMotor_Exit
 * @brief 
 */
void pifDutyMotor_Exit()
{
    if (s_pstDutyMotorBase) {
		for (int i = 0; i < s_ucDutyMotorBaseSize; i++) {
			PIF_stDutyMotorBase *pstBase = &s_pstDutyMotorBase[i];
			if (pstBase->pvInfo) {
				free(pstBase->pvInfo);
				pstBase->pvInfo = NULL;
			}
		}
        free(s_pstDutyMotorBase);
        s_pstDutyMotorBase = NULL;
    }
}

/**
 * @fn pifDutyMotor_Add
 * @brief 
 * @param unDeviceCode
 * @param usMaxDuty
 * @return 
 */
PIF_stDutyMotor *pifDutyMotor_Add(PIF_unDeviceCode unDeviceCode, uint16_t usMaxDuty)
{
    PIF_stDutyMotorBase *pstBase = NULL;

    if (s_ucDutyMotorBasePos >= s_ucDutyMotorBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    pstBase = &s_pstDutyMotorBase[s_ucDutyMotorBasePos];
    PIF_stDutyMotor *pstOwner = &pstBase->stOwner;

    pstOwner->unDeviceCode = unDeviceCode;
    pstOwner->enState = MS_enIdle;
    pstOwner->usMaxDuty = usMaxDuty;

    s_ucDutyMotorBasePos = s_ucDutyMotorBasePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DM:%u(%u) MD:%u EC:%d", __LINE__, unDeviceCode, usMaxDuty, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifDutyMotor_AttachAction
 * @brief
 * @param pstOwner
 * @param actSetDuty
 * @param actSetDirection
 * @param actOperateBreak
 */
void pifDutyMotor_AttachAction(PIF_stDutyMotor *pstOwner, PIF_actDutyMotorSetDuty actSetDuty, PIF_actDutyMotorSetDirection actSetDirection,
		PIF_actDutyMotorOperateBreak actOperateBreak)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    pstBase->actSetDuty = actSetDuty;
    pstBase->actSetDirection = actSetDirection;
    pstBase->actOperateBreak = actOperateBreak;
}

/**
 * @fn pifDutyMotor_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtStable
 * @param evtStop
 * @param evtError
 */
void pifDutyMotor_AttachEvent(PIF_stDutyMotor *pstOwner, PIF_evtDutyMotorStable evtStable, PIF_evtDutyMotorStop evtStop,
		PIF_evtDutyMotorError evtError)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    pstBase->evtStable = evtStable;
    pstBase->evtStop = evtStop;
    pstBase->evtError = evtError;
}

/**
 * @fn pifDutyMotor_SetDirection
 * @brief
 * @param pstOwner
 * @param ucDirection
 */
void pifDutyMotor_SetDirection(PIF_stDutyMotor *pstOwner, uint8_t ucDirection)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

	pstOwner->ucDirection = ucDirection;

	if (pstBase->actSetDirection) (*pstBase->actSetDirection)(pstOwner->ucDirection);
}

/**
 * @fn pifDutyMotor_SetDuty
 * @brief
 * @param pstOwner
 * @param usDuty
 */
void pifDutyMotor_SetDuty(PIF_stDutyMotor *pstOwner, uint16_t usDuty)
{
    if (usDuty > pstOwner->usMaxDuty) usDuty = pstOwner->usMaxDuty;
	pstOwner->usCurrentDuty = usDuty;

	(*((PIF_stDutyMotorBase *)pstOwner)->actSetDuty)(pstOwner->usCurrentDuty);
}

/**
 * @fn pifDutyMotor_Start
 * @brief
 * @param pstOwner
 * @param usDuty
 * @return
 */
BOOL pifDutyMotor_Start(PIF_stDutyMotor *pstOwner, uint16_t usDuty)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    if (!pstBase->actSetDuty || !pstBase->actSetDirection) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    (*pstBase->actSetDirection)(pstOwner->ucDirection);

   	pstOwner->usCurrentDuty = usDuty;

    (*pstBase->actSetDuty)(pstOwner->usCurrentDuty);
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DM:%u(%u) EC:%d", __LINE__, pstOwner->unDeviceCode, usDuty, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifDutyMotor_BreakRelease
 * @brief
 * @param pstOwner
 * @param usBreakTime
 */
void pifDutyMotor_BreakRelease(PIF_stDutyMotor *pstOwner, uint16_t usBreakTime)
{
    PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

    pstOwner->usCurrentDuty = 0;

    (*pstBase->actSetDuty)(pstOwner->usCurrentDuty);

    if (usBreakTime && pstBase->actOperateBreak) {
	    if (!pstBase->pstTimerBreak) {
	    	pstBase->pstTimerBreak = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
	    }
	    if (pstBase->pstTimerBreak) {
	    	pifPulse_AttachEvtFinish(pstBase->pstTimerBreak, _TimerBreakFinish, pstBase);
			if (pifPulse_StartItem(pstBase->pstTimerBreak, usBreakTime)) {
		    	(*pstBase->actOperateBreak)(1);
			}
	    }
	}
}

/**
 * @fn pifDutyMotor_InitControl
 * @brief
 * @param pstOwner
 * @param usControlPeriod
 * @return
 */
BOOL pifDutyMotor_InitControl(PIF_stDutyMotor *pstOwner, uint16_t usControlPeriod)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

	pstBase->pstTimerControl = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enRepeat);
    if (!pstBase->pstTimerControl) return FALSE;

    pstBase->usControlPeriod = usControlPeriod;

	pifPulse_AttachEvtFinish(pstBase->pstTimerControl, _TimerControlFinish, pstBase);
	return TRUE;
}

/**
 * @fn pifDutyMotor_StartControl
 * @brief 
 * @param pstOwner
 * @return 
 */
BOOL pifDutyMotor_StartControl(PIF_stDutyMotor *pstOwner)
{
	PIF_stDutyMotorBase *pstBase = (PIF_stDutyMotorBase *)pstOwner;

	if (pstOwner->enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

    return pifPulse_StartItem(pstBase->pstTimerControl, pstBase->usControlPeriod);

fail:
#ifndef __PIF_NO_LOG__
    pifLog_Printf(LT_enError, "DM:%u(%u) S:%u EC:%u", __LINE__, pstOwner->unDeviceCode, pstOwner->enState, pif_enError);
#endif
    return FALSE;
}
