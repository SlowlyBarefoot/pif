#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensor.h"


static PIF_stSensor *s_pstSensor;
static uint8_t s_ucSensorSize;
static uint8_t s_ucSensorPos;

static PIF_stPulse *s_pstSensorTimer;


static void _evtTimerPeriodFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSensor *pstOwner = (PIF_stSensor *)pvIssuer;

    if (pstOwner->__evtPeriod) {
        (*pstOwner->__evtPeriod)(pstOwner->_usPifId, pstOwner->__usCurrLevel);
    }
}

static uint16_t _FilterAverage(uint16_t usLevel, PIF_stSensorFilter *pstFilter)
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

static void _TaskCommon(PIF_stSensor *pstOwner)
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
			(*pstOwner->__evtChange)(pstOwner->_usPifId, swState);
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

    s_pstSensor = calloc(sizeof(PIF_stSensor), ucSize);
    if (!s_pstSensor) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSensorSize = ucSize;
    s_ucSensorPos = 0;

    s_pstSensorTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sensor:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSensor_Exit
 * @brief 
 */
void pifSensor_Exit()
{
	PIF_stSensor *pstOwner;

    if (s_pstSensor) {
        for (int i = 0; i < s_ucSensorPos; i++) {
        	pstOwner = &s_pstSensor[i];
        	if (pstOwner->__ucFilterMethod) {
            	PIF_stSensorFilter *pstFilter = pstOwner->__pstFilter;
				if (pstFilter->apusBuffer) {
					free(pstFilter->apusBuffer);
					pstFilter->apusBuffer = NULL;
				}
        	}
        }
        free(s_pstSensor);
        s_pstSensor = NULL;
    }
}

/**
 * @fn pifSensor_Add
 * @brief 
 * @param usPifId
 * @return 
 */
PIF_stSensor *pifSensor_Add(PIF_usId usPifId)
{
    if (s_ucSensorPos >= s_ucSensorSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSensor *pstOwner = &s_pstSensor[s_ucSensorPos];

	pstOwner->__swState = OFF;

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;

    s_ucSensorPos = s_ucSensorPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sensor:Add(DC:%u) EC:%d", usPifId, pif_enError);
#endif
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
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerPeriod, _evtTimerPeriodFinish, pstOwner);
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
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sensor:AttachFilter(M:%d S:%u) EC:%d", ucFilterMethod, ucFilterSize, pif_enError);
#endif
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
 * @fn pifSensor_taskAll
 * @brief
 * @param pstTask
 */
void pifSensor_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

    for (int i = 0; i < s_ucSensorPos; i++) {
    	PIF_stSensor *pstOwner = &s_pstSensor[i];
        if (!pstOwner->__enTaskLoop) _TaskCommon(pstOwner);
    }
}

/**
 * @fn pifSensor_taskEach
 * @brief
 * @param pstTask
 */
void pifSensor_taskEach(PIF_stTask *pstTask)
{
	PIF_stSensor *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}
