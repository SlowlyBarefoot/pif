#include "pifLed.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static void _evtTimerBlinkFinish(void *pvIssuer)
{
	BOOL bBlink = FALSE;

    PIF_stLed *pstOwner = (PIF_stLed *)pvIssuer;
    pstOwner->__swBlink ^= 1;
    for (uint8_t i = 0; i < pstOwner->ucLedCount; i++) {
    	if (pstOwner->__unBlinkFlag & (1 << i)) {
    		if (pstOwner->__swBlink) {
    			pstOwner->__unState |= 1 << i;
    		}
    		else {
    			pstOwner->__unState &= ~(1 << i);
    		}
    		bBlink = TRUE;
    	}
    }
    if (bBlink) (*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_Create
 * @brief
 * @param usPifId
 * @param pstTimer
 * @param ucCount
 * @param actState
 * @return
 */
PIF_stLed *pifLed_Create(PifId usPifId, PifPulse *pstTimer, uint8_t ucCount, PIF_actLedState actState)
{
	PIF_stLed *pstOwner = NULL;

    if (!pstTimer || !ucCount || ucCount > 32 || !actState) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
    }

    pstOwner = calloc(sizeof(PIF_stLed), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    pstOwner->__pstTimer = pstTimer;
    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucLedCount = ucCount;
    pstOwner->__actState = actState;
    return pstOwner;
}

/**
 * @fn pifLed_Destroy
 * @brief
 * @param pp_owner
 */
void pifLed_Destroy(PIF_stLed** pp_owner)
{
	if (*pp_owner) {
		PIF_stLed* pstOwner = *pp_owner;
		if (pstOwner->__pstTimerBlink) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__pstTimerBlink);
		}
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifLed_EachOn
 * @brief
 * @param pstOwner
 * @param ucIndex
 */
void pifLed_EachOn(PIF_stLed *pstOwner, uint8_t ucIndex)
{
	pstOwner->__unState |= 1 << ucIndex;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_EachOff
 * @brief
 * @param pstOwner
 * @param ucIndex
 */
void pifLed_EachOff(PIF_stLed *pstOwner, uint8_t ucIndex)
{
	pstOwner->__unState &= ~(1 << ucIndex);
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_EachChange
 * @brief
 * @param pstOwner
 * @param ucIndex
 * @param swState
 */
void pifLed_EachChange(PIF_stLed *pstOwner, uint8_t ucIndex, SWITCH swState)
{
	if (swState) {
		pstOwner->__unState |= 1 << ucIndex;
	}
	else {
		pstOwner->__unState &= ~(1 << ucIndex);
	}
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_EachToggle
 * @brief
 * @param pstOwner
 * @param ucIndex
 */
void pifLed_EachToggle(PIF_stLed *pstOwner, uint8_t ucIndex)
{
	pstOwner->__unState ^= 1 << ucIndex;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_AllOn
 * @brief
 * @param pstOwner
 */
void pifLed_AllOn(PIF_stLed *pstOwner)
{
	pstOwner->__unState = (1 << pstOwner->ucLedCount) - 1;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_AllOff
 * @brief
 * @param pstOwner
 */
void pifLed_AllOff(PIF_stLed *pstOwner)
{
	pstOwner->__unState = 0;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_AllChange
 * @brief
 * @param pstOwner
 * @param unState
 */
void pifLed_AllChange(PIF_stLed *pstOwner, uint32_t unState)
{
	pstOwner->__unState = unState;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_AllToggle
 * @brief
 * @param pstOwner
 */
void pifLed_AllToggle(PIF_stLed *pstOwner)
{
	pstOwner->__unState ^= (1 << pstOwner->ucLedCount) - 1;
	(*pstOwner->__actState)(pstOwner->_usPifId, pstOwner->__unState);
}

/**
 * @fn pifLed_AttachBlink
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifLed_AttachBlink(PIF_stLed *pstOwner, uint16_t usPeriodMs)
{
	if (!usPeriodMs) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink = pifPulse_AddItem(pstOwner->__pstTimer, PT_REPEAT);
		if (!pstOwner->__pstTimerBlink) return FALSE;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _evtTimerBlinkFinish, pstOwner);
	}

	pstOwner->__unBlinkFlag = 0L;
    pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs * 1000L / pstOwner->__pstTimer->_period1us);
	return TRUE;
}

/**
 * @fn pifLed_DetachBlink
 * @brief
 * @param pstOwner
 */
void pifLed_DetachBlink(PIF_stLed *pstOwner)
{
	if (pstOwner->__pstTimerBlink) {
		pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__pstTimerBlink);
		pstOwner->__pstTimerBlink = NULL;
	}
}

/**
 * @fn pifLed_ChangeBlinkPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifLed_ChangeBlinkPeriod(PIF_stLed *pstOwner, uint16_t usPeriodMs)
{
	if (!usPeriodMs) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!pstOwner->__pstTimerBlink || pstOwner->__pstTimerBlink->_step == PS_STOP) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	pstOwner->__pstTimerBlink->target = usPeriodMs * 1000L / pstOwner->__pstTimer->_period1us;
	return TRUE;
}

/**
 * @fn pifLed_BlinkOn
 * @brief
 * @param pstOwner
 * @param ucIndex
 */
void pifLed_BlinkOn(PIF_stLed *pstOwner, uint8_t ucIndex)
{
    pstOwner->__unBlinkFlag |= 1 << ucIndex;
}

/**
 * @fn pifLed_BlinkOff
 * @brief
 * @param pstOwner
 * @param ucIndex
 */
void pifLed_BlinkOff(PIF_stLed *pstOwner, uint8_t ucIndex)
{
	pstOwner->__unBlinkFlag &= ~(1 << ucIndex);
	pifLed_EachOff(pstOwner, ucIndex);
}
