#include "pif_timer.h"


static uint16_t _doTask(PifTask* p_task)
{
	PifTimerManager* p_manager = p_task->_p_client;

	PifFixListIterator it = pifFixList_Begin(&p_manager->__timers);
	while (it) {
		PifTimer* p_timer = (PifTimer*)it->data;

		if (p_timer->__event) {
			p_timer->__event = FALSE;

			if (p_timer->__evt_finish) (*p_timer->__evt_finish)(p_timer->__p_finish_issuer);
		}

		it = pifFixList_Next(it);
	}
	return 0;
}

BOOL pifTimerManager_Init(PifTimerManager* p_manager, PifId id, uint32_t period1us, int max_count)
{
    if (!p_manager || !period1us) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_manager, 0, sizeof(PifTimerManager));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_manager->_id = id;
    if (!pifFixList_Init(&p_manager->__timers, sizeof(PifTimer), max_count)) goto fail;
    p_manager->_period1us = period1us;

    p_manager->__p_task = pifTaskManager_Add(TM_ALWAYS, 100, _doTask, p_manager, TRUE);
    if (!p_manager->__p_task) goto fail;
    return TRUE;

fail:
	pifTimerManager_Clear(p_manager);
    return FALSE;
}

void pifTimerManager_Clear(PifTimerManager* p_manager)
{
	if (p_manager->__p_task) {
		pifTaskManager_Remove(p_manager->__p_task);
		p_manager->__p_task = NULL;
	}
	pifFixList_Clear(&p_manager->__timers, NULL);
}

PifTimer* pifTimerManager_Add(PifTimerManager* p_manager, PifTimerType type)
{
	PifTimer* p_timer = (PifTimer*)pifFixList_AddFirst(&p_manager->__timers);
    if (!p_timer) return NULL;

    p_timer->_type = type;
    p_timer->_step = TS_STOP;
    return p_timer;
}

void pifTimerManager_Remove(PifTimer* p_timer)
{
	p_timer->_step = TS_REMOVE;
}

int pifTimerManager_Count(PifTimerManager* p_manager)
{
	return pifFixList_Count(&p_manager->__timers);
}

BOOL pifTimer_Start(PifTimer* p_owner, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (p_owner->_step == TS_STOP) {
    	p_owner->_step = TS_RUNNING;
    	p_owner->__event = FALSE;
    }
    p_owner->target = target;
    p_owner->__current = target;

    if (p_owner->_type == TT_PWM) {
    	p_owner->__pwm_duty = 0;
    }
    return TRUE;
}

void pifTimer_Stop(PifTimer* p_owner)
{
	p_owner->__current = 0;
	p_owner->_step = TS_STOP;
	if (p_owner->_type == TT_PWM) {
		(*p_owner->act_pwm)(OFF);
	}
}

void pifTimer_Reset(PifTimer* p_owner)
{
	p_owner->__current = p_owner->target;
	p_owner->_step = TS_RUNNING;
}

void pifTimer_SetPwmDuty(PifTimer* p_owner, uint16_t duty)
{
	p_owner->__pwm_duty = p_owner->target * duty / PIF_PWM_MAX_DUTY;
	if (p_owner->__pwm_duty == p_owner->target) {
		(*p_owner->act_pwm)(ON);
	}
}

uint32_t pifTimer_Remain(PifTimer* p_owner)
{
	if (p_owner->_step != TS_RUNNING) return 0;
	else return p_owner->__current;
}

uint32_t pifTimer_Elapsed(PifTimer* p_owner)
{
	if (p_owner->_step != TS_RUNNING) return 0;
	else return p_owner->target - p_owner->__current;
}

void pifTimerManager_sigTick(PifTimerManager* p_manager)
{
	PifTimer* p_remove = NULL;

    if (!p_manager) return;

    PifFixListIterator it = pifFixList_Begin(&p_manager->__timers);
	while (it) {
		PifTimer* p_timer = (PifTimer*)it->data;

		if (p_timer->_step == TS_REMOVE) {
			if (!p_remove) p_remove = p_timer;
		}
		else if (p_timer->__current) {
			p_timer->__current--;
			switch (p_timer->_type) {
			case TT_ONCE:
				if (!p_timer->__current) {
					p_timer->_step = TS_STOP;
					p_timer->__event = TRUE;
					if (p_timer->__evt_int_finish) {
						(*p_timer->__evt_int_finish)(p_timer->__p_int_finish_issuer);
					}
				}
				break;

			case TT_REPEAT:
				if (!p_timer->__current) {
					p_timer->__current = p_timer->target;
					p_timer->__event = TRUE;
					if (p_timer->__evt_int_finish) {
						(*p_timer->__evt_int_finish)(p_timer->__p_int_finish_issuer);
					}
				}
				break;

			case TT_PWM:
				if (p_timer->__pwm_duty != p_timer->target) {
					if (!p_timer->__current) {
						(*p_timer->act_pwm)(OFF);
						p_timer->__current = p_timer->target;
					}
					if (p_timer->__current == p_timer->__pwm_duty) {
						(*p_timer->act_pwm)(ON);
					}
				}
				else {
					if (!p_timer->__current) {
						p_timer->__current = p_timer->target;
					}
				}
				break;
			}
		}

		it = pifFixList_Next(it);
	}

	if (p_remove) pifFixList_Remove(&p_manager->__timers, p_remove);
}

void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, void* p_issuer)
{
	p_owner->__evt_finish = evt_finish;
	p_owner->__p_finish_issuer = p_issuer;
}

void pifTimer_AttachEvtIntFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, void* p_issuer)
{
	p_owner->__evt_int_finish = evt_finish;
	p_owner->__p_int_finish_issuer = p_issuer;
}
