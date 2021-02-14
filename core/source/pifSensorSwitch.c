#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorSwitch.h"


static PIF_stSensorSwitch *s_pstSensorSwitch;
static uint8_t s_ucSensorSwitchSize;
static uint8_t s_ucSensorSwitchPos;


static SWITCH _FilterCount(SWITCH swState, PIF_stSensorSwitchFilter *pstOwner)
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

static SWITCH _FilterContinue(SWITCH swState, PIF_stSensorSwitchFilter *pstOwner)
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

static void _TaskCommon(PIF_stSensorSwitch *pstOwner)
{
	PIF_stSensor *pstSensor = &pstOwner->stSensor;

	if (pstSensor->__actAcquire) {
		pifSensorSwitch_sigData(pstSensor, (*pstSensor->__actAcquire)(pstSensor->_usPifId));
	}

	if (pstOwner->__swState != pstSensor->_swCurrState) {
		if (pstSensor->__evtChange) {
			(*pstSensor->__evtChange)(pstSensor->_usPifId, pstOwner->__swState, pstSensor->__pvChangeIssuer);
		}
		pstSensor->_swCurrState = pstOwner->__swState;
	}
}

/**
 * @fn pifSensorSwitch_Init
 * @brief 
 * @param ucSize
 * @return 
 */
BOOL pifSensorSwitch_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSensorSwitch = calloc(sizeof(PIF_stSensorSwitch), ucSize);
    if (!s_pstSensorSwitch) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSensorSwitchSize = ucSize;
    s_ucSensorSwitchPos = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SS:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSensorSwitch_Exit
 * @brief 
 */
void pifSensorSwitch_Exit()
{
    if (s_pstSensorSwitch) {
        free(s_pstSensorSwitch);
        s_pstSensorSwitch = NULL;
    }
}

/**
 * @fn pifSensorSwitch_Add
 * @brief 
 * @param usPifId
 * @param swInitState
 * @return 
 */
PIF_stSensor *pifSensorSwitch_Add(PIF_usId usPifId, SWITCH swInitState)
{
    if (s_ucSensorSwitchPos >= s_ucSensorSwitchSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSensorSwitch *pstOwner = &s_pstSensorSwitch[s_ucSensorSwitchPos];
    PIF_stSensor *pstSensor = &pstOwner->stSensor;

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
	pstOwner->stSensor._usPifId = usPifId;
	pstSensor->_swInitState = swInitState;
	pstSensor->_swCurrState = swInitState;

    s_ucSensorSwitchPos = s_ucSensorSwitchPos + 1;
    return &pstOwner->stSensor;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SS:Add(DC:%u IS:%u) EC:%d", usPifId, swInitState, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifSensorSwitch_InitialState
 * @brief
 * @param pstSensor
 */
void pifSensorSwitch_InitialState(PIF_stSensor *pstSensor)
{
	pstSensor->_swCurrState = pstSensor->_swInitState;
	((PIF_stSensorSwitch *)pstSensor)->__swState = pstSensor->_swInitState;
}

/**
 * @fn pifSensorSwitch_AttachFilterState
 * @brief
 * @param pstSensor
 * @param ucFilterMethod
 * @param ucFilterSize
 * @param pstFilter
 * @return
 */
BOOL pifSensorSwitch_AttachFilter(PIF_stSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorSwitchFilter *pstFilter)
{
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;

    if (!ucFilterMethod || ucFilterSize < 3 || ucFilterSize >= 32 || !pstFilter) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    pifSensorSwitch_DetachFilter(&pstOwner->stSensor);

	pstFilter->ucSize = ucFilterSize;
	pstFilter->ucHalf = ucFilterSize / 2;
	pstFilter->unMsb = 1L << (ucFilterSize - 1);
	pstFilter->ucCount = 0;
	pstFilter->unList = 0L;
	switch (ucFilterMethod) {
    case PIF_SENSOR_SWITCH_FILTER_COUNT:
    	pstFilter->evtFilter = _FilterCount;
        break;

    case PIF_SENSOR_SWITCH_FILTER_CONTINUE:
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
	pifLog_Printf(LT_enError, "SS:AttachFilterState(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSensorSwitch_DetachFilter
 * @brief
 * @param pstSensor
 */
void pifSensorSwitch_DetachFilter(PIF_stSensor *pstSensor)
{
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;
	PIF_stSensorSwitchFilter *pstState;

	if (pstOwner->__ucFilterMethod) {
    	pstState = pstOwner->__pstFilter;
    	pstState->ucSize = 0;
    	pstState->evtFilter = NULL;
	}

	pstOwner->__ucFilterMethod = PIF_SENSOR_SWITCH_FILTER_NONE;
	pstOwner->__pstFilter = NULL;
}

/**
 * @fn pifSensorSwitch_sigData
 * @brief 
 * @param pstSensor
 * @param swState
 */
void pifSensorSwitch_sigData(PIF_stSensor *pstSensor, SWITCH swState)
{
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;

	if (pstOwner->__ucFilterMethod) {
    	PIF_stSensorSwitchFilter *pstState = pstOwner->__pstFilter;
    	pstOwner->__swState = (*pstState->evtFilter)(swState, pstState);
    }
	else {
		pstOwner->__swState = swState;
	}
}

/**
 * @fn pifSensorSwitch_taskAll
 * @brief
 * @param pstTask
 */
void pifSensorSwitch_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

    for (int i = 0; i < s_ucSensorSwitchPos; i++) {
    	PIF_stSensorSwitch *pstOwner = &s_pstSensorSwitch[i];
        if (!pstOwner->stSensor.__enTaskLoop) _TaskCommon(pstOwner);
    }
}

/**
 * @fn pifSensorSwitch_taskEach
 * @brief
 * @param pstTask
 */
void pifSensorSwitch_taskEach(PIF_stTask *pstTask)
{
	PIF_stSensorSwitch *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->stSensor.__enTaskLoop != TL_enEach) {
		pstOwner->stSensor.__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}
