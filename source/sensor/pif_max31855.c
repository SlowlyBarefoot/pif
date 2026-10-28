// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_max31855.h"


/**
 * @fn _measure
 * @brief Reads the device once.
 * @param p_owner Pointer to the owner instance.
 * @param p_quarter Thermocouple temperature after the attached filter in 0.25 degree units, or NULL.
 * @param p_internal Cold junction temperature in degree Celsius, or NULL.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _measure(PifMax31855* p_owner, int16_t* p_quarter, double* p_internal)
{
	uint8_t raw[4];
	int16_t data;

	if (!pifSpiDevice_Transfer((PifSpiDevice *)p_owner->_p_device, NULL, raw, 4)) return FALSE;
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

	if (p_quarter) {
		if (raw[0] & 0x80) {
			data = (int16_t)(0xC000 | (raw[0] << 6) | (raw[1] >> 2));
		}
		else {
			data = (raw[0] << 6) | (raw[1] >> 2);
		}
		if (p_owner->__p_filter) data = *(int16_t*)pifNoiseFilter_Process(p_owner->__p_filter, &data);
		*p_quarter = data;
	}
	return TRUE;
}

/**
 * @fn _toQuarter
 * @brief Converts degrees to the smallest 0.25 degree count not below them, so that comparing counts
 *        gives the same result as comparing the degrees.
 * @param degree Temperature in degree Celsius.
 * @return The count.
 */
static int32_t _toQuarter(double degree)
{
	double quarter = degree * 4;
	int32_t count;

	if (quarter >= 2147483647.0) return 2147483647L;
	if (quarter <= -2147483648.0) return -2147483647L - 1;
	count = (int32_t)quarter;		// Truncated toward zero
	if (count < quarter) count++;
	return count;
}

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
	int16_t quarter;
	double temperature;
	SWITCH state;

	if (!_measure(p_owner, &quarter, &p_owner->_internal)) return 0;
	temperature = quarter / 4.0;

	if (p_owner->__threshold_set) {
		state = pifSensor_Hysteresis(p_parent->_curr_state, quarter, p_owner->__low_threshold, p_owner->__high_threshold);
		if (pifSensor_ChangeState(p_parent, state, &temperature)) {
#ifdef COLLECT_SIGNAL_MAX31855
			pifCollectSignal_Put(&p_owner->__cs[M3_CSF_STATE_IDX], state);
#endif
		}
	}

	if (p_owner->__evt_measure) {
		(*p_owner->__evt_measure)(p_owner, temperature, p_owner->__p_issuer);
	}
	return 0;
}

BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port, void *p_client)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax31855));

	p_device = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

    return TRUE;
}

void pifMax31855_Clear(PifMax31855* p_owner)
{
#ifdef COLLECT_SIGNAL_MAX31855
	pifMax31855_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, M3_CSF_ALL_BIT);
#endif

	if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
	}

	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifMax31855_AttachFilter(PifMax31855* p_owner, PifNoiseFilter* p_filter)
{
	if (!p_owner || !pifNoiseFilter_Check(p_filter, NFV_INT16)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_filter = p_filter;
	return TRUE;
}

BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal)
{
	int16_t quarter;

	if (!_measure(p_owner, p_temperature ? &quarter : NULL, p_internal)) return FALSE;
	if (p_temperature) *p_temperature = quarter / 4.0;
	return TRUE;
}

BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period1ms,
	PifEvtMax31855Measure evt_measure, PifIssuerP p_issuer, const char *name)
{
	if (!p_owner || !period1ms) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}
	if (p_owner->_p_task) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1ms * 1000UL, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = name ? name : "MAX31855";

	p_owner->__evt_measure = evt_measure;
	p_owner->__p_issuer = p_issuer;
	return TRUE;
}

void pifMax31855_StopMeasurement(PifMax31855* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold)
{
	if (low_threshold > high_threshold) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__low_threshold = _toQuarter(low_threshold);
	p_owner->__high_threshold = _toQuarter(high_threshold);
	p_owner->__threshold_set = TRUE;
	return TRUE;
}

#ifdef COLLECT_SIGNAL_MAX31855

BOOL pifMax31855_SetCsFlag(PifMax31855* p_owner, uint8_t index, PifMax31855CsFlag flag)
{
	(void)index;
	if (flag & M3_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[M3_CSF_STATE_IDX], "M3", p_owner->parent._id, CSVT_WIRE, 1,
				p_owner->parent._curr_state)) return FALSE;
	}
	return TRUE;
}

void pifMax31855_ResetCsFlag(PifMax31855* p_owner, uint8_t index, PifMax31855CsFlag flag)
{
	(void)index;
	if (flag & M3_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[M3_CSF_STATE_IDX]);
}

#endif	// COLLECT_SIGNAL_MAX31855
