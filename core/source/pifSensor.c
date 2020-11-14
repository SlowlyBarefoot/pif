#include "pifLog.h"
#include "pifSensor.h"


static PIF_stSensor *s_pstSensorArray;
static uint8_t s_ucSensorArraySize;
static uint8_t s_ucSensorArrayPos;

static PIF_stPulse *s_pstSensorTimer;


static void _TimerPeriodFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSensor *pstOwner = (PIF_stSensor *)pvIssuer;

    if (pstOwner->__evtPeriod) {
        (*pstOwner->__evtPeriod)(pstOwner->unDeviceCode, pstOwner->__usCurrLevel);
    }
}

static uint16_t _FilterAverage(uint16_t usLevel, PIF_stSensorFilter *pstOwner)
{
    uint8_t pos;

    pos = pstOwner->ucPos + 1;
    if (pos >= pstOwner->ucSize) pos = 0;

    if (pstOwner->unSum < pstOwner->apusBuffer[pos]) {
    	pstOwner->unSum = 0L;
    }
    else {
    	pstOwner->unSum -= pstOwner->apusBuffer[pos];
    }
    pstOwner->unSum += usLevel;
    pstOwner->apusBuffer[pos] = usLevel;
    pstOwner->ucPos = pos;

    return pstOwner->unSum / pstOwner->ucSize;
}

static void _LoopCommon(PIF_stSensor *pstOwner)
{
	SWITCH swState;

	if (!pstOwner->__evtChange) return;

   	switch (pstOwner->__enEventType) {
   	case SET_enThreshold1P:
   		swState = pstOwner->__usCurrLevel >= pstOwner->__usThreshold;
   		break;

   	case SET_enThreshold2P:
   		if (pstOwner->__usCurrLevel <= pstOwner->__usThresholdLow) {
   			swState = OFF;
   		}
   		else if (pstOwner->__usCurrLevel >= pstOwner->__usThresholdHigh) {
   			swState = ON;
   		}
   		else {
   			swState = pstOwner->__swState;
   		}
   		break;

   	default:
   		return;
   	}

	if (swState != pstOwner->__swState) {
		if (pstOwner->__evtChange) {
			(*pstOwner->__evtChange)(pstOwner->unDeviceCode, swState);
		}
		pstOwner->__swState = swState;
	}
}

/**
 * @fn pifSensor_Init
 * @brief 
 * @param pstTimer
 * @param ucSize
 * @return 
 */
BOOL pifSensor_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSensorArray = calloc(sizeof(PIF_stSensor), ucSize);
    if (!s_pstSensorArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSensorArraySize = ucSize;
    s_ucSensorArrayPos = 0;

    s_pstSensorTimer = pstTimer;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Sensor:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifSensor_Exit
 * @brief 
 */
void pifSensor_Exit()
{
	PIF_stSensor *pstOwner;

    if (s_pstSensorArray) {
        for (int i = 0; i < s_ucSensorArrayPos; i++) {
        	pstOwner = &s_pstSensorArray[i];
        	if (pstOwner->__ucFilterMethod) {
            	PIF_stSensorFilter *pstFilter = pstOwner->__pstFilter;
				if (pstFilter->apusBuffer) {
					free(pstFilter->apusBuffer);
					pstFilter->apusBuffer = NULL;
				}
        	}
        }
        free(s_pstSensorArray);
        s_pstSensorArray = NULL;
    }
}

/**
 * @fn pifSensor_Add
 * @brief 
 * @param unDeviceCode
 * @return 
 */
PIF_stSensor *pifSensor_Add(PIF_unDeviceCode unDeviceCode)
{
    if (s_ucSensorArrayPos >= s_ucSensorArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSensor *pstOwner = &s_pstSensorArray[s_ucSensorArrayPos];

	pstOwner->unDeviceCode = unDeviceCode;

	pstOwner->__swState = OFF;

    s_ucSensorArrayPos = s_ucSensorArrayPos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "Sensor:Add(DC:%u) EC:%d", unDeviceCode, pif_enError);
    return NULL;
}

/**
 * @fn pifSensor_SetEventPeriod
 * @brief
 * @param pstOwner
 * @param evtPeriod
 * @return
 */
BOOL pifSensor_SetEventPeriod(PIF_stSensor *pstOwner, PIF_evtSensorPeriod evtPeriod)
{
    pstOwner->__pstTimerPeriod = pifPulse_AddItem(s_pstSensorTimer, PT_enRepeat);
    if (!pstOwner->__pstTimerPeriod) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerPeriod, _TimerPeriodFinish, pstOwner);
	pstOwner->__enEventType = SET_enPeriod;
	pstOwner->__evtPeriod = evtPeriod;
	return TRUE;
}

/**
 * @fn pifSensor_StartPeriod
 * @brief
 * @param pstOwner
 * @param usPeriod
 * @return
 */
BOOL pifSensor_StartPeriod(PIF_stSensor *pstOwner, uint16_t usPeriod)
{
	return pifPulse_StartItem(pstOwner->__pstTimerPeriod, usPeriod);
}

/**
 * @fn pifSensor_StopPeriod
 * @brief
 * @param pstOwner
 */
void pifSensor_StopPeriod(PIF_stSensor *pstOwner)
{
	pifPulse_StopItem(pstOwner->__pstTimerPeriod);
}

/**
 * @fn pifSensor_SetEventThreshold1P
 * @brief
 * @param pstOwner
 * @param usThreshold
 * @param evtChange
 */
void pifSensor_SetEventThreshold1P(PIF_stSensor *pstOwner, uint16_t usThreshold, PIF_evtSensorChange evtChange)
{
	pstOwner->__enEventType = SET_enThreshold1P;
	pstOwner->__usThreshold = usThreshold;
	pstOwner->__evtChange = evtChange;
}

/**
 * @fn pifSensor_SetEventThreshold2P
 * @brief
 * @param pstOwner
 * @param usThresholdLow
 * @param usThresholdHigh
 * @param evtChange
 */
void pifSensor_SetEventThreshold2P(PIF_stSensor *pstOwner, uint16_t usThresholdLow, uint16_t usThresholdHigh, PIF_evtSensorChange evtChange)
{
	pstOwner->__enEventType = SET_enThreshold2P;
	pstOwner->__usThresholdLow = usThresholdLow;
	pstOwner->__usThresholdHigh = usThresholdHigh;
	pstOwner->__evtChange = evtChange;
}

/**
 * @fn pifSensor_AttachFilter
 * @brief 
 * @param pstOwner
 * @param enFilterMethod
 * @param ucFilterSize
 * @param pstFilter
 * @param bInitFilter
 * @return 
 */
BOOL pifSensor_AttachFilter(PIF_stSensor *pstOwner, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorFilter *pstFilter, BOOL bInitFilter)
{
    if (!ucFilterMethod || !ucFilterSize || !pstFilter) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    if (bInitFilter) {
		pstFilter->apusBuffer = NULL;
    }
    else {
        pifSensor_DetachFilter(pstOwner);
    }

	pstFilter->ucSize = ucFilterSize;
	pstFilter->apusBuffer = calloc(sizeof(uint16_t), ucFilterSize);
	if (!pstFilter->apusBuffer) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	switch (ucFilterMethod) {
    case PIF_SENSOR_FILTER_AVERAGE:
    	pstFilter->evtFilter = _FilterAverage;
        break;

    default:
        break;
    }

	pstOwner->__ucFilterMethod = ucFilterMethod;
	pstOwner->__pstFilter = pstFilter;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Sensor:AttachFilter(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifSensor_DetachFilter
 * @brief
 * @param pstOwner
 */
void pifSensor_DetachFilter(PIF_stSensor *pstOwner)
{
	PIF_stSensorFilter *pstFilter;

	pstFilter = pstOwner->__pstFilter;
	if (pstFilter->apusBuffer) {
		free(pstFilter->apusBuffer);
		pstFilter->apusBuffer = NULL;
	}
	pstFilter->ucSize = 0;
	pstFilter->evtFilter = NULL;

	pstOwner->__ucFilterMethod = PIF_SENSOR_FILTER_NONE;
	pstOwner->__pstFilter = NULL;
}

/**
 * @fn pifSensor_sigData
 * @brief 
 * @param pstOwner
 * @param usLevel
 */
void pifSensor_sigData(PIF_stSensor *pstOwner, uint16_t usLevel)
{
	pstOwner->__usPrevLevel = pstOwner->__usCurrLevel;

	if (pstOwner->__ucFilterMethod) {
    	PIF_stSensorFilter *pstFilter = pstOwner->__pstFilter;
    	pstOwner->__usCurrLevel = (*pstFilter->evtFilter)(usLevel, pstFilter);
    }
    else {
    	pstOwner->__usCurrLevel = usLevel;
    }
}

/**
 * @fn pifSensor_LoopAll
 * @brief
 * @param pstTask
 */
void pifSensor_LoopAll(PIF_stTask *pstTask)
{
	(void)pstTask;

    for (int i = 0; i < s_ucSensorArrayPos; i++) {
    	PIF_stSensor *pstOwner = &s_pstSensorArray[i];
        if (!pstOwner->__enTaskLoop) _LoopCommon(pstOwner);
    }
}

/**
 * @fn pifSensor_LoopEach
 * @brief
 * @param pstTask
 */
void pifSensor_LoopEach(PIF_stTask *pstTask)
{
	PIF_stSensor *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstOwner);
	}
}
