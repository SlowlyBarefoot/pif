#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensor.h"


typedef struct _PIF_stSensorBase
{
	// Public Member Variable
	PIF_stSensor stOwner;

	// Private Member Variable
    PIF_enSensorEventType enEventType;
    union {
    	uint16_t usPeriod;
		uint16_t usThreshold;
    	struct {
			uint16_t usThresholdLow;
			uint16_t usThresholdHigh;
    	};
    };
    union {
        PIF_stPulseItem *pstTimerPeriod;
	    SWITCH swState;						// Default: OFF
    };
    uint16_t usCurrLevel;
    uint16_t usPrevLevel;

    uint8_t ucFilterMethod;					// Default: PIF_SENSOR_FILTER_NONE
    PIF_stSensorFilter *pstFilter;			// Default: NULL

	// Private Event Function
    union {
    	PIF_evtSensorPeriod evtPeriod;		// Default: NULL
    	PIF_evtSensorChange evtChange;		// Default: NULL
    };

	PIF_enTaskLoop enTaskLoop;				// Default: TL_enAll
} PIF_stSensorBase;


static PIF_stSensorBase *s_pstSensorBase;
static uint8_t s_ucSensorBaseSize;
static uint8_t s_ucSensorBasePos;

static PIF_stPulse *s_pstSensorTimer;


static void _evtTimerPeriodFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pvIssuer;

    if (pstBase->evtPeriod) {
        (*pstBase->evtPeriod)(pstBase->stOwner.usPifId, pstBase->usCurrLevel);
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

static void _TaskCommon(PIF_stSensorBase *pstBase)
{
	SWITCH swState;

	if (!pstBase->evtChange) return;

   	switch (pstBase->enEventType) {
   	case SET_enThreshold1P:
   		swState = pstBase->usCurrLevel >= pstBase->usThreshold;
   		break;

   	case SET_enThreshold2P:
   		if (pstBase->usCurrLevel <= pstBase->usThresholdLow) {
   			swState = OFF;
   		}
   		else if (pstBase->usCurrLevel >= pstBase->usThresholdHigh) {
   			swState = ON;
   		}
   		else {
   			swState = pstBase->swState;
   		}
   		break;

   	default:
   		return;
   	}

	if (swState != pstBase->swState) {
		if (pstBase->evtChange) {
			(*pstBase->evtChange)(pstBase->stOwner.usPifId, swState);
		}
		pstBase->swState = swState;
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

    s_pstSensorBase = calloc(sizeof(PIF_stSensorBase), ucSize);
    if (!s_pstSensorBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSensorBaseSize = ucSize;
    s_ucSensorBasePos = 0;

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
	PIF_stSensorBase *pstBase;

    if (s_pstSensorBase) {
        for (int i = 0; i < s_ucSensorBasePos; i++) {
        	pstBase = &s_pstSensorBase[i];
        	if (pstBase->ucFilterMethod) {
            	PIF_stSensorFilter *pstFilter = pstBase->pstFilter;
				if (pstFilter->apusBuffer) {
					free(pstFilter->apusBuffer);
					pstFilter->apusBuffer = NULL;
				}
        	}
        }
        free(s_pstSensorBase);
        s_pstSensorBase = NULL;
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
    if (s_ucSensorBasePos >= s_ucSensorBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stSensorBase *pstBase = &s_pstSensorBase[s_ucSensorBasePos];

	pstBase->swState = OFF;

    PIF_stSensor *pstOwner = &pstBase->stOwner;

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->usPifId = usPifId;

    s_ucSensorBasePos = s_ucSensorBasePos + 1;
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
    PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;

    pstBase->pstTimerPeriod = pifPulse_AddItem(s_pstSensorTimer, PT_enRepeat);
    if (!pstBase->pstTimerPeriod) return FALSE;
    pifPulse_AttachEvtFinish(pstBase->pstTimerPeriod, _evtTimerPeriodFinish, pstBase);
    pstBase->enEventType = SET_enPeriod;
    pstBase->evtPeriod = evtPeriod;
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
	return pifPulse_StartItem(((PIF_stSensorBase *)pstOwner)->pstTimerPeriod, usPeriod);
}

/**
 * @fn pifSensor_StopPeriod
 * @brief
 * @param pstOwner
 */
void pifSensor_StopPeriod(PIF_stSensor *pstOwner)
{
	pifPulse_StopItem(((PIF_stSensorBase *)pstOwner)->pstTimerPeriod);
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
    PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;

    pstBase->enEventType = SET_enThreshold1P;
    pstBase->usThreshold = usThreshold;
    pstBase->evtChange = evtChange;
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
    PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;

    pstBase->enEventType = SET_enThreshold2P;
    pstBase->usThresholdLow = usThresholdLow;
    pstBase->usThresholdHigh = usThresholdHigh;
    pstBase->evtChange = evtChange;
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

	PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;

	pstBase->ucFilterMethod = ucFilterMethod;
	pstBase->pstFilter = pstFilter;
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
	PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;
	PIF_stSensorFilter *pstFilter;

	pstFilter = pstBase->pstFilter;
	if (pstFilter->apusBuffer) {
		free(pstFilter->apusBuffer);
		pstFilter->apusBuffer = NULL;
	}
	pstFilter->ucSize = 0;
	pstFilter->evtFilter = NULL;

	pstBase->ucFilterMethod = PIF_SENSOR_FILTER_NONE;
	pstBase->pstFilter = NULL;
}

/**
 * @fn pifSensor_sigData
 * @brief 
 * @param pstOwner
 * @param usLevel
 */
void pifSensor_sigData(PIF_stSensor *pstOwner, uint16_t usLevel)
{
	PIF_stSensorBase *pstBase = (PIF_stSensorBase *)pstOwner;

	pstBase->usPrevLevel = pstBase->usCurrLevel;

	if (pstBase->ucFilterMethod) {
    	PIF_stSensorFilter *pstFilter = pstBase->pstFilter;
    	pstBase->usCurrLevel = (*pstFilter->evtFilter)(usLevel, pstFilter);
    }
    else {
    	pstBase->usCurrLevel = usLevel;
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

    for (int i = 0; i < s_ucSensorBasePos; i++) {
    	PIF_stSensorBase *pstBase = &s_pstSensorBase[i];
        if (!pstBase->enTaskLoop) _TaskCommon(pstBase);
    }
}

/**
 * @fn pifSensor_taskEach
 * @brief
 * @param pstTask
 */
void pifSensor_taskEach(PIF_stTask *pstTask)
{
	PIF_stSensorBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstBase);
	}
}
