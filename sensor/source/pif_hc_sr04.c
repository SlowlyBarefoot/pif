#include "pif_hc_sr04.h"
#include "pif_task.h"


static uint16_t _doTask(PifTask* p_task)
{
	pifHcSr04_Trigger((PifHcSr04*)p_task->_p_client);
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

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    pifHcSr04_SetTemperature(p_owner, 20);

    return TRUE;
}

void pifHcSr04_Trigger(PifHcSr04* p_owner)
{
	(*p_owner->act_trigger)(ON);
	pif_Delay1us(10);
	(*p_owner->act_trigger)(OFF);
}

void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature)
{
	p_owner->__transform_const = 2.0 / ((331.6 + 0.6 * temperature) / 10000.0);		// 2 : 왕복, 10000 : m/s -> cm/ms
}

PifTask* pifHcSr04_AttachTask(PifHcSr04* p_owner, uint16_t period, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(TM_PERIOD_MS, period, _doTask, p_owner, start);
	return p_owner->_p_task;
}

void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state)
{
	int32_t distance;

	if (!state) {
		if (p_owner->evt_distance) {
			distance = ((*pif_act_timer1us)() - p_owner->__tigger_time_us) / p_owner->__transform_const;
			(*p_owner->evt_distance)(distance);
		}
		p_owner->__tigger_time_us = 0;
	}
	else {
		p_owner->__tigger_time_us = (*pif_act_timer1us)();
	}
}
