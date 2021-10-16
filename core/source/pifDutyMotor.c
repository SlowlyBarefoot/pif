#include "pifLog.h"
#include "pifDutyMotor.h"


static void _evtTimerControlFinish(void *pvIssuer)
{
	PIF_stDutyMotor *pstOwner = (PIF_stDutyMotor *)pvIssuer;

    if (pstOwner->__fnControl) (*pstOwner->__fnControl)(pstOwner);

    if (pstOwner->__ucError) {
        if (pstOwner->_enState < MS_enBreak) {
			(*pstOwner->__actSetDuty)(0);
			pstOwner->_usCurrentDuty = 0;

			pstOwner->_enState = MS_enBreak;
        }

        if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
    }

	if (pstOwner->_enState == MS_enStop) {
		pifPulse_StopItem(pstOwner->__pstTimerControl);
		pstOwner->_enState = MS_enIdle;
		if (pstOwner->evtStop) (*pstOwner->evtStop)(pstOwner);
	}
}

static void _evtTimerBreakFinish(void *pvIssuer)
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
 * @fn pifDutyMotor_Create
 * @brief 
 * @param usPifId
 * @param p_timer
 * @param usMaxDuty
 * @return 
 */
PIF_stDutyMotor *pifDutyMotor_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty)
{
    PIF_stDutyMotor *pstOwner = NULL;

    pstOwner = calloc(sizeof(PIF_stDutyMotor), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (!pifDutyMotor_Init(pstOwner, usPifId, p_timer, usMaxDuty)) return NULL;
    return pstOwner;
}

void pifDutyMotor_Destroy(PIF_stDutyMotor** pp_owner)
{
	if (*pp_owner) {
		pifDutyMotor_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifDutyMotor_Init
 * @brief 
 * @param pstOwner
 * @param usPifId
 * @param p_timer
 * @param usMaxDuty
 * @return 
 */
BOOL pifDutyMotor_Init(PIF_stDutyMotor* pstOwner, PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty)
{
    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    pstOwner->_p_timer = p_timer;
    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    pstOwner->_enState = MS_enIdle;
    pstOwner->_usMaxDuty = usMaxDuty;
    return TRUE;
}

/**
 * @fn pifDutyMotor_Clear
 * @brief
 * @param pstOwner
 */
void pifDutyMotor_Clear(PIF_stDutyMotor* pstOwner)
{
	if (pstOwner->__pstTimerControl) {
		pifPulse_RemoveItem(pstOwner->__pstTimerControl);
		pstOwner->__pstTimerControl = NULL;
	}
	if (pstOwner->__pstTimerDelay) {
		pifPulse_RemoveItem(pstOwner->__pstTimerDelay);
		pstOwner->__pstTimerDelay = NULL;
	}
	if (pstOwner->__pstTimerBreak) {
		pifPulse_RemoveItem(pstOwner->__pstTimerBreak);
		pstOwner->__pstTimerBreak = NULL;
	}
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
		pstOwner->__pstTimerBreak = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
	}
	if (pstOwner->__pstTimerBreak) {
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _evtTimerBreakFinish, pstOwner);
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
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    (*pstOwner->__actSetDirection)(pstOwner->_ucDirection);

   	pstOwner->_usCurrentDuty = usDuty;

    (*pstOwner->__actSetDuty)(pstOwner->_usCurrentDuty);
	return TRUE;
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
	    	pstOwner->__pstTimerBreak = pifPulse_AddItem(pstOwner->_p_timer, PT_ONCE);
	    }
	    if (pstOwner->__pstTimerBreak) {
	    	pifPulse_AttachEvtFinish(pstOwner->__pstTimerBreak, _evtTimerBreakFinish, pstOwner);
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
	pstOwner->__pstTimerControl = pifPulse_AddItem(pstOwner->_p_timer, PT_REPEAT);
    if (!pstOwner->__pstTimerControl) return FALSE;

    pstOwner->__usControlPeriod = usControlPeriod;

	pifPulse_AttachEvtFinish(pstOwner->__pstTimerControl, _evtTimerControlFinish, pstOwner);
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
        pif_error = E_INVALID_STATE;
	    return FALSE;
    }

    return pifPulse_StartItem(pstOwner->__pstTimerControl, pstOwner->__usControlPeriod);
}
