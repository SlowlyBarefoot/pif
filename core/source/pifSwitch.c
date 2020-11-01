#include "pifLog.h"
#include "pifSwitch.h"


static PIF_stSwitch *s_pstSwitchArray;
static uint8_t s_ucSwitchArraySize;
static uint8_t s_ucSwitchArrayPos;


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

static void _LoopCommon(PIF_stSwitch *pstOwner)
{
	if (pstOwner->actAcquire) {
		pifSwitch_sigData(pstOwner, (*pstOwner->actAcquire)(pstOwner));
	}

	if (pstOwner->swCurrState != pstOwner->__swPrevState) {
		if (pstOwner->evtChange) {
			(*pstOwner->evtChange)(pstOwner);
		}
		pstOwner->__swPrevState = pstOwner->swCurrState;
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

    s_pstSwitchArray = calloc(sizeof(PIF_stSwitch), ucSize);
    if (!s_pstSwitchArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSwitchArraySize = ucSize;
    s_ucSwitchArrayPos = 0;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Switch:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifSwitch_Exit
 * @brief 
 */
void pifSwitch_Exit()
{
    if (s_pstSwitchArray) {
        free(s_pstSwitchArray);
        s_pstSwitchArray = NULL;
    }
}

/**
 * @fn pifSwitch_Add
 * @brief 
 * @param unDeviceCode
 * @param swInitState
 * @return 
 */
PIF_stSwitch *pifSwitch_Add(PIF_unDeviceCode unDeviceCode, SWITCH swInitState)
{
    if (s_ucSwitchArrayPos >= s_ucSwitchArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSwitch *pstOwner = &s_pstSwitchArray[s_ucSwitchArrayPos];

	pstOwner->unDeviceCode = unDeviceCode;
	pstOwner->swInitState = swInitState;
	pstOwner->swCurrState = swInitState;

	pstOwner->__swPrevState = swInitState;

    s_ucSwitchArrayPos = s_ucSwitchArrayPos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "Switch:Add(DC:%u IS:%u) EC:%d", unDeviceCode, swInitState, pif_enError);
    return NULL;
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
	pifLog_Printf(LT_enError, "Switch:AttachFilterState(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
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
	pstOwner->swCurrState = pstOwner->swInitState;
	pstOwner->__swPrevState = pstOwner->swInitState;
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
    	pstOwner->swCurrState = (*pstState->evtFilter)(swState, pstState);
    }
	else {
		pstOwner->swCurrState = swState;
	}
}

/**
 * @fn pifSwitch_LoopAll
 * @brief
 * @param pstTask
 */
void pifSwitch_LoopAll(PIF_stTask *pstTask)
{
	(void)pstTask;

    for (int i = 0; i < s_ucSwitchArrayPos; i++) {
    	PIF_stSwitch *pstOwner = &s_pstSwitchArray[i];
        if (!pstOwner->__enTaskLoop) _LoopCommon(pstOwner);
    }
}

/**
 * @fn pifSwitch_LoopEach
 * @brief
 * @param pstTask
 */
void pifSwitch_LoopEach(PIF_stTask *pstTask)
{
	PIF_stSwitch *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstOwner);
	}
}
