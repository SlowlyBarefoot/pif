#include "pifLed.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stLed *s_pstLed = NULL;
static uint8_t s_ucLedSize;
static uint8_t s_ucLedPos;

static PIF_stPulse *s_pstLedTimer;


static void _evtTimerBlinkFinish(void *pvIssuer)
{
	BOOL bBlink = FALSE;

    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

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
 * @fn pifLed_Init
 * @brief
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifLed_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstLed = calloc(sizeof(PIF_stLed), ucSize);
    if (!s_pstLed) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucLedSize = ucSize;
    s_ucLedPos = 0;

    s_pstLedTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "LED:%u S:%u EC:%d", __LINE__, ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifLed_Exit
 * @brief
 */
void pifLed_Exit()
{
    if (s_pstLed) {
        for (int i = 0; i < s_ucLedPos; i++) {
        	PIF_stLed *pstOwner = (PIF_stLed *)&s_pstLed[i];
        	if (pstOwner->__pstTimerBlink) {
        		pifPulse_RemoveItem(s_pstLedTimer, pstOwner->__pstTimerBlink);
        	}
        }
    	free(s_pstLed);
        s_pstLed = NULL;
    }
}

/**
 * @fn pifLed_Add
 * @brief
 * @param usPifId
 * @param ucCount
 * @param actState
 * @return
 */
PIF_stLed *pifLed_Add(PIF_usId usPifId, uint8_t ucCount, PIF_actLedState actState)
{
    if (s_ucLedPos >= s_ucLedSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucCount || ucCount > 32 || !actState) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stLed *pstOwner = &s_pstLed[s_ucLedPos];

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucLedCount = ucCount;
    pstOwner->__actState = actState;

    s_ucLedPos = s_ucLedPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "LED:%u(%u) C=%u EC:%d", __LINE__, usPifId, ucCount, pif_enError);
#endif
    return NULL;
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
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink = pifPulse_AddItem(s_pstLedTimer, PT_enRepeat);
		if (!pstOwner->__pstTimerBlink) goto fail;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _evtTimerBlinkFinish, pstOwner);
	}

	pstOwner->__unBlinkFlag = 0L;
    pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs * 1000L / s_pstLedTimer->_unPeriodUs);
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "LED:%u(%u) P:%u EC:%d", __LINE__, pstOwner->_usPifId, usPeriodMs, pif_enError);
#endif
	return FALSE;
}

/**
 * @fn pifLed_DetachBlink
 * @brief
 * @param pstOwner
 */
void pifLed_DetachBlink(PIF_stLed *pstOwner)
{
	if (pstOwner->__pstTimerBlink) {
		pifPulse_RemoveItem(s_pstLedTimer, pstOwner->__pstTimerBlink);
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
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstOwner->__pstTimerBlink || pstOwner->__pstTimerBlink->_enStep == PS_enStop) {
        pif_enError = E_enInvalidState;
		goto fail;
	}

	pstOwner->__pstTimerBlink->unTarget = usPeriodMs * 1000L / s_pstLedTimer->_unPeriodUs;
	return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "LED:%u(%u) P:%u EC:%d", __LINE__, pstOwner->_usPifId, usPeriodMs, pif_enError);
#endif
	return FALSE;
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
