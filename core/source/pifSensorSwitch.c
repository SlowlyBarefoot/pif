#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorSwitch.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


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

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorSwitchColSig* p_colsig = (PIF_SensorSwitchColSig*)it->data;
		PIF_stSensorSwitch* pstOwner = p_colsig->p_owner;
		for (int f = 0; f < SSCsF_enCount; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(pstOwner->stSensor._id, CSVT_enWire, 1,
						prefix[f], pstOwner->stSensor._curr_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SS_CS:Add(DC:%u F:%u)", pstOwner->stSensor._id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

void pifSensorSwitch_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifSensorSwitch_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifSensorSwitch_Create
 * @brief 
 * @param usPifId
 * @param swInitState
 * @return 
 */
PifSensor *pifSensorSwitch_Create(PifId usPifId, SWITCH swInitState)
{
    PIF_stSensorSwitch *pstOwner = NULL;

    pstOwner = calloc(sizeof(PIF_stSensorSwitch), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    PifSensor *pstSensor = &pstOwner->stSensor;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstSensor->_id = usPifId;
	pstSensor->_init_state = swInitState;
	pstSensor->_curr_state = swInitState;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSensorSwitch, _AddDeviceInCollectSignal);
	PIF_SensorSwitchColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_SensorSwitchColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = pstOwner;
	pstOwner->__p_colsig = p_colsig;
	p_colsig->state = swInitState;
#endif
    return pstSensor;
}

/**
 * @fn pifSensorSwitch_Destroy
 * @brief
 * @param pp_sensor
 */
void pifSensorSwitch_Destroy(PifSensor** pp_sensor)
{
    if (*pp_sensor) {
        free(*pp_sensor);
        *pp_sensor = NULL;
    }
}

/**
 * @fn pifSensorSwitch_InitialState
 * @brief
 * @param pstSensor
 */
void pifSensorSwitch_InitialState(PifSensor *pstSensor)
{
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;

	pstSensor->_curr_state = pstSensor->_init_state;
#ifdef __PIF_COLLECT_SIGNAL__
	pstOwner->__p_colsig->state = pstSensor->_init_state;
#endif
	pstOwner->__swState = pstSensor->_init_state;
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
BOOL pifSensorSwitch_AttachFilter(PifSensor *pstSensor, uint8_t ucFilterMethod, uint8_t ucFilterSize, PIF_stSensorSwitchFilter *pstFilter)
{
	PIF_stSensorSwitch *pstOwner = (PIF_stSensorSwitch *)pstSensor;

    if (!ucFilterMethod || ucFilterSize < 3 || ucFilterSize >= 32 || !pstFilter) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
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
}

/**
 * @fn pifSensorSwitch_DetachFilter
 * @brief
 * @param pstSensor
 */
void pifSensorSwitch_DetachFilter(PifSensor *pstSensor)
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
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorSwitchColSig* p_colsig = (PIF_SensorSwitchColSig*)it->data;
		p_colsig->flag |= enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorSwitch_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSensorSwitch_ResetCsFlagAll(PIF_enSensorSwitchCsFlag enFlag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SensorSwitchColSig* p_colsig = (PIF_SensorSwitchColSig*)it->data;
		p_colsig->flag &= ~enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorSwitch_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorSwitch_SetCsFlagEach(PifSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag)
{
	((PIF_stSensorSwitch*)pstSensor)->__p_colsig->flag |= enFlag;
}

/**
 * @fn pifSensorSwitch_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSensorSwitch_ResetCsFlagEach(PifSensor *pstSensor, PIF_enSensorSwitchCsFlag enFlag)
{
	((PIF_stSensorSwitch*)pstSensor)->__p_colsig->flag &= ~enFlag;
}

#endif

/**
 * @fn pifSensorSwitch_sigData
 * @brief 
 * @param pstSensor
 * @param swState
 */
void pifSensorSwitch_sigData(PifSensor *pstSensor, SWITCH swState)
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
	PIF_SensorSwitchColSig* p_colsig = pstOwner->__p_colsig;
	if (p_colsig->flag & SSCsF_enRawBit) {
		if (p_colsig->state != swState) {
			pifCollectSignal_AddSignal(p_colsig->p_device[SSCsF_enRawIdx], swState);
			p_colsig->state = swState;
		}
	}
#endif
}

static uint16_t _DoTask(PifTask *pstTask)
{
	PIF_stSensorSwitch *pstOwner = pstTask->_p_client;
	PifSensor *pstParent = &pstOwner->stSensor;

	if (pstParent->__act_acquire) {
		pifSensorSwitch_sigData(pstParent, (*pstParent->__act_acquire)(pstParent->_id));
	}

	if (pstOwner->__swState != pstParent->_curr_state) {
		if (pstParent->__evt_change) {
			(*pstParent->__evt_change)(pstParent->_id, pstOwner->__swState, pstParent->__p_change_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (pstOwner->__p_colsig->flag & SSCsF_enFilterBit) {
				pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[SSCsF_enFilterIdx], pstOwner->__swState);
			}
#endif
		}
		pstParent->_curr_state = pstOwner->__swState;
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
PifTask *pifSensorSwitch_AttachTask(PifSensor *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}

