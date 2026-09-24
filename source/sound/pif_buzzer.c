#include "sound/pif_buzzer.h"


/**
 * @fn _setOutput
 * @brief Drives the buzzer output and reports the edge, only when the output actually changes.
 * @param p_owner Pointer to the buzzer instance.
 * @param on Requested output level.
 */
static void _setOutput(PifBuzzer* p_owner, BOOL on)
{
	if (p_owner->_output == on) return;

	(*p_owner->__act_action)(on);
	p_owner->_output = on;
	if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, on);
}

/**
 * @fn _finish
 * @brief Ends the sequence: turns the output off, returns to idle and reports the finish.
 * @param p_owner Pointer to the buzzer instance.
 */
static void _finish(PifBuzzer* p_owner)
{
	_setOutput(p_owner, OFF);
	p_owner->_state = BS_IDLE;
	if (p_owner->evt_finish) (*p_owner->evt_finish)(p_owner->_id);
}

/**
 * @fn _nextStep
 * @brief Loads the next non-zero duration of the sequence, handling end markers wherever they appear.
 * @param p_owner Pointer to the buzzer instance.
 */
static void _nextStep(PifBuzzer* p_owner)
{
	uint8_t value, repeat;
	BOOL on, restarted = FALSE;

	for (;;) {
		value = p_owner->__p_sequence[p_owner->__pos];
		if (value >= PIF_BUZZER_STOP) {
			repeat = value - PIF_BUZZER_STOP;
			// A second restart within one step means the whole sequence has no duration.
			if (p_owner->__repeat < repeat && !restarted) {
				p_owner->__repeat++;
				p_owner->__pos = 0;
				restarted = TRUE;
				continue;
			}
			_finish(p_owner);
			return;
		}

		// Even positions are ON durations, odd positions are OFF durations.
		on = !(p_owner->__pos & 1);
		p_owner->__pos++;
		if (value) {
			// _state is still BS_START here for the first edge of a sequence.
			_setOutput(p_owner, on);
			p_owner->__count = value;
			p_owner->_state = on ? BS_ON : BS_OFF;
			return;
		}
		// A zero duration is skipped, so the output keeps its level.
	}
}

/**
 * @fn _doTask
 * @brief Periodic buzzer state-machine task that processes sequence timing and transitions.
 * @param p_task Pointer to the scheduler task context containing the buzzer instance.
 * @return Always returns 0 because the task period is fixed by the task manager.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifBuzzer* p_owner = (PifBuzzer*)p_task->_p_client;

	if (p_owner->evt_period) (*p_owner->evt_period)(p_owner->_id);

	switch (p_owner->_state) {
	case BS_START:
		_nextStep(p_owner);
		break;

	case BS_ON:
	case BS_OFF:
		if (p_owner->__count) p_owner->__count--;
		if (!p_owner->__count) _nextStep(p_owner);
		break;

	default:
		break;
	}
	return 0;
}

BOOL pifBuzzer_Init(PifBuzzer* p_owner, PifId id, uint16_t period1ms, PifActBuzzerAction act_action)
{
	if (!p_owner || !period1ms || !act_action) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBuzzer));

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1ms * 1000, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "Buzzer";

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__act_action = act_action;
    return TRUE;
}

void pifBuzzer_Clear(PifBuzzer* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifBuzzer_Start(PifBuzzer* p_owner, const uint8_t* p_sequence)
{
	if (!p_owner || !p_sequence) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->_p_task) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_sequence = p_sequence;
	p_owner->__pos = 0;
	p_owner->__repeat = 0;
	p_owner->__count = 0;
	p_owner->_state = BS_START;
	return TRUE;
}

void pifBuzzer_Stop(PifBuzzer* p_owner)
{
	if (!p_owner || !p_owner->__act_action) return;

	// Drive OFF even if the output is believed off, so the hardware is left silent.
	(*p_owner->__act_action)(OFF);
	if (p_owner->_output) {
		p_owner->_output = OFF;
		if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, OFF);
	}
	p_owner->_state = BS_IDLE;
}

BOOL pifBuzzer_State(PifBuzzer* p_owner)
{
    return p_owner->_output;
}
