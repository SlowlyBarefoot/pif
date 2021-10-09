#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorDigital.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PIF_DList s_cs_list;
#endif


static void _evtTimerPeriodFinish(void *pvIssuer)
{
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

	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorDigitalColSig* p_colsig = (PIF_SensorDigitalColSig*)it->data;
		PIF_stSensorDigital* pstOwner = p_colsig->p_owner;
		for (int f = 0; f < SDCsF_enCount; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(pstOwner->stSensor._usPifId, CSVT_enWire, 1,
						prefix[f], pstOwner->stSensor._swCurrState);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enInfo, "SD_CS:Add(DC:%u F:%u)", pstOwner->stSensor._usPifId, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

void pifSensorDigital_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifSensorDigital_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifSensorDigital_Create
 * @brief 
 * @param usPifId
 * @param pstTimer
 * @return 
 */
PIF_stSensor *pifSensorDigital_Create(PifId usPifId, PIF_stPulse *pstTimer)
{
    PIF_stSensorDigital *pstOwner = NULL;

    if (!pstTimer) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

    pstOwner = calloc(sizeof(PIF_stSensorDigital), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    pstOwner->__pstTimer = pstTimer;
	pstOwner->stSensor._swCurrState = OFF;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->stSensor._usPifId = usPifId;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSensorDigital, _AddDeviceInCollectSignal);
	PIF_SensorDigitalColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_SensorDigitalColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = pstOwner;
	pstOwner->__p_colsig = p_colsig;
#endif
    return &pstOwner->stSensor;
}

/**
 * @fn pifSensorDigital_Desoroy
 * @brief 
 * @param pp_sensor
 */
void pifSensorDigital_Desoroy(PIF_stSensor** pp_sensor)
{
   if (*pp_sensor) {
    	PIF_stSensorDigital *pstOwner = (PIF_stSensorDigital *)*pp_sensor;
		if (pstOwner->__ucFilterMethod) {
			PIF_stSensorDigitalFilter *pstFilter = pstOwner->__pstFilter;
			if (pstFilter->apusBuffer) {
				free(pstFilter->apusBuffer);
				pstFilter->apusBuffer = NULL;
			}
		}
		if (pstOwner->__ui.stP.pstTimerPeriod) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__ui.stP.pstTimerPeriod);
		}

		free(*pp_sensor);
        *pp_sensor = NULL;
    }
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

	pstOwner->__ui.stP.pstTimerPeriod = pifPulse_AddItem(pstOwner->__pstTimer, PT_enRepeat);
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
		pif_error = E_INVALID_PARAM;
	    return FALSE;
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
		pif_error = E_OUT_OF_HEAP;
	    return FALSE;
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
	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorDigitalColSig* p_colsig = (PIF_SensorDigitalColSig*)it->data;
		p_colsig->flag |= enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorDigital_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorDigital_ResetCsFlagAll(PIF_enSensorDigitalCsFlag enFlag)
{
	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorDigitalColSig* p_colsig = (PIF_SensorDigitalColSig*)it->data;
		p_colsig->flag &= ~enFlag;
		it = pifDList_Next(it);
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
	((PIF_stSensorDigital*)pstSensor)->__p_colsig->flag |= enFlag;
}

/**
 * @fn pifSensorDigital_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorDigital_ResetCsFlagEach(PIF_stSensor *pstSensor, PIF_enSensorDigitalCsFlag enFlag)
{
	((PIF_stSensorDigital*)pstSensor)->__p_colsig->flag &= ~enFlag;
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

static uint16_t _DoTask(PIF_stTask *pstTask)
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
			if (pstOwner->__p_colsig->flag & SDCsF_enStateBit) {
				pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[SDCsF_enStateIdx], swState);
			}
#endif
		}
		pstParent->_swCurrState = swState;
	}
    return 0;
}

/**
 * @fn pifSensorDigital_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifSensorDigital_AttachTask(PIF_stSensor *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}
