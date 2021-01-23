#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSwitch.h"


static PIF_stSwitch *s_pstSwitch;
static uint8_t s_ucSwitchSize;
static uint8_t s_ucSwitchPos;


static SWITCH _FilterCount(SWITCH swState, PIF_stSwitchFilter *pstOwner)
{
    if (pstOwner->unList & pstOwner->unMsb) {
    	pstOwner->unList &= ~pstOwner->unMsb;
    	pstOwner->ucCount--;
    }
    pstOwner->unList <<= 1;
    if (swState) {
    	pstOwner->unList |= 1;
    	pstOwner->ucCount++;
    }
    return pstOwner->ucCount >= pstOwner->ucHalf;
}

static SWITCH _FilterContinue(SWITCH swState, PIF_stSwitchFilter *pstOwner)
{
	int i, count;
	SWITCH sw;
	uint32_t mask;

	sw = pstOwner->unList & 1;
	if (sw != swState) {
		count = 1;
		mask = 1L;
		for (i = 1; i < pstOwner->ucSize; i++) {
			if (((pstOwner->unList >> i) & 1) != sw) break;
			count++;
			mask |= 1L << i;
	    }
		if (count <= pstOwner->ucHalf) {
			if (sw) {
				pstOwner->unList &= ~mask;
				pstOwner->ucCount -= count;
			}
			else {
				pstOwner->unList |= mask;
				pstOwner->ucCount += count;
			}
		}
	}
    if (pstOwner->unList & pstOwner->unMsb) {
    	pstOwner->unList &= ~pstOwner->unMsb;
    	pstOwner->ucCount--;
    }
	pstOwner->unList <<= 1;
	if (swState) {
		pstOwner->unList |= 1;
		pstOwner->ucCount++;
	}
    return (pstOwner->unList >> pstOwner->ucHalf) & 1;
}

static void _TaskCommon(PIF_stSwitch *pstOwner)
{
	if (pstOwner->__actAcquire) {
		pifSwitch_sigData(pstOwner, (*pstOwner->__actAcquire)(pstOwner->_usPifId));
	}

	if (pstOwner->_swCurrState != pstOwner->__swPrevState) {
		if (pstOwner->__evtChange) {
			(*pstOwner->__evtChange)(pstOwner->_usPifId, pstOwner->_swCurrState, pstOwner->__pvChangeIssuer);
		}
		pstOwner->__swPrevState = pstOwner->_swCurrState;
	}
}

/**
 * @fn pifSwitch_Init
 * @brief 
 * @param ucSize
 * @return 
 */
BOOL pifSwitch_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSwitch = calloc(sizeof(PIF_stSwitch), ucSize);
    if (!s_pstSwitch) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSwitchSize = ucSize;
    s_ucSwitchPos = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Switch:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSwitch_Exit
 * @brief 
 */
void pifSwitch_Exit()
{
    if (s_pstSwitch) {
        free(s_pstSwitch);
        s_pstSwitch = NULL;
    }
}

/**
 * @fn pifSwitch_Add
 * @brief 
 * @param usPifId
 * @param swInitState
 * @return 
 */
PIF_stSwitch *pifSwitch_Add(PIF_usId usPifId, SWITCH swInitState)
{
    if (s_ucSwitchPos >= s_ucSwitchSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSwitch *pstOwner = &s_pstSwitch[s_ucSwitchPos];

	pstOwner->__swPrevState = swInitState;

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
	pstOwner->_usPifId = usPifId;
	pstOwner->_swInitState = swInitState;
	pstOwner->_swCurrState = swInitState;

    s_ucSwitchPos = s_ucSwitchPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Switch:Add(DC:%u IS:%u) EC:%d", usPifId, swInitState, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifSwitch_AttachAction
 * @brief
 * @param pstOwner
 * @param actAcquire
 */
void pifSwitch_AttachAction(PIF_stSwitch *pstOwner, PIF_actSwitchAcquire actAcquire)
{
	pstOwner->__actAcquire = actAcquire;
}

/**
 * @fn pifSwitch_AttachEvtChange
 * @brief
 * @param pstOwner
 * @param evtChange
 * @param pvIssuer
 */
void pifSwitch_AttachEvtChange(PIF_stSwitch *pstOwner, PIF_evtSwitchChange evtChange, void *pvIssuer)
{
	pstOwner->__evtChange = evtChange;
	pstOwner->__pvChangeIssuer = pvIssuer;
}

/**
 * @fn pifSwitch_DetachEvtChange
 * @brief
 * @param pstOwner
 */
void pifSwitch_DetachEvtChange(PIF_stSwitch *pstOwner)
{
	pstOwner->__evtChange = NULL;
	pstOwner->__pvChangeIssuer = NULL;
}

/**
 * @fn pifSwitch_AttachFilterState
 * @brief
 * @param pstOwner
 * @param ucFilterMethod
 * @param ucFilterSize
 * @param pstFilter
 * @return
 */
BOOL pifSwitch_AttachFilter(PIF_stSwitch *pstOwner, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSwitchFilter *pstFilter)
{
    if (!ucFilterMethod || ucFilterSize < 3 || ucFilterSize >= 32 || !pstFilter) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    pifSwitch_DetachFilter(pstOwner);

	pstFilter->ucSize = ucFilterSize;
	pstFilter->ucHalf = ucFilterSize / 2;
	pstFilter->unMsb = 1L << (ucFilterSize - 1);
	pstFilter->ucCount = 0;
	pstFilter->unList = 0L;
	switch (ucFilterMethod) {
    case PIF_SWITCH_FILTER_COUNT:
    	pstFilter->evtFilter = _FilterCount;
        break;

    case PIF_SWITCH_FILTER_CONTINUE:
    	pstFilter->evtFilter = _FilterContinue;
        break;

    default:
        break;
    }

	pstOwner->__ucFilterMethod = ucFilterMethod;
	pstOwner->__pstFilter = pstFilter;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Switch:AttachFilterState(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSwitch_DetachFilter
 * @brief
 * @param pstOwner
 */
void pifSwitch_DetachFilter(PIF_stSwitch *pstOwner)
{
	PIF_stSwitchFilter *pstState;

	if (pstOwner->__ucFilterMethod) {
    	pstState = pstOwner->__pstFilter;
    	pstState->ucSize = 0;
    	pstState->evtFilter = NULL;
	}

	pstOwner->__ucFilterMethod = PIF_SWITCH_FILTER_NONE;
	pstOwner->__pstFilter = NULL;
}

/**
 * @fn pifSwitch_InitialState
 * @brief
 * @param pstOwner
 */
void pifSwitch_InitialState(PIF_stSwitch *pstOwner)
{
	pstOwner->_swCurrState = pstOwner->_swInitState;
	pstOwner->__swPrevState = pstOwner->_swInitState;
}

/**
 * @fn pifSwitch_sigData
 * @brief 
 * @param pstOwner
 * @param swState
 */
void pifSwitch_sigData(PIF_stSwitch *pstOwner, SWITCH swState)
{
	if (pstOwner->bStateReverse) swState ^= 1;

	if (pstOwner->__ucFilterMethod) {
    	PIF_stSwitchFilter *pstState = pstOwner->__pstFilter;
    	pstOwner->_swCurrState = (*pstState->evtFilter)(swState, pstState);
    }
	else {
		pstOwner->_swCurrState = swState;
	}
}

/**
 * @fn pifSwitch_taskAll
 * @brief
 * @param pstTask
 */
void pifSwitch_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

    for (int i = 0; i < s_ucSwitchPos; i++) {
    	PIF_stSwitch *pstOwner = &s_pstSwitch[i];
        if (!pstOwner->__enTaskLoop) _TaskCommon(pstOwner);
    }
}

/**
 * @fn pifSwitch_taskEach
 * @brief
 * @param pstTask
 */
void pifSwitch_taskEach(PIF_stTask *pstTask)
{
	PIF_stSwitch *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}
