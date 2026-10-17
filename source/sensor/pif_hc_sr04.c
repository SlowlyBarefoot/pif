// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_hc_sr04.h"


/**
 * @brief Sets the state and records it for pifCollectSignal.
 * @param p_owner Pointer to the sensor instance.
 * @param state New state.
 */
static void _setState(PifHcSr04* p_owner, PifHcSr04State state)
{
	p_owner->__state = state;
#ifdef COLLECT_SIGNAL_HC_SR04
	pifCollectSignal_Put(&p_owner->__cs[HS_CSF_STATE_IDX], state);
#endif
}

/**
 * @fn _doTask
 * @brief Internal helper that supports do task logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifHcSr04* p_owner = (PifHcSr04*)p_task->_p_client;

	switch (p_owner->__state) {
	case HSS_READY:
		pifHcSr04_Trigger(p_owner);
		break;

	case HSS_LOW:
#ifdef COLLECT_SIGNAL_HC_SR04
		pifCollectSignal_Put(&p_owner->__cs[HS_CSF_DISTANCE_IDX], p_owner->__distance);
#endif
		if (p_owner->evt_read) (*p_owner->evt_read)(p_owner->__distance);
		_setState(p_owner, HSS_READY);
		break;

	case HSS_TRIGGER:
	case HSS_HIGH:
		// An echo that never started or never ended, from a missing sensor or a missed edge, would
		// otherwise hold the state here for good and no measurement would follow.
		if ((*pif_act_timer1us)() - p_owner->__trigger_time_us >= PIF_HC_SR04_ECHO_TIMEOUT_US) {
			pifHcSr04_Trigger(p_owner);
		}
		break;

	default:
		break;
	}
	return 0;
}

BOOL pifHcSr04_Init(PifHcSr04* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifHcSr04));

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, 50000, _doTask, p_owner, FALSE);		// 50ms
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "HC_SR04";

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    pifHcSr04_SetTemperature(p_owner, 20);

    return TRUE;
}

void pifHcSr04_Clear(PifHcSr04* p_owner)
{
#ifdef COLLECT_SIGNAL_HC_SR04
	pifHcSr04_ResetCsFlag(p_owner, HS_CSF_ALL_BIT);
#endif
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifHcSr04_Trigger(PifHcSr04* p_owner)
{
    if (!p_owner->act_trigger) return FALSE;

	(*p_owner->act_trigger)(ON);
	pif_Delay1us(11);
	(*p_owner->act_trigger)(OFF);
	// Overwritten by the rising edge of the echo; until then it times the wait for that edge.
	p_owner->__trigger_time_us = (*pif_act_timer1us)();
	_setState(p_owner, HSS_TRIGGER);
    return TRUE;
}

BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period1ms)
{
	if (!p_owner || !period1ms) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (!pifTask_ChangePeriod(p_owner->_p_task, period1ms * 1000UL)) return FALSE;
	_setState(p_owner, HSS_READY);
	pifTask_Resume(p_owner->_p_task);
	return TRUE;
}

void pifHcSr04_StopTrigger(PifHcSr04* p_owner)
{
	pifTask_Pause(p_owner->_p_task);
	// A measurement cut short here is dropped, rather than reported as stale on the next start.
	_setState(p_owner, HSS_READY);
}

void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature)
{
	p_owner->_transform_const = 2.0f / ((331.6f + 0.6f * temperature) / 10000.0f);		// 2: round trip, 10000: m/s -> cm/us
}

void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state)
{
	switch (p_owner->__state) {
	case HSS_TRIGGER:
		if (state) {
			p_owner->__trigger_time_us = (*pif_act_timer1us)();
			_setState(p_owner, HSS_HIGH);
		}
		break;

	case HSS_HIGH:
		if (!state) {
			p_owner->__distance = ((*pif_act_timer1us)() - p_owner->__trigger_time_us) / p_owner->_transform_const;
			_setState(p_owner, HSS_LOW);
			pifTask_SetTrigger(p_owner->_p_task, 0);
		}
		break;

	default:
		break;
	}
}

#ifdef COLLECT_SIGNAL_HC_SR04

BOOL pifHcSr04_SetCsFlag(PifHcSr04* p_owner, PifHcSr04CsFlag flag)
{
	if (flag & HS_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[HS_CSF_STATE_IDX], "HSS", p_owner->_id, CSVT_REG, 2,
				p_owner->__state)) return FALSE;
	}
	if (flag & HS_CSF_DISTANCE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[HS_CSF_DISTANCE_IDX], "HSD", p_owner->_id, CSVT_INTEGER, 32,
				p_owner->__distance)) return FALSE;
	}
	return TRUE;
}

void pifHcSr04_ResetCsFlag(PifHcSr04* p_owner, PifHcSr04CsFlag flag)
{
	if (flag & HS_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[HS_CSF_STATE_IDX]);
	if (flag & HS_CSF_DISTANCE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[HS_CSF_DISTANCE_IDX]);
}

#endif	// COLLECT_SIGNAL_HC_SR04
