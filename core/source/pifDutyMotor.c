#include "pifLog.h"
#include "pifDutyMotor.h"


static PIF_stDutyMotor *s_pstDutyMotor;
static uint8_t s_ucDutyMotorSize;
static uint8_t s_ucDutyMotorPos;

PIF_stPulse *g_pstDutyMotorTimer;


static void _TimerControlFinish(void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

    if (pstOwner->__fnControl) (*pstOwner->__fnControl)(pstOwner);

    if (pstOwner->__ucError) {
        if (pstOwner->_enState < MS_enBreak) {
			(*pstOwner->__actSetDuty)(0);
			pstOwner->_usCurrentDuty = 0;

			pstOwner->_enState = MS_enBreak;
        }

        if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner, pstOwner->__pvChild);
    }

	if (pstOwner->_enState == MS_enStop) {
		pifPulse_StopItem(pstOwner->__pstTimerControl);
		pstOwner->_enState = MS_enIdle;
		if (pstOwner->evtStop) (*pstOwner->evtStop)(pstOwner, pstOwner->__pvChild);
	}
}

static void _TimerBreakFinish(void *pvIssuer)
{
    PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

    if (pstOwner->_enState > MS_enIdle && pstOwner->_enState < MS_enReduce) {
    	pstOwner->_enState = MS_enReduce;
    }
    else {
    	(*pstOwner->__actOperateBreak)(0);
    }
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

    s_pstDutyMotor = calloc(sizeof(PIF_stDutyMotor), ucSize);
    if (!s_pstDutyMotor) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucDutyMotorSize = ucSize;
    s_ucDutyMotorPos = 0;

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
    if (s_pstDutyMotor) {
		for (int i = 0; i < s_ucDutyMotorSize; i++) {
			PIF_stDutyMotor *pstOwner = &s_pstDutyMotor[i];
			if (pstOwner->__pvChild) {
				free(pstOwner->__pvChild);
				pstOwner->__pvChild = NULL;
			}
		}
        free(s_pstDutyMotor);
        s_pstDutyMotor = NULL;
    }
}

/**
 * @fn pifDutyMotor_Add
 * @brief 
 * @param usPifId
 * @param usMaxDuty
 * @return 
 */
PIF_stDutyMotor *pifDutyMotor_Add(PIF_usId usPifId, uint16_t usMaxDuty)
{
    PIF_stDutyMotor *pstOwner = NULL;

    if (s_ucDutyMotorPos >= s_ucDutyMotorSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    pstOwner = &s_pstDutyMotor[s_ucDutyMotorPos];

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->_enState = MS_enIdle;
    pstOwner->_usMaxDuty = usMaxDuty;

    s_ucDutyMotorPos = s_ucDutyMotorPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DM:%u(%u) MD:%u EC:%d", __LINE__, usPifId, usMaxDuty, pif_enError);
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
    pstOwner->__actSetDuty = actSetDuty;
    pstOwner->__actSetDirection = actSetDirection;
    pstOwner->__actOperateBreak = actOperateBreak;
}

/**
 * @fn pifDutyMotor_SetDirection
 * @brief
 * @param pstOwner
 * @param ucDirection
 */
void pifDutyMotor_SetDirection(PIF_stDutyMotor *pstOwner, uint8_t ucDirection)
{
	pstOwner->_ucDirection = ucDirection;

	if (pstOwner->__actSetDirection) (*pstOwner->__actSetDirection)(pstOwner->_ucDirection);
}

/**
 * @fn pifDutyMotor_SetDuty
 * @brief
 * @param pstOwner
 * @param usDuty
 */
void pifDutyMotor_SetDuty(PIF_stDutyMotor *pstOwner, uint16_t usDuty)
{
    if (usDuty > pstOwner->_usMaxDuty) usDuty = pstOwner->_usMaxDuty;
	pstOwner->_usCurrentDuty = usDuty;

	(*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
}

/**
 * @fn pifDutyMotor_SetOperatingTime
 * @brief
 * @param pstOwner
 * @param unOperatingTime
 */
BOOL pifDutyMotor_SetOperatingTime(PIF_stDutyMotor *pstOwner, uint32_t unOperatingTime)
{
	if (!pstOwner->__pstTimerBreak) {
		pstOwner->__pstTimerBreak = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
	}
	if (pstOwner->__pstTimerBreak) {
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _TimerBreakFinish, pstOwner);
		if (pifPulse_StartItem(pstOwner->__pstTimerBreak, unOperatingTime)) {
			return TRUE;
		}
	}
	return FALSE;
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
    if (!pstOwner->__actSetDuty || !pstOwner->__actSetDirection) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    (*pstOwner->__actSetDirection)(pstOwner->_ucDirection);

   	pstOwner->_usCurrentDuty = usDuty;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "DM:%u(%u) EC:%d", __LINE__, pstOwner->_usPifId, usDuty, pif_enError);
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
    pstOwner->_usCurrentDuty = 0;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);

    if (usBreakTime && pstOwner->__actOperateBreak) {
	    if (!pstOwner->__pstTimerBreak) {
	    	pstOwner->__pstTimerBreak = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enOnce);
	    }
	    if (pstOwner->__pstTimerBreak) {
	    	pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _TimerBreakFinish, pstOwner);
			if (pifPulse_StartItem(pstOwner->__pstTimerBreak, usBreakTime)) {
		    	(*pstOwner->__actOperateBreak)(1);
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
	pstOwner->__pstTimerControl = pifPulse_AddItem(g_pstDutyMotorTimer, PT_enRepeat);
    if (!pstOwner->__pstTimerControl) return FALSE;

    pstOwner->__usControlPeriod = usControlPeriod;

	pifPulse_AttachEvtFinish(pstOwner->__pstTimerControl, _TimerControlFinish, pstOwner);
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
	if (pstOwner->_enState != MS_enIdle) {
        pif_enError = E_enInvalidState;
        goto fail;
    }

    return pifPulse_StartItem(pstOwner->__pstTimerControl, pstOwner->__usControlPeriod);

fail:
#ifndef __PIF_NO_LOG__
    pifLog_Printf(LT_enError, "DM:%u(%u) S:%u EC:%u", __LINE__, pstOwner->_usPifId, pstOwner->_enState, pif_enError);
#endif
    return FALSE;
}
