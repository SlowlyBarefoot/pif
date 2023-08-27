#ifdef __PIF_COLLECT_SIGNAL__
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "sensor/pif_sensor_digital.h"


#ifdef __PIF_COLLECT_SIGNAL__
	static PifDList s_cs_list;
#endif


static uint16_t _doTaskAcquire(PifTask* p_task)
{
	pifSensorDigital_ProcessAcquire((PifSensorDigital*)p_task->_p_client);
	return 0;
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

#endif	// __PIF_COLLECT_SIGNAL__

BOOL pifSensorDigital_Init(PifSensorDigital* p_owner, PifId id, PifActSensorAcquire act_acquire)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSensorDigital));

	p_owner->parent._curr_state = OFF;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent.__act_acquire = act_acquire;

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
	pifSensorDigital_Clear(p_owner);
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
#else
	(void)p_owner;
#endif
}

void pifSensorDigital_InitialState(PifSensorDigital* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	p_parent->_curr_state = p_parent->_init_state;
	p_owner->__curr_level = p_parent->_init_state ? 0xFFFF : 0;
}

void pifSensorDigital_SetThreshold(PifSensorDigital* p_owner, uint16_t low_threshold, uint16_t high_threshold)
{
	p_owner->__low_threshold = low_threshold;
	p_owner->__high_threshold = high_threshold;
}

BOOL pifSensorDigital_AttachFilter(PifSensorDigital* p_owner, PifNoiseFilter* p_filter, uint8_t index)
{
    if (!p_filter || index >= p_filter->_count) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	p_owner->__p_filter = p_filter;
	p_owner->__filter_index = index;
	return TRUE;
}

void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level)
{
	p_owner->__prev_level = p_owner->__curr_level;

	if (p_owner->__p_filter) {
    	p_owner->__curr_level = *(uint16_t*)pifNoiseFilter_Process(p_owner->__p_filter, p_owner->__filter_index, &level);
    }
    else {
    	p_owner->__curr_level = level;
    }
}

uint16_t pifSensorDigital_ProcessAcquire(PifSensorDigital* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	if (p_parent->__act_acquire) {
		pifSensorDigital_sigData(p_owner, (*p_parent->__act_acquire)(p_parent));
	}

	if (p_parent->evt_change) {
		if (p_parent->_curr_state) {
			if (p_owner->__curr_level <= p_owner->__low_threshold) {
				p_parent->_curr_state = OFF;
				(*p_parent->evt_change)(p_parent, p_parent->_curr_state, &p_owner->__curr_level, p_parent->p_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
				if (p_owner->__p_colsig->flag & SD_CSF_STATE_BIT) {
					pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SD_CSF_STATE_IDX], p_parent->_curr_state);
				}
#endif
			}
		}
		else {
			if (p_owner->__curr_level >= p_owner->__high_threshold) {
				p_parent->_curr_state = ON;
				(*p_parent->evt_change)(p_parent, p_parent->_curr_state, &p_owner->__curr_level, p_parent->p_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
				if (p_owner->__p_colsig->flag & SD_CSF_STATE_BIT) {
					pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SD_CSF_STATE_IDX], p_parent->_curr_state);
				}
#endif
			}
		}
	}
	return 0;
}

PifTask* pifSensorDigital_AttachTaskAcquire(PifSensorDigital* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	PifTask* p_task;

	p_task = pifTaskManager_Add(mode, period, _doTaskAcquire, p_owner, start);
	if (p_task) p_task->name = "SensorDigital";
	return p_task;
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
	pifDList_Clear(&s_cs_list, NULL);
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

#endif	// __PIF_COLLECT_SIGNAL__
