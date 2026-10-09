// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_max31855.h"


/**
 * @fn _doTask
 * @brief Internal helper that supports do task logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifMax31855* p_owner = (PifMax31855*)p_task->_p_client;
	PifSensor* p_parent = &p_owner->parent;
	double temperature;

	if (!pifMax31855_Measure(p_owner, &temperature, NULL)) return 0;

	if (p_parent->__evt_change) {
		if (p_parent->_curr_state) {
			if (temperature < p_owner->__low_threshold) {
				p_parent->_curr_state = OFF;
				(*p_parent->__evt_change)(p_parent, p_parent->_curr_state, (PifSensorValueP)&temperature, p_parent->__p_issuer);
#ifdef PIF_COLLECT_SIGNAL
				pifCollectSignal_Put(&p_owner->__cs[M3_CSF_STATE_IDX], p_parent->_curr_state);
#endif
			}
		}
		else {
			if (temperature >= p_owner->__high_threshold) {
				p_parent->_curr_state = ON;
				(*p_parent->__evt_change)(p_parent, p_parent->_curr_state, (PifSensorValueP)&temperature, p_parent->__p_issuer);
#ifdef PIF_COLLECT_SIGNAL
				pifCollectSignal_Put(&p_owner->__cs[M3_CSF_STATE_IDX], p_parent->_curr_state);
#endif
			}
		}
	}

	if (p_owner->__evt_measure) {
		(*p_owner->__evt_measure)(p_owner, temperature, p_owner->__p_issuer);
	}
	return 0;
}

BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port, void *p_client)
{
	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax31855));

	p_owner->_p_spi = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
    if (!p_owner->_p_spi) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

    return TRUE;
}

void pifMax31855_Clear(PifMax31855* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifMax31855_ResetCsFlag(p_owner, M3_CSF_ALL_BIT);
#endif

	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
	}

	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal)
{
	uint8_t raw[4];
	int16_t data;

	pifSpiDevice_Transfer(p_owner->_p_spi, NULL, raw, 4);
	if ((raw[1] & 1) || (raw[3] & 7)) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (p_internal) {
		if (raw[2] & 0x80) {
			data = (int16_t)(0xF000 | (raw[2] << 4) | (raw[3] >> 4));
		}
		else {
			data = (raw[2] << 4) | (raw[3] >> 4);
		}
		*p_internal = data / 16.0;
	}

	if (p_temperature) {
		if (raw[0] & 0x80) {
			data = (int16_t)(0xC000 | (raw[0] << 6) | (raw[1] >> 2));
		}
		else {
			data = (raw[0] << 6) | (raw[1] >> 2);
		}
		if (p_owner->p_filter) {
			*p_temperature = *(int16_t*)pifNoiseFilter_Process(p_owner->p_filter, &data) / 4.0;
		}
		else {
			*p_temperature = data / 4.0;
		}
	}
	return TRUE;
}

BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period1ms, PifEvtMax31855Measure evt_measure, PifIssuerP p_issuer)
{
	if (!p_owner || !period1ms) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1ms * 1000UL, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "MAX31855";

	p_owner->__evt_measure = evt_measure;
	p_owner->__p_issuer = p_issuer;
	return TRUE;
}

void pifMax31855_StopMeasurement(PifMax31855* p_owner)
{
	pifTaskManager_Remove(p_owner->_p_task);
	p_owner->_p_task = NULL;
}

void pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold)
{
	p_owner->__low_threshold = low_threshold;
	p_owner->__high_threshold = high_threshold;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifMax31855_SetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag)
{
	if (flag & M3_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[M3_CSF_STATE_IDX], "M3", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	return TRUE;
}

void pifMax31855_ResetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag)
{
	if (flag & M3_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[M3_CSF_STATE_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
