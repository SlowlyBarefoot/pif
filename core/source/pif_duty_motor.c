#include "pif_log.h"
#include "pif_duty_motor.h"


void pifDutyMotor_Control(PifDutyMotor* p_owner)
{
    if (p_owner->__error) {
        if (p_owner->_state < MS_BREAK) {
			(*p_owner->__act_set_duty)(0);
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
    	(*p_owner->__act_operate_break)(0);
    }
}

PifDutyMotor* pifDutyMotor_Create(PifId id, PifPulse* p_timer, uint16_t max_duty)
{
    PifDutyMotor* p_owner = malloc(sizeof(PifDutyMotor));
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (!pifDutyMotor_Init(p_owner, id, p_timer, max_duty)) {
		pifDutyMotor_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifDutyMotor_Destroy(PifDutyMotor** pp_owner)
{
	if (*pp_owner) {
		pifDutyMotor_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifPulse* p_timer, uint16_t max_duty)
{
    if (!p_owner || !p_timer || !max_duty) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDutyMotor));

    p_owner->_p_timer = p_timer;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_max_duty = max_duty;
    return TRUE;
}

void pifDutyMotor_Clear(PifDutyMotor* p_owner)
{
	if (p_owner->__p_timer_delay) {
		pifPulse_RemoveItem(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	if (p_owner->__p_timer_break) {
		pifPulse_RemoveItem(p_owner->__p_timer_break);
		p_owner->__p_timer_break = NULL;
	}
}

void pifDutyMotor_AttachAction(PifDutyMotor* p_owner, PifActDutyMotorSetDuty act_set_duty,
		PifActDutyMotorSetDirection act_set_direction, PifActDutyMotorOperateBreak act_operate_break)
{
    p_owner->__act_set_duty = act_set_duty;
    p_owner->__act_set_direction = act_set_direction;
    p_owner->__act_operate_break = act_operate_break;
}

void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction)
{
	p_owner->_direction = direction;

	if (p_owner->__act_set_direction) (*p_owner->__act_set_direction)(p_owner->_direction);
}

void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    if (duty > p_owner->_max_duty) duty = p_owner->_max_duty;
	p_owner->_current_duty = duty;

	(*p_owner->__act_set_duty)(p_owner->_current_duty);
}

BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time)
{
	if (!p_owner->__p_timer_break) {
		p_owner->__p_timer_break = pifPulse_AddItem(p_owner->_p_timer, PT_ONCE);
	}
	if (p_owner->__p_timer_break) {
		pifPulse_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
		if (pifPulse_StartItem(p_owner->__p_timer_break, operating_time)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty)
{
    if (!p_owner->__act_set_duty || !p_owner->__act_set_direction) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    (*p_owner->__act_set_direction)(p_owner->_direction);

   	p_owner->_current_duty = duty;

    (*p_owner->__act_set_duty)(p_owner->_current_duty);
	return TRUE;
}

void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time)
{
    p_owner->_current_duty = 0;

    (*p_owner->__act_set_duty)(p_owner->_current_duty);

    if (break_time && p_owner->__act_operate_break) {
	    if (!p_owner->__p_timer_break) {
	    	p_owner->__p_timer_break = pifPulse_AddItem(p_owner->_p_timer, PT_ONCE);
	    }
	    if (p_owner->__p_timer_break) {
	    	pifPulse_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
			if (pifPulse_StartItem(p_owner->__p_timer_break, break_time)) {
		    	(*p_owner->__act_operate_break)(1);
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
