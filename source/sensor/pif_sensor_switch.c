// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_sensor_switch.h"


/**
 * @fn _doTaskAcquire
 * @brief Internal helper that supports do task acquire logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTaskAcquire(PifTask* p_task)
{
	pifSensorSwitch_ProcessAcquire((PifSensorSwitch*)p_task->_p_client);
	return 0;
}

BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSensorSwitch));

    PifSensor *p_parent = &p_owner->parent;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_parent->_id = id;
    p_parent->_init_state = init_state;
    p_parent->_curr_state = init_state;
	p_parent->__act_acquire = act_acquire;

    return TRUE;
}

void pifSensorSwitch_Clear(PifSensorSwitch* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifSensorSwitch_ResetCsFlag(p_owner, SS_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifSensorSwitch_InitialState(PifSensorSwitch* p_owner)
{
	PifSensor *p_parent = &p_owner->parent;

	p_parent->_curr_state = p_parent->_init_state;
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
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[SS_CSF_RAW_IDX], state);
#endif
}

uint16_t pifSensorSwitch_ProcessAcquire(PifSensorSwitch* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	if (p_parent->__act_acquire) {
		pifSensorSwitch_sigData(p_owner, (*p_parent->__act_acquire)(p_parent));
	}

	if (p_owner->__state != p_parent->_curr_state) {
		if (p_parent->__evt_change) {
			(*p_parent->__evt_change)(p_parent, p_owner->__state, NULL, p_parent->__p_issuer);
		}
#ifdef PIF_COLLECT_SIGNAL
		pifCollectSignal_Put(&p_owner->__cs[SS_CSF_FILTER_IDX], p_owner->__state);
#endif
		p_parent->_curr_state = p_owner->__state;
	}
	return 0;
}

PifTask* pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start)
{
	PifTask* p_task;

	p_task = pifTaskManager_Add(id, mode, period, _doTaskAcquire, p_owner, start);
	if (p_task) p_task->name = "SensorSwitch";
	return p_task;
}


#ifdef PIF_COLLECT_SIGNAL

BOOL pifSensorSwitch_SetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag)
{
	if (flag & SS_CSF_RAW_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[SS_CSF_RAW_IDX], "SSR", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	if (flag & SS_CSF_FILTER_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[SS_CSF_FILTER_IDX], "SSF", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	return TRUE;
}

void pifSensorSwitch_ResetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag)
{
	if (flag & SS_CSF_RAW_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[SS_CSF_RAW_IDX]);
	if (flag & SS_CSF_FILTER_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[SS_CSF_FILTER_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
