#ifdef __PIF_COLLECT_SIGNAL__
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "sensor/pif_sensor_switch.h"


#ifdef __PIF_COLLECT_SIGNAL__
	static PifDList s_cs_list;
#endif


static uint16_t _doTaskAcquire(PifTask* p_task)
{
	pifSensorSwitch_ProcessAcquire((PifSensorSwitch*)p_task->_p_client);
	return 0;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char *prefix[SS_CSF_COUNT] = { "SSR", "SSF" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		PifSensorSwitch* p_owner = p_colsig->p_owner;
		for (int f = 0; f < SS_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->parent._id, CSVT_WIRE, 1,
						prefix[f], p_owner->parent._curr_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SS_CS:Add(DC:%u F:%u)", p_owner->parent._id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

#endif	// __PIF_COLLECT_SIGNAL__

BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire)
{
    memset(p_owner, 0, sizeof(PifSensorSwitch));

    PifSensor *p_parent = &p_owner->parent;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_parent->_id = id;
    p_parent->_init_state = init_state;
    p_parent->_curr_state = init_state;
	p_parent->__act_acquire = act_acquire;

#ifdef __PIF_COLLECT_SIGNAL__
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_SENSOR_SWITCH, _addDeviceInCollectSignal);
	}
	PifSensorSwitchColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSensorSwitchColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
	p_colsig->state = init_state;
#endif
    return TRUE;

#ifdef __PIF_COLLECT_SIGNAL__
fail:
	pifSensorSwitch_Clear(p_owner);
	return FALSE;
#endif
}

void pifSensorSwitch_Clear(PifSensorSwitch* p_owner)
{
#ifdef __PIF_COLLECT_SIGNAL__
	pifDList_Remove(&s_cs_list, p_owner->__p_colsig);
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_SENSOR_SWITCH);
	}
	p_owner->__p_colsig = NULL;
#else
	(void)p_owner;
#endif
}

void pifSensorSwitch_InitialState(PifSensorSwitch* p_owner)
{
	PifSensor *p_parent = &p_owner->parent;

	p_parent->_curr_state = p_parent->_init_state;
#ifdef __PIF_COLLECT_SIGNAL__
	p_owner->__p_colsig->state = p_parent->_init_state;
#endif
	p_owner->__state = p_parent->_init_state;
}

void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state)
{
	if (p_owner->p_filter) {
    	p_owner->__state = *(SWITCH*)pifNoiseFilter_Process(p_owner->p_filter, &state);
    }
	else {
		p_owner->__state = state;
	}
#ifdef __PIF_COLLECT_SIGNAL__
	PifSensorSwitchColSig* p_colsig = p_owner->__p_colsig;
	if (p_colsig->flag & SS_CSF_RAW_BIT) {
		if (p_colsig->state != state) {
			pifCollectSignal_AddSignal(p_colsig->p_device[SS_CSF_RAW_IDX], state);
			p_colsig->state = state;
		}
	}
#endif
}

uint16_t pifSensorSwitch_ProcessAcquire(PifSensorSwitch* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	if (p_parent->__act_acquire) {
		pifSensorSwitch_sigData(p_owner, (*p_parent->__act_acquire)(p_parent));
	}

	if (p_owner->__state != p_parent->_curr_state) {
		if (p_parent->evt_change) {
			(*p_parent->evt_change)(p_parent, p_owner->__state, NULL, p_parent->p_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (p_owner->__p_colsig->flag & SS_CSF_FILTER_BIT) {
				pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SS_CSF_FILTER_IDX], p_owner->__state);
			}
#endif
		}
		p_parent->_curr_state = p_owner->__state;
	}
	return 0;
}

PifTask* pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	PifTask* p_task;

	p_task = pifTaskManager_Add(mode, period, _doTaskAcquire, p_owner, start);
	if (p_task) p_task->name = "SensorSwitch";
	return p_task;
}


#ifdef __PIF_COLLECT_SIGNAL__

void pifSensorSwitch_SetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifSensorSwitch_ResetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifSensorSwitchColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifSensorSwitchColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifSensorSwitchColSig_SetFlag(PifSensorSwitchCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifSensorSwitchColSig_ResetFlag(PifSensorSwitchCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif	// __PIF_COLLECT_SIGNAL__
