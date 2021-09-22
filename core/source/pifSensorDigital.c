#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorDigital.h"


static PIF_stSensorDigital *s_pstSensorDigital = NULL;
static uint8_t s_ucSensorDigitalSize;
static uint8_t s_ucSensorDigitalPos;

static PIF_stPulse *s_pstSensorDigitalTimer;


static void _evtTimerPeriodFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pvIssuer;

    if (pstOwner->__evtPeriod) {
        (*pstOwner->__evtPeriod)(pstOwner->stSensor._usPifId, pstOwner->__usCurrLevel);
    }
}

static uint16_t _evtFilterAverage(uint16_t usLevel, PIF_stSensorDigitalFilter *pstFilter)
{
    uint8_t pos;

    pos = pstFilter->ucPos + 1;
    if (pos >= pstFilter->ucSize) pos = 0;

    if (pstFilter->unSum < pstFilter->apusBuffer[pos]) {
    	pstFilter->unSum = 0L;
    }
    else {
    	pstFilter->unSum -= pstFilter->apusBuffer[pos];
    }
    pstFilter->unSum += usLevel;
    pstFilter->apusBuffer[pos] = usLevel;
    pstFilter->ucPos = pos;

    return pstFilter->unSum / pstFilter->ucSize;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[SDCsF_enCount] = { "SD" };

	for (int i = 0; i < s_ucSensorDigitalPos; i++) {
		PIF_stSensorDigital *pstOwner = &s_pstSensorDigital[i];
		for (int f = 0; f < SDCsF_enCount; f++) {
			if (pstOwner->__ucCsFlag & (1 << f)) {
				pstOwner->__cCsIndex[f] = pifCollectSignal_AddDevice(pstOwner->stSensor._usPifId, CSVT_enWire, 1,
						prefix[f], pstOwner->stSensor._swCurrState);
			}
		}
	}
}

#endif

/**
 * @fn pifSensorDigital_Init
 * @brief 
 * @param ucSize
 * @param pstTimer
 * @return 
 */
BOOL pifSensorDigital_Init(uint8_t ucSize, PIF_stPulse *pstTimer)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSensorDigital = calloc(sizeof(PIF_stSensorDigital), ucSize);
    if (!s_pstSensorDigital) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSensorDigitalSize = ucSize;
    s_ucSensorDigitalPos = 0;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSensorDigital, _AddDeviceInCollectSignal);
#endif

    s_pstSensorDigitalTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SensorDigital:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSensorDigital_Exit
 * @brief 
 */
void pifSensorDigital_Exit()
{
	PIF_stSensorDigital *pstOwner;

    if (s_pstSensorDigital) {
        for (int i = 0; i < s_ucSensorDigitalPos; i++) {
        	pstOwner = &s_pstSensorDigital[i];
        	if (pstOwner->__ucFilterMethod) {
            	PIF_stSensorDigitalFilter *pstFilter = pstOwner->__pstFilter;
				if (pstFilter->apusBuffer) {
					free(pstFilter->apusBuffer);
					pstFilter->apusBuffer = NULL;
				}
        	}
        	if (pstOwner->__ui.stP.pstTimerPeriod) {
        		pifPulse_RemoveItem(s_pstSensorDigitalTimer, pstOwner->__ui.stP.pstTimerPeriod);
        	}
        }
        free(s_pstSensorDigital);
        s_pstSensorDigital = NULL;
    }
}

/**
 * @fn pifSensorDigital_Add
 * @brief 
 * @param usPifId
 * @return 
 */
PIF_stSensor *pifSensorDigital_Add(PIF_usId usPifId)
{
    if (s_ucSensorDigitalPos >= s_ucSensorDigitalSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSensorDigital *pstOwner = &s_pstSensorDigital[s_ucSensorDigitalPos];

    pstOwner->__ucIndex = s_ucSensorDigitalPos;
	pstOwner->stSensor._swCurrState = OFF;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->stSensor._usPifId = usPifId;

    s_ucSensorDigitalPos = s_ucSensorDigitalPos + 1;
    return &pstOwner->stSensor;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SensorDigital:Add(DC:%u) EC:%d", usPifId, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifSensorDigital_InitialState
 * @brief
 * @param pstSensor
 */
void pifSensorDigital_InitialState(PIF_stSensor *pstSensor)
{
	pstSensor->_swCurrState = pstSensor->_swInitState;
	((PIF_stSensorDigital *)pstSensor)->__usCurrLevel = pstSensor->_swInitState ? 0xFFFF : 0;
}

/**
 * @fn pifSensorDigital_AttachEvtPeriod
 * @brief
 * @param pstSensor
 * @param evtPeriod
 * @return
 */
BOOL pifSensorDigital_AttachEvtPeriod(PIF_stSensor *pstSensor, PIF_evtSensorDigitalPeriod evtPeriod)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;

	pstOwner->__ui.stP.pstTimerPeriod = pifPulse_AddItem(s_pstSensorDigitalTimer, PT_enRepeat);
    if (!pstOwner->__ui.stP.pstTimerPeriod) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__ui.stP.pstTimerPeriod, _evtTimerPeriodFinish, pstOwner);
    pstOwner->__enEventType = SDET_enPeriod;
    pstOwner->__evtPeriod = evtPeriod;
	return TRUE;
}

/**
 * @fn pifSensorDigital_StartPeriod
 * @brief
 * @param pstSensor
 * @param usPeriod
 * @return
 */
BOOL pifSensorDigital_StartPeriod(PIF_stSensor *pstSensor, uint16_t usPeriod)
{
	return pifPulse_StartItem(((PIF_stSensorDigital *)pstSensor)->__ui.stP.pstTimerPeriod, usPeriod);
}

/**
 * @fn pifSensorDigital_StopPeriod
 * @brief
 * @param pstSensor
 */
void pifSensorDigital_StopPeriod(PIF_stSensor *pstSensor)
{
	pifPulse_StopItem(((PIF_stSensorDigital *)pstSensor)->__ui.stP.pstTimerPeriod);
}

/**
 * @fn pifSensorDigital_SetEventThreshold1P
 * @brief
 * @param pstSensor
 * @param usThreshold
 */
void pifSensorDigital_SetEventThreshold1P(PIF_stSensor *pstSensor, uint16_t usThreshold)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;

	pstOwner->__enEventType = SDET_enThreshold1P;
	pstOwner->__ui.usThreshold = usThreshold;
}

/**
 * @fn pifSensorDigital_SetEventThreshold2P
 * @brief
 * @param pstSensor
 * @param usThresholdLow
 * @param usThresholdHigh
 */
void pifSensorDigital_SetEventThreshold2P(PIF_stSensor *pstSensor, uint16_t usThresholdLow, uint16_t usThresholdHigh)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;

	pstOwner->__enEventType = SDET_enThreshold2P;
    pstOwner->__ui.stT.usThresholdLow = usThresholdLow;
    pstOwner->__ui.stT.usThresholdHigh = usThresholdHigh;
}

/**
 * @fn pifSensorDigital_AttachFilter
 * @brief 
 * @param pstSensor
 * @param enFilterMethod
 * @param ucFilterSize
 * @param pstFilter
 * @param bInitFilter
 * @return 
 */
BOOL pifSensorDigital_AttachFilter(PIF_stSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorDigitalFilter *pstFilter, BOOL bInitFilter)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;

    if (!ucFilterMethod || !ucFilterSize || !pstFilter) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    if (bInitFilter) {
		pstFilter->apusBuffer = NULL;
    }
    else {
        pifSensorDigital_DetachFilter(&pstOwner->stSensor);
    }

	pstFilter->ucSize = ucFilterSize;
	pstFilter->apusBuffer = calloc(sizeof(uint16_t), ucFilterSize);
	if (!pstFilter->apusBuffer) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	switch (ucFilterMethod) {
    case PIF_SENSOR_DIGITAL_FILTER_AVERAGE:
    	pstFilter->evtFilter = _evtFilterAverage;
        break;

    default:
        break;
    }

	pstOwner->__ucFilterMethod = ucFilterMethod;
	pstOwner->__pstFilter = pstFilter;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SensorDigital:AttachFilter(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSensorDigital_DetachFilter
 * @brief
 * @param pstSensor
 */
void pifSensorDigital_DetachFilter(PIF_stSensor *pstSensor)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;
	PIF_stSensorDigitalFilter *pstFilter;

	pstFilter = pstOwner->__pstFilter;
	if (pstFilter->apusBuffer) {
		free(pstFilter->apusBuffer);
		pstFilter->apusBuffer = NULL;
	}
	pstFilter->ucSize = 0;
	pstFilter->evtFilter = NULL;

	pstOwner->__ucFilterMethod = PIF_SENSOR_DIGITAL_FILTER_NONE;
	pstOwner->__pstFilter = NULL;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorDigital_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorDigital_SetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag)
{
    for (int i = 0; i < s_ucSensorDigitalPos; i++) {
    	s_pstSensorDigital[i].__ucCsFlag |= enFlag;
    }
}

/**
 * @fn pifSensorDigital_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorDigital_ResetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag)
{
    for (int i = 0; i < s_ucSensorDigitalPos; i++) {
    	s_pstSensorDigital[i].__ucCsFlag &= ~enFlag;
    }
}

/**
 * @fn pifSensorDigital_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorDigital_SetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag)
{
	((PIF_stSensorDigital *)pstSensor)->__ucCsFlag |= enFlag;
}

/**
 * @fn pifSensorDigital_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorDigital_ResetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag)
{
	((PIF_stSensorDigital *)pstSensor)->__ucCsFlag &= ~enFlag;
}

#endif

/**
 * @fn pifSensorDigital_sigData
 * @brief 
 * @param pstSensor
 * @param usLevel
 */
void pifSensorDigital_sigData(PIF_stSensor *pstSensor, uint16_t usLevel)
{
	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)pstSensor;

	pstOwner->__usPrevLevel = pstOwner->__usCurrLevel;

	if (pstOwner->__ucFilterMethod) {
    	PIF_stSensorDigitalFilter *pstFilter = pstOwner->__pstFilter;
    	pstOwner->__usCurrLevel = (*pstFilter->evtFilter)(usLevel, pstFilter);
    }
    else {
    	pstOwner->__usCurrLevel = usLevel;
    }
}

/**
 * @fn pifSensorDigital_Task
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifSensorDigital_Task(PIF_stTask *pstTask)
{
	PIF_stSensorDigital *pstOwner = pstTask->_pvClient;
	PIF_stSensor *pstParent = &pstOwner->stSensor;
	SWITCH swState;

	if (pstParent->__actAcquire) {
		pifSensorDigital_sigData(pstParent, (*pstParent->__actAcquire)(pstParent->_usPifId));
	}

   	switch (pstOwner->__enEventType) {
   	case SDET_enThreshold1P:
   		swState = pstOwner->__usCurrLevel >= pstOwner->__ui.usThreshold;
   		break;

   	case SDET_enThreshold2P:
   		if (pstOwner->__usCurrLevel <= pstOwner->__ui.stT.usThresholdLow) {
   			swState = OFF;
   		}
   		else if (pstOwner->__usCurrLevel >= pstOwner->__ui.stT.usThresholdHigh) {
   			swState = ON;
   		}
   		else {
   			swState = pstParent->_swCurrState;
   		}
   		break;

   	default:
   		return 0;
   	}

	if (swState != pstParent->_swCurrState) {
		if (pstParent->__evtChange) {
			(*pstParent->__evtChange)(pstParent->_usPifId, swState, pstParent->__pvChangeIssuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (pstOwner->__ucCsFlag & SDCsF_enStateBit) {
				pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SDCsF_enStateIdx], swState);
			}
#endif
		}
		pstParent->_swCurrState = swState;
	}
    return 0;
}
