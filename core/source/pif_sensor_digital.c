#ifdef __PIF_COLLECT_SIGNAL__
	#include "pif_collect_signal.h"
#endif
#include "pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_sensor_digital.h"


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

#endif

BOOL pifSensorDigital_Init(PifSensorDigital* p_owner, PifId id, PifTimerManager *p_timer_manager)
{
    if (!p_timer_manager) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSensorDigital));

    p_owner->__p_timer_manager = p_timer_manager;
	p_owner->parent._curr_state = OFF;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

#ifdef __PIF_COLLECT_SIGNAL__
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_SENSOR_DIGITAL, _addDeviceInCollectSignal);
	}
	PifSensorDigitalColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSensorDigitalColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return TRUE;

#ifdef __PIF_COLLECT_SIGNAL__
fail:
	pifSensorDigital_Destroy((PifSensor**)&p_owner);
	return FALSE;
#endif
}

void pifSensorDigital_Clear(PifSensorDigital* p_owner)
{
#ifdef __PIF_COLLECT_SIGNAL__
	pifDList_Remove(&s_cs_list, p_owner->__p_colsig);
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_SENSOR_DIGITAL);
	}
	p_owner->__p_colsig = NULL;
#endif

	if (p_owner->__filter_method) {
		PifSensorDigitalFilter* p_filter = p_owner->__p_filter;
		if (p_filter->p_buffer) {
			free(p_filter->p_buffer);
			p_filter->p_buffer = NULL;
		}
	}
	if (p_owner->__ui.period.p_timer) {
		pifTimerManager_Remove(p_owner->__ui.period.p_timer);
	}
}

void pifSensorDigital_InitialState(PifSensorDigital* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	p_parent->_curr_state = p_parent->_init_state;
	p_owner->__curr_level = p_parent->_init_state ? 0xFFFF : 0;
}

BOOL pifSensorDigital_AttachEvtPeriod(PifSensorDigital* p_owner, PifEvtSensorDigitalPeriod evt_period)
{
	p_owner->__ui.period.p_timer = pifTimerManager_Add(p_owner->__p_timer_manager, TT_REPEAT);
    if (!p_owner->__ui.period.p_timer) return FALSE;
    pifTimer_AttachEvtFinish(p_owner->__ui.period.p_timer, _evtTimerPeriodFinish, p_owner);
    p_owner->__event_type = SDET_PERIOD;
    p_owner->__evt_period = evt_period;
	return TRUE;
}

BOOL pifSensorDigital_StartPeriod(PifSensorDigital* p_owner, uint16_t period)
{
	return pifTimer_Start(p_owner->__ui.period.p_timer, period);
}

void pifSensorDigital_StopPeriod(PifSensorDigital* p_owner)
{
	pifTimer_Stop(p_owner->__ui.period.p_timer);
}

void pifSensorDigital_SetEventThreshold1P(PifSensorDigital* p_owner, uint16_t threshold)
{
	p_owner->__event_type = SDET_THRESHOLD_1P;
	p_owner->__ui.threshold1p = threshold;
}

void pifSensorDigital_SetEventThreshold2P(PifSensorDigital* p_owner, uint16_t threshold_low, uint16_t threshold_high)
{
	p_owner->__event_type = SDET_THRESHOLD_2P;
    p_owner->__ui.threshold2p.low = threshold_low;
    p_owner->__ui.threshold2p.high = threshold_high;
}

BOOL pifSensorDigital_AttachFilter(PifSensorDigital* p_owner, uint8_t filter_method, uint8_t filter_size,
		PifSensorDigitalFilter* p_filter, BOOL init_filter)
{
    if (!filter_method || !filter_size || !p_filter) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    if (init_filter) {
    	p_filter->p_buffer = NULL;
    }
    else {
        pifSensorDigital_DetachFilter(p_owner);
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

void pifSensorDigital_DetachFilter(PifSensorDigital* p_owner)
{
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

void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level)
{
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
	PifSensorDigital* p_owner = (PifSensorDigital*)p_task->_p_client;
	PifSensor* p_parent = &p_owner->parent;
	SWITCH state;

	if (p_parent->__act_acquire) {
		pifSensorDigital_sigData(p_owner, (*p_parent->__act_acquire)(p_parent->_id));
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

PifTask* pifSensorDigital_AttachTask(PifSensorDigital* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_owner, start);
}


#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorDigital_SetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifSensorDigital_ResetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifSensorDigitalColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifSensorDigitalColSig_Clear()
{
	pifDList_Clear(&s_cs_list);
}

void pifSensorDigitalColSig_SetFlag(PifSensorDigitalCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorDigitalColSig* p_colsig = (PifSensorDigitalColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifSensorDigitalColSig_ResetFlag(PifSensorDigitalCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorDigitalColSig* p_colsig = (PifSensorDigitalColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif
