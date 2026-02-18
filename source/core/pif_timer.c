#include "core/pif_timer.h"

// Timer control helpers for one-shot, repeat, and PWM timer modes.

BOOL pifTimer_Start(PifTimer* p_owner, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (p_owner->__event) {
    	p_owner->__event = FALSE;
		if (p_owner->__evt_finish) (*p_owner->__evt_finish)(p_owner->__p_finish_issuer);
        if (p_owner->__p_task) p_owner->__p_task->__timer_trigger--;
    }

    if (p_owner->_step == TS_STOP) {
    	p_owner->_step = TS_RUNNING;
    }
    p_owner->target = target;
    p_owner->__current = target;

    if (p_owner->_type == TT_PWM) {
    	// Duty is set explicitly after start.
    	p_owner->__pwm_duty = 0;
    }
    return TRUE;
}

void pifTimer_Stop(PifTimer* p_owner)
{
	p_owner->__current = 0;
	p_owner->_step = TS_STOP;
	if (p_owner->_type == TT_PWM) {
		// Ensure PWM output is turned off on stop.
		if (p_owner->act_pwm) (*p_owner->act_pwm)(OFF);
	}
}

void pifTimer_Reset(PifTimer* p_owner)
{
	p_owner->__current = p_owner->target;
	p_owner->_step = TS_RUNNING;
}

void pifTimer_SetPwmDuty(PifTimer* p_owner, uint16_t duty)
{
	// Convert duty ratio to an absolute on-time based on target period.
	p_owner->__pwm_duty = p_owner->target * duty / PIF_PWM_MAX_DUTY;
	if (p_owner->__pwm_duty == p_owner->target) {
		if (p_owner->act_pwm) (*p_owner->act_pwm)(ON);
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

void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer)
{
	p_owner->__evt_finish = evt_finish;
	p_owner->__p_finish_issuer = p_issuer;
	p_owner->__event_into_int = FALSE;
}

void pifTimer_AttachEvtIntFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer)
{
	p_owner->__evt_finish = evt_finish;
	p_owner->__p_finish_issuer = p_issuer;
	// Mark callback as safe to run in interrupt context.
	p_owner->__event_into_int = TRUE;
}
