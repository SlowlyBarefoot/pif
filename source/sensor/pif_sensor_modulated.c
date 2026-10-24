// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_sensor_modulated.h"

// The task steps the cycle and an interrupt may add the samples. A window opens by emptying the sums of the
// channels before the phase turns to a sample phase, and closes by turning the phase away before the sums are
// read. The interrupt adds only while the phase is a sample phase, so it never touches a window the task is
// emptying or reading, and the two need no lock. The task adds the samples of act_acquire inside
// pif_IrqDisable(), as the interrupt may be adding to the same window.


/**
 * @fn _acquire
 * @brief Reads every channel with act_acquire and adds the samples to the open window.
 * @param p_owner Pointer to the owner instance.
 */
static void _acquire(PifSensorModulated* p_owner)
{
	PifSensorModulatedChannel* p_channel;
	int32_t value;
	uint32_t irq;
	uint8_t i;

	for (i = 0; i < p_owner->_channel_count; i++) {
		value = (*p_owner->__act_acquire)(p_owner, i);
		p_channel = &p_owner->__p_channel[i];
		irq = pif_IrqDisable();
		if (p_channel->__count < 0xFFFF) {
			p_channel->__sum += value;
			p_channel->__count++;
		}
		pif_IrqRestore(irq);
	}
}

/**
 * @fn _openWindow
 * @brief Empties the sums of every channel, then lets the samples count. With act_acquire it reads the first
 *        samples at once.
 * @param p_owner Pointer to the owner instance.
 * @param phase SMP_ON_SAMPLE or SMP_OFF_SAMPLE.
 * @return Time in microseconds until the next step.
 */
static uint32_t _openWindow(PifSensorModulated* p_owner, PifSensorModulatedPhase phase)
{
	PifSensorModulatedChannel* p_channel;
	uint8_t i;

	for (i = 0; i < p_owner->_channel_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		p_channel->__sum = 0;
		p_channel->__count = 0;
	}
	p_owner->_phase = phase;

	p_owner->__window_acquire = p_owner->__act_acquire != NULL;
	if (!p_owner->__window_acquire) return p_owner->__sample_us;

	p_owner->__window_end = (*pif_act_timer1us)() + p_owner->__sample_us;
	_acquire(p_owner);
	return (p_owner->__acquire_us < p_owner->__sample_us) ? p_owner->__acquire_us : p_owner->__sample_us;
}

/**
 * @fn _continueWindow
 * @brief While a window read with act_acquire lasts, reads every channel again.
 * @param p_owner Pointer to the owner instance.
 * @param p_delay Time in microseconds until the next read or the end of the window, when it lasts.
 * @return TRUE while the window lasts, FALSE when it is over or not read with act_acquire.
 */
static BOOL _continueWindow(PifSensorModulated* p_owner, uint32_t* p_delay)
{
	uint32_t remain;

	if (!p_owner->__window_acquire || !p_owner->__act_acquire) return FALSE;

	remain = p_owner->__window_end - (*pif_act_timer1us)();
	if ((int32_t)remain <= 0) return FALSE;

	_acquire(p_owner);
	*p_delay = (remain < p_owner->__acquire_us) ? remain : p_owner->__acquire_us;
	return TRUE;
}

/**
 * @fn _closeWindow
 * @brief Averages the window of every channel. The phase has already left the sample phase.
 * @param p_owner Pointer to the owner instance.
 * @param on TRUE for the window with the LED on.
 */
static void _closeWindow(PifSensorModulated* p_owner, BOOL on)
{
	PifSensorModulatedChannel* p_channel;
	uint16_t count;
	uint8_t i;

	for (i = 0; i < p_owner->_channel_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		count = p_channel->__count;
		if (on) {
			p_channel->__on_valid = count != 0;
			if (count) p_channel->_on_level = p_channel->__sum / count;
		}
		else {
			p_channel->__off_valid = count != 0;
			if (count) p_channel->_off_level = p_channel->__sum / count;
		}
	}
}

/**
 * @fn _level
 * @brief Returns one level of a channel.
 */
static int32_t _level(PifSensorModulatedChannel* p_channel, PifSensorModulatedLevel level)
{
	switch (level) {
	case SML_ON:	return p_channel->_on_level;
	case SML_OFF:	return p_channel->_off_level;
	default:		return p_channel->_on_level - p_channel->_off_level;
	}
}

/**
 * @fn _isValid
 * @brief Tells whether the windows behind one level of a channel had samples in the last cycle.
 */
static BOOL _isValid(PifSensorModulatedChannel* p_channel, PifSensorModulatedLevel level)
{
	switch (level) {
	case SML_ON:	return p_channel->__on_valid;
	case SML_OFF:	return p_channel->__off_valid;
	default:		return p_channel->__on_valid && p_channel->__off_valid;
	}
}

/**
 * @fn _endCycle
 * @brief Hands the levels to the attached sensors and reports the end of the cycle.
 * @param p_owner Pointer to the owner instance.
 */
static void _endCycle(PifSensorModulated* p_owner)
{
	PifSensorModulatedChannel* p_channel;
	uint8_t i;

	for (i = 0; i < p_owner->_channel_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		if (!p_channel->__p_sensor || !_isValid(p_channel, p_channel->__level)) continue;

		pifSensorThreshold_sigData(p_channel->__p_sensor, _level(p_channel, p_channel->__level));
		pifSensorThreshold_ProcessAcquire(p_channel->__p_sensor);
	}
	p_owner->_cycle_count++;
	if (p_owner->evt_cycle) (*p_owner->evt_cycle)(p_owner);
}

/**
 * @fn _doTask
 * @brief Steps the cycle. As a TM_EXTERNAL task it returns the time until its next step.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Time in microseconds until the next step, or 0 when stopped.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifSensorModulated* p_owner = (PifSensorModulated*)p_task->_p_client;
	uint32_t delay;

	if (!p_owner->__running) return 0;

	switch (p_owner->_phase) {
	case SMP_ON_SETTLE:
		return _openWindow(p_owner, SMP_ON_SAMPLE);

	case SMP_ON_SAMPLE:
		if (_continueWindow(p_owner, &delay)) return delay;
		p_owner->_phase = SMP_OFF_SETTLE;
		_closeWindow(p_owner, TRUE);
		(*p_owner->__act_led)(p_owner, OFF);
		if (p_owner->__settle_us) return p_owner->__settle_us;
		return _openWindow(p_owner, SMP_OFF_SAMPLE);

	case SMP_OFF_SETTLE:
		return _openWindow(p_owner, SMP_OFF_SAMPLE);

	case SMP_OFF_SAMPLE:
		if (_continueWindow(p_owner, &delay)) return delay;
		p_owner->_phase = SMP_ON_SETTLE;
		_closeWindow(p_owner, FALSE);
		_endCycle(p_owner);
		// evt_cycle or the change event of a sensor may have stopped it.
		if (!p_owner->__running) return 0;
		break;

	default:		// SMP_IDLE, just started
		break;
	}

	// A cycle begins with the LED on.
	p_owner->_phase = SMP_ON_SETTLE;
	(*p_owner->__act_led)(p_owner, ON);
	if (p_owner->__settle_us) return p_owner->__settle_us;
	return _openWindow(p_owner, SMP_ON_SAMPLE);
}

BOOL pifSensorModulated_Init(PifSensorModulated* p_owner, PifId id, uint8_t channel_count, PifActSensorModulatedLed act_led)
{
	if (!p_owner || !channel_count || !act_led) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSensorModulated));

	p_owner->__p_channel = calloc(channel_count, sizeof(PifSensorModulatedChannel));
	if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;

	// TM_EXTERNAL: released by pifSensorModulated_Start() and then by the time each step returns.
	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, p_owner, FALSE);
	if (!p_owner->_p_task) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
		return FALSE;
	}
	p_owner->_p_task->name = "Modulated";

	p_owner->_channel_count = channel_count;
	p_owner->__act_led = act_led;
	return TRUE;
}

void pifSensorModulated_Clear(PifSensorModulated* p_owner)
{
	if (p_owner->_p_task) {
		if (p_owner->__running) pifSensorModulated_Stop(p_owner);
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->__p_channel) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
	}
	p_owner->_channel_count = 0;
}

BOOL pifSensorModulated_SetTiming(PifSensorModulated* p_owner, uint32_t settle_us, uint32_t sample_us)
{
	if (!p_owner || !sample_us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__settle_us = settle_us;
	p_owner->__sample_us = sample_us;
	return TRUE;
}

BOOL pifSensorModulated_AttachActAcquire(PifSensorModulated* p_owner, PifActSensorModulatedAcquire act_acquire,
		uint32_t period_us)
{
	if (!p_owner || (act_acquire && !period_us)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__act_acquire = act_acquire;
	p_owner->__acquire_us = period_us;
	return TRUE;
}

BOOL pifSensorModulated_AttachSensor(PifSensorModulated* p_owner, uint8_t channel, PifSensorThreshold* p_sensor,
		PifSensorModulatedLevel level)
{
	PifSensorModulatedChannel* p_channel;

	if (!p_owner || channel >= p_owner->_channel_count || level > SML_DIFF) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_channel = &p_owner->__p_channel[channel];
	p_channel->__p_sensor = p_sensor;
	p_channel->__level = level;
	return TRUE;
}

BOOL pifSensorModulated_Start(PifSensorModulated* p_owner)
{
	uint8_t i;

	if (!p_owner->__sample_us || !p_owner->_p_task) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// The levels of a cycle before are kept, but none of them is handed over again.
	for (i = 0; i < p_owner->_channel_count; i++) {
		p_owner->__p_channel[i].__on_valid = FALSE;
		p_owner->__p_channel[i].__off_valid = FALSE;
	}
	p_owner->_phase = SMP_IDLE;
	p_owner->_cycle_count = 0;
	p_owner->__running = TRUE;
	return pifTask_SetTrigger(p_owner->_p_task, 0);
}

void pifSensorModulated_Stop(PifSensorModulated* p_owner)
{
	p_owner->__running = FALSE;
	p_owner->_phase = SMP_IDLE;
	if (p_owner->_p_task) pifTask_CancelTrigger(p_owner->_p_task);
	(*p_owner->__act_led)(p_owner, OFF);
}

void pifSensorModulated_sigSample(PifSensorModulated* p_owner, uint8_t channel, int32_t value)
{
	PifSensorModulatedChannel* p_channel;
	PifSensorModulatedPhase phase = p_owner->_phase;

	if (phase != SMP_ON_SAMPLE && phase != SMP_OFF_SAMPLE) return;
	if (channel >= p_owner->_channel_count) return;

	p_channel = &p_owner->__p_channel[channel];
	if (p_channel->__count == 0xFFFF) return;
	p_channel->__sum += value;
	p_channel->__count++;
}

int32_t pifSensorModulated_GetLevel(PifSensorModulated* p_owner, uint8_t channel, PifSensorModulatedLevel level)
{
	if (!p_owner || channel >= p_owner->_channel_count) return 0;
	return _level(&p_owner->__p_channel[channel], level);
}
