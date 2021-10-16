#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorDigital.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


static void _evtTimerPeriodFinish(void* p_issuer)
{
    PifSensorDigital* p_owner = (PifSensorDigital*)p_issuer;

    if (p_owner->__evt_period) {
        (*p_owner->__evt_period)(p_owner->parent._id, p_owner->__curr_level);
    }
}

static uint16_t _evtFilterAverage(uint16_t level, PifSensorDigitalFilter* p_filter)
{
    uint8_t pos;

    pos = p_filter->pos + 1;
    if (pos >= p_filter->size) pos = 0;

    if (p_filter->sum < p_filter->p_buffer[pos]) {
    	p_filter->sum = 0L;
    }
    else {
    	p_filter->sum -= p_filter->p_buffer[pos];
    }
    p_filter->sum += level;
    p_filter->p_buffer[pos] = level;
    p_filter->pos = pos;

    return p_filter->sum / p_filter->size;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char* prefix[SD_CSF_COUNT] = { "SD" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorDigitalColSig* p_colsig = (PifSensorDigitalColSig*)it->data;
		PifSensorDigital* p_owner = p_colsig->p_owner;
		for (int f = 0; f < SD_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->parent._id, CSVT_WIRE, 1,
						prefix[f], p_owner->parent._curr_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SD_CS:Add(DC:%u F:%u)", p_owner->parent._id, p_colsig->flag);
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
 * @param id
 * @param p_timer
 * @return 
 */
PifSensor* pifSensorDigital_Create(PifId id, PifPulse *p_timer)
{
    PifSensorDigital* p_owner = NULL;

    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

    p_owner = calloc(sizeof(PifSensorDigital), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    p_owner->__p_timer = p_timer;
	p_owner->parent._curr_state = OFF;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_SENSOR_DIGITAL, _addDeviceInCollectSignal);
	PifSensorDigitalColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSensorDigitalColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return &p_owner->parent;
}

/**
 * @fn pifSensorDigital_Desoroy
 * @brief 
 * @param pp_parent
 */
void pifSensorDigital_Desoroy(PifSensor** pp_parent)
{
   if (*pp_parent) {
    	PifSensorDigital* p_owner = (PifSensorDigital*)*pp_parent;
		if (p_owner->__filter_method) {
			PifSensorDigitalFilter* p_filter = p_owner->__p_filter;
			if (p_filter->p_buffer) {
				free(p_filter->p_buffer);
				p_filter->p_buffer = NULL;
			}
		}
		if (p_owner->__ui.period.p_timer) {
			pifPulse_RemoveItem(p_owner->__p_timer, p_owner->__ui.period.p_timer);
		}

		free(*pp_parent);
        *pp_parent = NULL;
    }
}

/**
 * @fn pifSensorDigital_InitialState
 * @brief
 * @param p_parent
 */
void pifSensorDigital_InitialState(PifSensor* p_parent)
{
	p_parent->_curr_state = p_parent->_init_state;
	((PifSensorDigital*)p_parent)->__curr_level = p_parent->_init_state ? 0xFFFF : 0;
}

/**
 * @fn pifSensorDigital_AttachEvtPeriod
 * @brief
 * @param p_parent
 * @param evt_period
 * @return
 */
BOOL pifSensorDigital_AttachEvtPeriod(PifSensor* p_parent, PifEvtSensorDigitalPeriod evt_period)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;

	p_owner->__ui.period.p_timer = pifPulse_AddItem(p_owner->__p_timer, PT_REPEAT);
    if (!p_owner->__ui.period.p_timer) return FALSE;
    pifPulse_AttachEvtFinish(p_owner->__ui.period.p_timer, _evtTimerPeriodFinish, p_owner);
    p_owner->__event_type = SDET_PERIOD;
    p_owner->__evt_period = evt_period;
	return TRUE;
}

/**
 * @fn pifSensorDigital_StartPeriod
 * @brief
 * @param p_parent
 * @param period
 * @return
 */
BOOL pifSensorDigital_StartPeriod(PifSensor* p_parent, uint16_t period)
{
	return pifPulse_StartItem(((PifSensorDigital*)p_parent)->__ui.period.p_timer, period);
}

/**
 * @fn pifSensorDigital_StopPeriod
 * @brief
 * @param p_parent
 */
void pifSensorDigital_StopPeriod(PifSensor* p_parent)
{
	pifPulse_StopItem(((PifSensorDigital*)p_parent)->__ui.period.p_timer);
}

/**
 * @fn pifSensorDigital_SetEventThreshold1P
 * @brief
 * @param p_parent
 * @param threshold
 */
void pifSensorDigital_SetEventThreshold1P(PifSensor* p_parent, uint16_t threshold)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;

	p_owner->__event_type = SDET_THRESHOLD_1P;
	p_owner->__ui.threshold1p = threshold;
}

/**
 * @fn pifSensorDigital_SetEventThreshold2P
 * @brief
 * @param p_parent
 * @param threshold_low
 * @param threshold_high
 */
void pifSensorDigital_SetEventThreshold2P(PifSensor* p_parent, uint16_t threshold_low, uint16_t threshold_high)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;

	p_owner->__event_type = SDET_THRESHOLD_2P;
    p_owner->__ui.threshold2p.low = threshold_low;
    p_owner->__ui.threshold2p.high = threshold_high;
}

/**
 * @fn pifSensorDigital_AttachFilter
 * @brief 
 * @param p_parent
 * @param filter_method
 * @param filter_size
 * @param p_filter
 * @param init_filter
 * @return 
 */
BOOL pifSensorDigital_AttachFilter(PifSensor* p_parent, uint8_t filter_method, uint8_t filter_size,
		PifSensorDigitalFilter* p_filter, BOOL init_filter)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;

    if (!filter_method || !filter_size || !p_filter) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    if (init_filter) {
    	p_filter->p_buffer = NULL;
    }
    else {
        pifSensorDigital_DetachFilter(&p_owner->parent);
    }

    p_filter->size = filter_size;
    p_filter->p_buffer = calloc(sizeof(uint16_t), filter_size);
	if (!p_filter->p_buffer) {
		pif_error = E_OUT_OF_HEAP;
	    return FALSE;
	}

	switch (filter_method) {
    case PIF_SENSOR_DIGITAL_FILTER_AVERAGE:
    	p_filter->evt_filter = _evtFilterAverage;
        break;

    default:
        break;
    }

	p_owner->__filter_method = filter_method;
	p_owner->__p_filter = p_filter;
    return TRUE;
}

/**
 * @fn pifSensorDigital_DetachFilter
 * @brief
 * @param p_parent
 */
void pifSensorDigital_DetachFilter(PifSensor* p_parent)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;
	PifSensorDigitalFilter* p_filter;

	p_filter = p_owner->__p_filter;
	if (p_filter->p_buffer) {
		free(p_filter->p_buffer);
		p_filter->p_buffer = NULL;
	}
	p_filter->size = 0;
	p_filter->evt_filter = NULL;

	p_owner->__filter_method = PIF_SENSOR_DIGITAL_FILTER_NONE;
	p_owner->__p_filter = NULL;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorDigital_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorDigital_SetCsFlagAll(PifSensorDigitalCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorDigitalColSig* p_colsig = (PifSensorDigitalColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorDigital_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorDigital_ResetCsFlagAll(PifSensorDigitalCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorDigitalColSig* p_colsig = (PifSensorDigitalColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorDigital_SetCsFlagEach
 * @brief
 * @param p_parent
 * @param flag
 */
void pifSensorDigital_SetCsFlagEach(PifSensor* p_parent, PifSensorDigitalCsFlag flag)
{
	((PifSensorDigital*)p_parent)->__p_colsig->flag |= flag;
}

/**
 * @fn pifSensorDigital_ResetCsFlagEach
 * @brief
 * @param p_parent
 * @param flag
 */
void pifSensorDigital_ResetCsFlagEach(PifSensor* p_parent, PifSensorDigitalCsFlag flag)
{
	((PifSensorDigital*)p_parent)->__p_colsig->flag &= ~flag;
}

#endif

/**
 * @fn pifSensorDigital_sigData
 * @brief 
 * @param p_parent
 * @param level
 */
void pifSensorDigital_sigData(PifSensor* p_parent, uint16_t level)
{
	PifSensorDigital* p_owner = (PifSensorDigital*)p_parent;

	p_owner->__prev_level = p_owner->__curr_level;

	if (p_owner->__filter_method) {
    	PifSensorDigitalFilter *p_filter = p_owner->__p_filter;
    	p_owner->__curr_level = (*p_filter->evt_filter)(level, p_filter);
    }
    else {
    	p_owner->__curr_level = level;
    }
}

static uint16_t _doTask(PifTask* p_task)
{
	PifSensorDigital* p_owner = p_task->_p_client;
	PifSensor* p_parent = &p_owner->parent;
	SWITCH state;

	if (p_parent->__act_acquire) {
		pifSensorDigital_sigData(p_parent, (*p_parent->__act_acquire)(p_parent->_id));
	}

   	switch (p_owner->__event_type) {
   	case SDET_THRESHOLD_1P:
   		state = p_owner->__curr_level >= p_owner->__ui.threshold1p;
   		break;

   	case SDET_THRESHOLD_2P:
   		if (p_owner->__curr_level <= p_owner->__ui.threshold2p.low) {
   			state = OFF;
   		}
   		else if (p_owner->__curr_level >= p_owner->__ui.threshold2p.high) {
   			state = ON;
   		}
   		else {
   			state = p_parent->_curr_state;
   		}
   		break;

   	default:
   		return 0;
   	}

	if (state != p_parent->_curr_state) {
		if (p_parent->__evt_change) {
			(*p_parent->__evt_change)(p_parent->_id, state, p_parent->__p_change_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (p_owner->__p_colsig->flag & SD_CSF_STATE_BIT) {
				pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SD_CSF_STATE_IDX], state);
			}
#endif
		}
		p_parent->_curr_state = state;
	}
    return 0;
}

/**
 * @fn pifSensorDigital_AttachTask
 * @brief Task를 추가한다.
 * @param p_parent
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifSensorDigital_AttachTask(PifSensor* p_parent, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_parent, start);
}
