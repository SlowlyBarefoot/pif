#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSwitch.h"


typedef struct _PIF_stSwitchBase
{
	// Public Member Variable
	PIF_stSwitch stOwner;

	// Private Member Variable
    SWITCH swPrevState;						// Default: enInitState

    uint8_t ucFilterMethod;					// Default: PIF_SWITCH_FILTER_NONE
    PIF_stSwitchFilter *pstFilter;			// Default: NULL

	PIF_enTaskLoop enTaskLoop;				// Default: TL_enAll

	// Public Action Function
	PIF_actSwitchAcquire actAcquire;			// Default: NULL

	// Public Event Function
    PIF_evtSwitchChange evtChange;				// Default: NULL
} PIF_stSwitchBase;


static PIF_stSwitchBase *s_pstSwitchBase;
static uint8_t s_ucSwitchBaseSize;
static uint8_t s_ucSwitchBasePos;


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

static void _TaskCommon(PIF_stSwitchBase *pstBase)
{
	PIF_stSwitch *pstOwner = &pstBase->stOwner;

	if (pstBase->actAcquire) {
		pifSwitch_sigData(pstOwner, (*pstBase->actAcquire)(pstOwner->unDeviceCode));
	}

	if (pstOwner->swCurrState != pstBase->swPrevState) {
		if (pstBase->evtChange) {
			(*pstBase->evtChange)(pstOwner->unDeviceCode, pstOwner->swCurrState);
		}
		pstBase->swPrevState = pstOwner->swCurrState;
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

    s_pstSwitchBase = calloc(sizeof(PIF_stSwitchBase), ucSize);
    if (!s_pstSwitchBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSwitchBaseSize = ucSize;
    s_ucSwitchBasePos = 0;
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
    if (s_pstSwitchBase) {
        free(s_pstSwitchBase);
        s_pstSwitchBase = NULL;
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
    if (s_ucSwitchBasePos >= s_ucSwitchBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSwitchBase *pstBase = &s_pstSwitchBase[s_ucSwitchBasePos];

	pstBase->swPrevState = swInitState;

    PIF_stSwitch *pstOwner = &pstBase->stOwner;

	pstOwner->unDeviceCode = unDeviceCode;
	pstOwner->swInitState = swInitState;
	pstOwner->swCurrState = swInitState;

    s_ucSwitchBasePos = s_ucSwitchBasePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Switch:Add(DC:%u IS:%u) EC:%d", unDeviceCode, swInitState, pif_enError);
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
	((PIF_stSwitchBase *)pstOwner)->actAcquire = actAcquire;
}

/**
 * @fn pifSwitch_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtChange
 */
void pifSwitch_AttachEvent(PIF_stSwitch *pstOwner, PIF_evtSwitchChange evtChange)
{
	((PIF_stSwitchBase *)pstOwner)->evtChange = evtChange;
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

	PIF_stSwitchBase *pstBase = (PIF_stSwitchBase *)pstOwner;

	pstBase->ucFilterMethod = ucFilterMethod;
	pstBase->pstFilter = pstFilter;
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
	PIF_stSwitchBase *pstBase = (PIF_stSwitchBase *)pstOwner;
	PIF_stSwitchFilter *pstState;

	if (pstBase->ucFilterMethod) {
    	pstState = pstBase->pstFilter;
    	pstState->ucSize = 0;
    	pstState->evtFilter = NULL;
	}

	pstBase->ucFilterMethod = PIF_SWITCH_FILTER_NONE;
	pstBase->pstFilter = NULL;
}

/**
 * @fn pifSwitch_InitialState
 * @brief
 * @param pstOwner
 */
void pifSwitch_InitialState(PIF_stSwitch *pstOwner)
{
	pstOwner->swCurrState = pstOwner->swInitState;
	((PIF_stSwitchBase *)pstOwner)->swPrevState = pstOwner->swInitState;
}

/**
 * @fn pifSwitch_sigData
 * @brief 
 * @param pstOwner
 * @param swState
 */
void pifSwitch_sigData(PIF_stSwitch *pstOwner, SWITCH swState)
{
	PIF_stSwitchBase *pstBase = (PIF_stSwitchBase *)pstOwner;

	if (pstOwner->bStateReverse) swState ^= 1;

	if (pstBase->ucFilterMethod) {
    	PIF_stSwitchFilter *pstState = pstBase->pstFilter;
    	pstOwner->swCurrState = (*pstState->evtFilter)(swState, pstState);
    }
	else {
		pstOwner->swCurrState = swState;
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

    for (int i = 0; i < s_ucSwitchBasePos; i++) {
    	PIF_stSwitchBase *pstBase = &s_pstSwitchBase[i];
        if (!pstBase->enTaskLoop) _TaskCommon(pstBase);
    }
}

/**
 * @fn pifSwitch_taskEach
 * @brief
 * @param pstTask
 */
void pifSwitch_taskEach(PIF_stTask *pstTask)
{
	PIF_stSwitchBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstBase);
	}
}
