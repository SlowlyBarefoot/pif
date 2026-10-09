// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_sensor_digital.h"


/**
 * @fn _doTaskAcquire
 * @brief Internal helper that supports do task acquire logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTaskAcquire(PifTask* p_task)
{
	pifSensorDigital_ProcessAcquire((PifSensorDigital*)p_task->_p_client);
	return 0;
}

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

    return TRUE;
}

void pifSensorDigital_Clear(PifSensorDigital* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifSensorDigital_ResetCsFlag(p_owner, SD_CSF_ALL_BIT);
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

void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level)
{
	if (p_owner->p_filter) {
    	p_owner->__curr_level = *(uint16_t*)pifNoiseFilter_Process(p_owner->p_filter, &level);
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

	if (p_parent->_curr_state) {
		if (p_owner->__curr_level <= p_owner->__low_threshold) {
			p_parent->_curr_state = OFF;
			if (p_parent->__evt_change) {
				(*p_parent->__evt_change)(p_parent, p_parent->_curr_state, &p_owner->__curr_level, p_parent->__p_issuer);
			}
#ifdef PIF_COLLECT_SIGNAL
			pifCollectSignal_Put(&p_owner->__cs[SD_CSF_STATE_IDX], p_parent->_curr_state);
#endif
		}
	}
	else {
		if (p_owner->__curr_level >= p_owner->__high_threshold) {
			p_parent->_curr_state = ON;
			if (p_parent->__evt_change) {
				(*p_parent->__evt_change)(p_parent, p_parent->_curr_state, &p_owner->__curr_level, p_parent->__p_issuer);
			}
#ifdef PIF_COLLECT_SIGNAL
			pifCollectSignal_Put(&p_owner->__cs[SD_CSF_STATE_IDX], p_parent->_curr_state);
#endif
		}
	}
	return 0;
}

PifTask* pifSensorDigital_AttachTaskAcquire(PifSensorDigital* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start)
{
	PifTask* p_task;

	p_task = pifTaskManager_Add(id, mode, period, _doTaskAcquire, p_owner, start);
	if (p_task) p_task->name = "SensorDigital";
	return p_task;
}


#ifdef PIF_COLLECT_SIGNAL

BOOL pifSensorDigital_SetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag)
{
	if (flag & SD_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[SD_CSF_STATE_IDX], "SD", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	return TRUE;
}

void pifSensorDigital_ResetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag)
{
	if (flag & SD_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[SD_CSF_STATE_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
