#include "core/pif_task.h"
#include "sensor/pif_hc_sr04.h"


static uint32_t _doTask(PifTask* p_task)
{
	PifHcSr04* p_owner = (PifHcSr04*)p_task->_p_client;

	switch (p_owner->__state) {
	case HSS_READY:
		pifHcSr04_Trigger(p_owner);
		break;

	case HSS_LOW:
		if (p_owner->evt_read) (*p_owner->evt_read)(p_owner->__distance);
		p_owner->__state = HSS_READY;
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
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

void pifHcSr04_Trigger(PifHcSr04* p_owner)
{
	(*p_owner->act_trigger)(ON);
	pif_Delay1us(11);
	(*p_owner->act_trigger)(OFF);
	p_owner->__state = HSS_TRIGGER;
}

BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period)
{
	if (!p_owner || !period) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	pifTask_ChangePeriod(p_owner->_p_task, period);
	p_owner->_p_task->pause = FALSE;
	return TRUE;
}

void pifHcSr04_StopTrigger(PifHcSr04* p_owner)
{
	p_owner->_p_task->pause = TRUE;
}

void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature)
{
	p_owner->_transform_const = 2.0f / ((331.6f + 0.6f * temperature) / 10000.0f);		// 2 : 왕복, 10000 : m/s -> cm/ms
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
			pifTask_SetTrigger(p_owner->_p_task, 0);
		}
		break;

	default:
		break;
	}
}
