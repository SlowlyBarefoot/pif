#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif
#include "pif_pulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


static uint16_t _doTask(PifTask* p_task)
{
	PifPulse* p_owner = p_task->_p_client;

	PifFixListIterator it = pifFixList_Begin(&p_owner->__items);
	while (it) {
		PifPulseItem* p_item = (PifPulseItem*)it->data;

		if (p_item->__event) {
			p_item->__event = FALSE;

			if (p_item->__evt_finish) (*p_item->__evt_finish)(p_item->__p_finish_issuer);
		}

		it = pifFixList_Next(it);
	}
	return 0;
}

PifPulse* pifPulse_Create(PifId id, uint32_t period1us, int max_count)
{
	PifPulse* p_owner = malloc(sizeof(PifPulse));
    if (!p_owner) {
    	pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifPulse_Init(p_owner, id, period1us, max_count)) {
		pifPulse_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifPulse_Destroy(PifPulse** pp_owner)
{
	if (*pp_owner) {
		pifPulse_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifPulse_Init(PifPulse* p_owner, PifId id, uint32_t period1us, int max_count)
{
    if (!p_owner || !period1us) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    if (!pifFixList_Init(&p_owner->__items, sizeof(PifPulseItem), max_count)) goto fail;
    p_owner->_period1us = period1us;

    p_owner->__p_task = pifTaskManager_Add(TM_RATIO, 100, _doTask, p_owner, TRUE);
    if (!p_owner->__p_task) goto fail;
    return TRUE;

fail:
	pifPulse_Clear(p_owner);
    return FALSE;
}

void pifPulse_Clear(PifPulse* p_owner)
{
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	pifFixList_Clear(&p_owner->__items);
}

PifPulseItem* pifPulse_AddItem(PifPulse* p_owner, PifPulseType type)
{
    PifPulseItem* p_item = (PifPulseItem*)pifFixList_AddFirst(&p_owner->__items);
    if (!p_item) return NULL;

    p_item->_type = type;
    p_item->_step = PS_STOP;
    return p_item;
}

void pifPulse_RemoveItem(PifPulseItem* p_item)
{
    p_item->_step = PS_REMOVE;
}

int pifPulse_Count(PifPulse* p_owner)
{
	return pifFixList_Count(&p_owner->__items);
}

BOOL pifPulse_StartItem(PifPulseItem* p_item, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (p_item->_step == PS_STOP) {
    	p_item->_step = PS_RUNNING;
    	p_item->__event = FALSE;
    }
    p_item->target = target;
    p_item->__current = target;

    if (p_item->_type == PT_PWM) {
    	p_item->__pwm_duty = 0;
    }
    return TRUE;
}

void pifPulse_StopItem(PifPulseItem* p_item)
{
	p_item->__current = 0;
	p_item->_step = PS_STOP;
	if (p_item->_type == PT_PWM) {
		(*p_item->act_pwm)(OFF);
	}
}

void pifPulse_SetPwmDuty(PifPulseItem* p_item, uint16_t duty)
{
	p_item->__pwm_duty = p_item->target * duty / PIF_PWM_MAX_DUTY;
	if (p_item->__pwm_duty == p_item->target) {
		(*p_item->act_pwm)(ON);
	}
}

uint32_t pifPulse_RemainItem(PifPulseItem* p_item)
{
	if (p_item->_step != PS_RUNNING) return 0;
	else return p_item->__current;
}

uint32_t pifPulse_ElapsedItem(PifPulseItem* p_item)
{
	if (p_item->_step != PS_RUNNING) return 0;
	else return p_item->target - p_item->__current;
}

void pifPulse_sigTick(PifPulse* p_owner)
{
	PifPulseItem* p_remove = NULL;

    if (!p_owner) return;

    PifFixListIterator it = pifFixList_Begin(&p_owner->__items);
	while (it) {
		PifPulseItem* p_item = (PifPulseItem*)it->data;

		if (p_item->_step == PS_REMOVE) {
			if (!p_remove) p_remove = p_item;
		}
		else if (p_item->__current) {
			p_item->__current--;
			switch (p_item->_type) {
			case PT_ONCE:
				if (!p_item->__current) {
					p_item->_step = PS_STOP;
					p_item->__event = TRUE;
					if (p_item->__evt_int_finish) {
						(*p_item->__evt_int_finish)(p_item->__p_int_finish_issuer);
					}
				}
				break;

			case PT_REPEAT:
				if (!p_item->__current) {
					p_item->__current = p_item->target;
					p_item->__event = TRUE;
					if (p_item->__evt_int_finish) {
						(*p_item->__evt_int_finish)(p_item->__p_int_finish_issuer);
					}
				}
				break;

			case PT_PWM:
				if (p_item->__pwm_duty != p_item->target) {
					if (!p_item->__current) {
						(*p_item->act_pwm)(OFF);
						p_item->__current = p_item->target;
					}
					if (p_item->__current == p_item->__pwm_duty) {
						(*p_item->act_pwm)(ON);
					}
				}
				else {
					if (!p_item->__current) {
						p_item->__current = p_item->target;
					}
				}
				break;
			}
		}

		it = pifFixList_Next(it);
	}

	if (p_remove) pifFixList_Remove(&p_owner->__items, p_remove);
}

void pifPulse_AttachEvtFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer)
{
	p_item->__evt_finish = evt_finish;
	p_item->__p_finish_issuer = p_issuer;
}

void pifPulse_AttachEvtIntFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer)
{
	p_item->__evt_int_finish = evt_finish;
	p_item->__p_int_finish_issuer = p_issuer;
}
