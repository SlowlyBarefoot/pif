#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorSwitch.h"


static PIF_stSensorSwitch *s_pstSensorSwitch = NULL;
static uint8_t s_ucSensorSwitchSize;
static uint8_t s_ucSensorSwitchPos;


static SWITCH _evtFilterCount(SWITCH swState, PIF_stSensorSwitchFilter *pstOwner)
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

static SWITCH _evtFilterContinue(SWITCH swState, PIF_stSensorSwitchFilter *pstOwner)
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

#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[SSCsF_enCount] = { "SSR", "SSF" };

	for (int i = 0; i < s_ucSensorSwitchPos; i++) {
		PIF_stSensorSwitch *pstOwner = &s_pstSensorSwitch[i];
		for (int f = 0; f < SSCsF_enCount; f++) {
			if (pstOwner->__ucCsFlag & (1 << f)) {
				pstOwner->__cCsIndex[f] = pifCollectSignal_AddDevice(pstOwner->stSensor._usPifId, CSVT_enWire, 1,
						prefix[f], pstOwner->stSensor._swCurrState);
			}
		}
	}
}

#endif

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

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSensorSwitch, _AddDeviceInCollectSignal);
#endif
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

    pstOwner->__ucIndex = s_ucSensorSwitchPos;
    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstSensor->_usPifId = usPifId;
	pstSensor->_swInitState = swInitState;
	pstSensor->_swCurrState = swInitState;
#ifdef __PIF_COLLECT_SIGNAL__
	pstOwner->__swRawState = swInitState;
#endif

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
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;

	pstSensor->_swCurrState = pstSensor->_swInitState;
#ifdef __PIF_COLLECT_SIGNAL__
	pstOwner->__swRawState = pstSensor->_swInitState;
#endif
	pstOwner->__swState = pstSensor->_swInitState;
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
    	pstFilter->evtFilter = _evtFilterCount;
        break;

    case PIF_SENSOR_SWITCH_FILTER_CONTINUE:
    	pstFilter->evtFilter = _evtFilterContinue;
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

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorSwitch_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorSwitch_SetCsFlagAll(PIF_enSensorSwitchCsFlag enFlag)
{
    for (int i = 0; i < s_ucSensorSwitchPos; i++) {
    	s_pstSensorSwitch[i].__ucCsFlag |= enFlag;
    }
}

/**
 * @fn pifSensorSwitch_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorSwitch_ResetCsFlagAll(PIF_enSensorSwitchCsFlag enFlag)
{
    for (int i = 0; i < s_ucSensorSwitchPos; i++) {
    	s_pstSensorSwitch[i].__ucCsFlag &= ~enFlag;
    }
}

/**
 * @fn pifSensorSwitch_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorSwitch_SetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag)
{
	((PIF_stSensorSwitch *)pstSensor)->__ucCsFlag |= enFlag;
}

/**
 * @fn pifSensorSwitch_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorSwitch_ResetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag)
{
	((PIF_stSensorSwitch *)pstSensor)->__ucCsFlag &= ~enFlag;
}

#endif

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
#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__ucCsFlag & SSCsF_enRawBit) {
		if (pstOwner->__swRawState != swState) {
			pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SSCsF_enRawIdx], swState);
			pstOwner->__swRawState = swState;
		}
	}
#endif
}

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	PIF_stSensorSwitch *pstOwner = pstTask->_pvClient;
	PIF_stSensor *pstParent = &pstOwner->stSensor;

	if (pstParent->__actAcquire) {
		pifSensorSwitch_sigData(pstParent, (*pstParent->__actAcquire)(pstParent->_usPifId));
	}

	if (pstOwner->__swState != pstParent->_swCurrState) {
		if (pstParent->__evtChange) {
			(*pstParent->__evtChange)(pstParent->_usPifId, pstOwner->__swState, pstParent->__pvChangeIssuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (pstOwner->__ucCsFlag & SSCsF_enFilterBit) {
				pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SSCsF_enFilterIdx], pstOwner->__swState);
			}
#endif
		}
		pstParent->_swCurrState = pstOwner->__swState;
	}
    return 0;
}

/**
 * @fn pifSensorSwitch_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifSensorSwitch_AttachTask(PIF_stSensor *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTask_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}

