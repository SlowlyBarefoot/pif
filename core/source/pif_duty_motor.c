#include "pif_log.h"
#include "pif_duty_motor.h"


void pifDutyMotor_Control(PifDutyMotor* p_owner)
{
    if (p_owner->__error) {
        if (p_owner->_state < MS_BREAK) {
			(*p_owner->act_set_duty)(0);
			p_owner->_current_duty = 0;

			p_owner->_state = MS_BREAK;
        }

        if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
    }

	if (p_owner->_state == MS_STOP) {
		p_owner->__p_task->pause = TRUE;
		p_owner->_state = MS_IDLE;
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
}

static void _evtTimerBreakFinish(void* p_issuer)
{
    PifDutyMotor* p_owner = (PifDutyMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
    	p_owner->_state = MS_REDUCE;
    }
    else {
    	(*p_owner->act_operate_break)(0);
    }
}

BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty)
{
    if (!p_owner || !p_timer_manager || !max_duty) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDutyMotor));

    p_owner->_p_timer_manager = p_timer_manager;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_max_duty = max_duty;
    return TRUE;
}

void pifDutyMotor_Clear(PifDutyMotor* p_owner)
{
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	if (p_owner->__p_timer_break) {
		pifTimerManager_Remove(p_owner->__p_timer_break);
		p_owner->__p_timer_break = NULL;
	}
}

void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction)
{
	p_owner->_direction = direction;

	if (p_owner->act_set_direction) (*p_owner->act_set_direction)(p_owner->_direction);
}

void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    if (duty > p_owner->_max_duty) duty = p_owner->_max_duty;
	p_owner->_current_duty = duty;

	(*p_owner->act_set_duty)(p_owner->_current_duty);
}

BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time)
{
	if (!p_owner->__p_timer_break) {
		p_owner->__p_timer_break = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	}
	if (p_owner->__p_timer_break) {
		pifTimer_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
		if (pifTimer_Start(p_owner->__p_timer_break, operating_time)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty)
{
    if (!p_owner->act_set_duty || !p_owner->act_set_direction) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    (*p_owner->act_set_direction)(p_owner->_direction);

   	p_owner->_current_duty = duty;

    (*p_owner->act_set_duty)(p_owner->_current_duty);
	return TRUE;
}

void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time)
{
    p_owner->_current_duty = 0;

    (*p_owner->act_set_duty)(p_owner->_current_duty);

    if (break_time && p_owner->act_operate_break) {
	    if (!p_owner->__p_timer_break) {
	    	p_owner->__p_timer_break = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	    }
	    if (p_owner->__p_timer_break) {
	    	pifTimer_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
			if (pifTimer_Start(p_owner->__p_timer_break, break_time)) {
		    	(*p_owner->act_operate_break)(1);
			}
	    }
	}
}

BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = FALSE;
    return TRUE;
}

BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = TRUE;
    return TRUE;
}
