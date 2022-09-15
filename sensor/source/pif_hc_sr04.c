#include "pif_hc_sr04.h"
#include "pif_task.h"


static uint16_t _doTask(PifTask* p_task)
{
	PifHcSr04* p_owner = (PifHcSr04*)p_task->_p_client;

	if (p_owner->__timer > 10) p_owner->__timer -= 10; else p_owner->__timer = 0;
	if (p_owner->__period > -1 && !p_owner->__timer) {
		if (p_owner->__state == HSS_HIGH) {
			if (p_owner->evt_distance) (*p_owner->evt_distance)(p_owner->__period * 1000L / p_owner->_transform_const);
	    }
		pifHcSr04_Trigger(p_owner);
		p_owner->__timer = p_owner->__period;
	}

	if (p_owner->__state == HSS_LOW) {
		if (p_owner->evt_distance) (*p_owner->evt_distance)(p_owner->__distance);
		p_owner->__state = HSS_READY;
    }
	return 0;
}

BOOL pifHcSr04_Init(PifHcSr04* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (!pif_act_timer1us) {
		pif_error = E_CANNOT_USE;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifHcSr04));

	p_owner->_p_task = pifTaskManager_Add(TM_PERIOD_MS, 10, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    pifHcSr04_SetTemperature(p_owner, 20);

    return TRUE;
}

void pifHcSr04_Clear(PifHcSr04* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

void pifHcSr04_Trigger(PifHcSr04* p_owner)
{
	(*p_owner->act_trigger)(ON);
	pif_Delay1us(10);
	(*p_owner->act_trigger)(OFF);
	p_owner->__state = HSS_TRIGGER;
}

BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period)
{
	if (!p_owner || !period) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	p_owner->__period = period;
	p_owner->__timer = 0;
	return TRUE;
}

void pifHcSr04_StopTrigger(PifHcSr04* p_owner)
{
	p_owner->__period = -1;
}

void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature)
{
	p_owner->_transform_const = 2.0 / ((331.6 + 0.6 * temperature) / 10000.0);		// 2 : 왕복, 10000 : m/s -> cm/ms
}

void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state)
{
	switch (p_owner->__state) {
	case HSS_TRIGGER:
		if (state) {
			p_owner->__tigger_time_us = (*pif_act_timer1us)();
			p_owner->__state = HSS_HIGH;
		}
		break;

	case HSS_HIGH:
		if (!state) {
			p_owner->__distance = ((*pif_act_timer1us)() - p_owner->__tigger_time_us) / p_owner->_transform_const;
			p_owner->__state = HSS_LOW;
		}
		break;

	default:
		break;
	}
}
