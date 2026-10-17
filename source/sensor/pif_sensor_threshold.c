// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_sensor_threshold.h"


/**
 * @fn _doTaskAcquire
 * @brief Internal helper that supports do task acquire logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTaskAcquire(PifTask* p_task)
{
	pifSensorThreshold_ProcessAcquire((PifSensorThreshold*)p_task->_p_client);
	return 0;
}

BOOL pifSensorThreshold_Init(PifSensorThreshold* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSensorThreshold));

    PifSensor *p_parent = &p_owner->parent;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_parent->_id = id;
    p_parent->_init_state = init_state ? ON : OFF;
    p_parent->_curr_state = p_parent->_init_state;
	p_parent->__act_acquire = act_acquire;

    return TRUE;
}

void pifSensorThreshold_Clear(PifSensorThreshold* p_owner)
{
#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD
	pifSensorThreshold_ResetCsFlag(p_owner, ST_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifSensorThreshold_InitialState(PifSensorThreshold* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;

	p_parent->_curr_state = p_parent->_init_state;
	p_owner->_curr_level = 0;
	p_owner->__level_set = FALSE;
}

BOOL pifSensorThreshold_SetThreshold(PifSensorThreshold* p_owner, int32_t low_threshold, int32_t high_threshold)
{
	if (low_threshold > high_threshold) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__low_threshold = low_threshold;
	p_owner->__high_threshold = high_threshold;
	p_owner->__threshold_set = TRUE;
	return TRUE;
}

void pifSensorThreshold_sigData(PifSensorThreshold* p_owner, int32_t level)
{
	if (p_owner->p_filter) {
		int32_t* p_level = (int32_t*)pifNoiseFilter_Process(p_owner->p_filter, &level);

		if (!p_level) return;		// The filter is still filling its window.
		level = *p_level;
	}
	p_owner->_curr_level = level;
	p_owner->__level_set = TRUE;
#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD
	pifCollectSignal_Put(&p_owner->__cs[ST_CSF_LEVEL_IDX], (uint32_t)p_owner->_curr_level);
#endif
}

void pifSensorThreshold_ProcessAcquire(PifSensorThreshold* p_owner)
{
	PifSensor* p_parent = &p_owner->parent;
	SWITCH state;

	if (p_parent->__act_acquire) {
		pifSensorThreshold_sigData(p_owner, (*p_parent->__act_acquire)(p_parent));
	}

	if (!p_owner->__threshold_set || !p_owner->__level_set) return;

	state = pifSensor_Hysteresis(p_parent->_curr_state, p_owner->_curr_level, p_owner->__low_threshold,
			p_owner->__high_threshold);
	if (pifSensor_ChangeState(p_parent, state, &p_owner->_curr_level)) {
#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD
		pifCollectSignal_Put(&p_owner->__cs[ST_CSF_STATE_IDX], state);
#endif
	}
}

PifTask* pifSensorThreshold_AttachTaskAcquire(PifSensorThreshold* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start)
{
	PifTask* p_task;

	p_task = pifTaskManager_Add(id, mode, period, _doTaskAcquire, p_owner, start);
	if (p_task) p_task->name = "SensorThreshold";
	return p_task;
}


#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD

BOOL pifSensorThreshold_SetCsFlag(PifSensorThreshold* p_owner, PifSensorThresholdCsFlag flag)
{
	if (flag & ST_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[ST_CSF_STATE_IDX], "ST", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	if (flag & ST_CSF_LEVEL_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[ST_CSF_LEVEL_IDX], "STL", p_owner->parent._id, CSVT_INTEGER, 32,
				(uint32_t)p_owner->_curr_level)) return FALSE;
	}
	return TRUE;
}

void pifSensorThreshold_ResetCsFlag(PifSensorThreshold* p_owner, PifSensorThresholdCsFlag flag)
{
	if (flag & ST_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[ST_CSF_STATE_IDX]);
	if (flag & ST_CSF_LEVEL_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[ST_CSF_LEVEL_IDX]);
}

#endif	// COLLECT_SIGNAL_SENSOR_THRESHOLD
