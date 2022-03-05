#include "pif_buzzer.h"


static uint16_t _doTask(PifTask* p_task)
{
	PifBuzzer* p_owner = (PifBuzzer*)p_task->_p_client;
	uint8_t repeat;
	uint16_t sound_10ms, delay = 0;

	sound_10ms = p_owner->__p_sound_10ms[p_owner->__pos];
	switch (p_owner->_state) {
	case BS_START:
		(*p_owner->__act_action)(p_owner->_id, ON);
		p_owner->__pos++;
		p_owner->_state = BS_ON;
		break;

	case BS_ON:
		(*p_owner->__act_action)(p_owner->_id, OFF);
		p_owner->__pos++;
		p_owner->_state = BS_OFF;
		break;

	case BS_OFF:
		if (sound_10ms < 0xF0) {
			(*p_owner->__act_action)(p_owner->_id, ON);
			p_owner->__pos++;
			p_owner->_state = BS_ON;
		}
		else if (sound_10ms == 0xF0) {
			p_owner->_state = BS_STOP;
			delay = 1;
		}
		else {
			repeat = sound_10ms - 0xF0;
			if (p_owner->__repeat < repeat) {
				p_owner->__pos = 0;
				p_owner->__repeat++;
				p_owner->_state = BS_START;
			}
			else {
				p_owner->_state = BS_STOP;
			}
			delay = 1;
		}
		break;

	case BS_STOP:
		(*p_owner->__act_action)(p_owner->_id, OFF);
		p_owner->_state = BS_IDLE;
		delay = 1;
		p_task->pause = TRUE;
		break;

	default:
		break;
	}
	return delay ? delay : sound_10ms * 10;
}

BOOL pifBuzzer_Init(PifBuzzer* p_owner, PifId id, PifActBuzzerAction act_action)
{
	if (!p_owner || !act_action) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBuzzer));

	p_owner->_p_task = pifTaskManager_Add(TM_CHANGE_MS, 1, _doTask, p_owner, FALSE);
	if (!p_owner->_p_task) return FALSE;

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

void pifBuzzer_Start(PifBuzzer* p_owner, const uint8_t* p_sound_10ms)
{
	p_owner->__p_sound_10ms = p_sound_10ms;
	p_owner->__pos = 0;
	p_owner->__repeat = 0;
	p_owner->_state = BS_START;
	p_owner->_p_task->pause = FALSE;
}

void pifBuzzer_Stop(PifBuzzer* p_owner)
{
	p_owner->_state = BS_STOP;
}
