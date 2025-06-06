#include "sound/pif_buzzer.h"


static uint32_t _doTask(PifTask* p_task)
{
	PifBuzzer* p_owner = (PifBuzzer*)p_task->_p_client;
	uint8_t repeat;
	static uint16_t count;

	if (p_owner->evt_period) (*p_owner->evt_period)(p_owner->_id);

	switch (p_owner->_state) {
	case BS_START:
		count = p_owner->__p_sequence[p_owner->__pos++];
		(*p_owner->__act_action)(ON);
		if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, ON);
		p_owner->_state = BS_ON;
		break;

	case BS_ON:
		if (count) count--;
		else {
			count = p_owner->__p_sequence[p_owner->__pos++];
			(*p_owner->__act_action)(OFF);
			if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, OFF);
			p_owner->_state = BS_OFF;
		}
		break;

	case BS_OFF:
		if (count) count--;
		else {
			count = p_owner->__p_sequence[p_owner->__pos++];
			if (count < PIF_BUZZER_STOP) {
				(*p_owner->__act_action)(ON);
				if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, ON);
				p_owner->_state = BS_ON;
			}
			else if (count == PIF_BUZZER_STOP) {
				p_owner->_state = BS_STOP;
			}
			else {
				repeat = count - PIF_BUZZER_STOP;
				if (p_owner->__repeat < repeat) {
					p_owner->__pos = 0;
					p_owner->__repeat++;
					p_owner->_state = BS_START;
				}
				else {
					p_owner->_state = BS_STOP;
				}
			}
		}
		break;

	case BS_STOP:
		(*p_owner->__act_action)(OFF);
		p_owner->_state = BS_IDLE;
		if (p_owner->evt_finish) (*p_owner->evt_finish)(p_owner->_id);
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
	if (!p_owner->_p_task) return FALSE;

	p_owner->__p_sequence = p_sequence;
	p_owner->__pos = 0;
	p_owner->__repeat = 0;
	p_owner->_state = BS_START;
	return TRUE;
}

void pifBuzzer_Stop(PifBuzzer* p_owner)
{
	(*p_owner->__act_action)(OFF);
	if (p_owner->evt_change) (*p_owner->evt_change)(p_owner->_id, OFF);
	p_owner->_state = BS_STOP;
}

BOOL pifBuzzer_State(PifBuzzer* p_owner)
{
    return p_owner->_state == BS_START || p_owner->_state == BS_ON;
}

