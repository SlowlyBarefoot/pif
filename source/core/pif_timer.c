// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_timer.h"

// Timer control helpers for one-shot, repeat, and PWM timer modes.

// The tick handler passes over a stopped timer, so it cannot read what is written while the timer
// is held. On an 8-bit target a 32-bit store takes several instructions.
static BOOL _hold(PifTimer* p_owner)
{
	if (p_owner->_step != TS_RUNNING) return FALSE;
	p_owner->_step = TS_STOP;
	return TRUE;
}

static void _release(PifTimer* p_owner, BOOL held)
{
	// A one-shot timer that expired between the check in _hold() and the stop has nothing left to
	// count and stays stopped. A running timer always has.
	if (held && p_owner->__current) p_owner->_step = TS_RUNNING;
}

BOOL pifTimer_Start(PifTimer* p_owner, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    // A timer marked for removal stays marked, and no callback runs for it any more.
    if (p_owner->_step == TS_REMOVE) return TRUE;

    if (p_owner->__event) {
    	p_owner->__event = FALSE;
		if (p_owner->__evt_finish) (*p_owner->__evt_finish)(p_owner->__p_finish_issuer);
    }

    // The tick handler passes over a stopped timer, so it cannot see the fields half written. On an
    // 8-bit target a 32-bit store takes several instructions.
    p_owner->_step = TS_STOP;
    p_owner->_target = target;
    p_owner->__current = target;

    if (p_owner->_type == TT_PWM) {
    	// Duty is set explicitly after start.
    	p_owner->__pwm_duty = 0;
    	p_owner->__pwm_ratio = 0;
    }
    p_owner->_step = TS_RUNNING;
    return TRUE;
}

void pifTimer_Stop(PifTimer* p_owner)
{
	// Stopped first, so that the tick handler leaves __current alone while it is cleared.
	if (p_owner->_step != TS_REMOVE) p_owner->_step = TS_STOP;
	p_owner->__current = 0;
	if (p_owner->_type == TT_PWM) {
		// Ensure PWM output is turned off on stop.
		if (p_owner->act_pwm) (*p_owner->act_pwm)(OFF);
	}
}

void pifTimer_Reset(PifTimer* p_owner)
{
	if (p_owner->_step == TS_REMOVE) return;

	p_owner->_step = TS_STOP;
	p_owner->__current = p_owner->_target;
	p_owner->_step = TS_RUNNING;
}

BOOL pifTimer_SetTarget(PifTimer* p_owner, uint32_t target)
{
	if (!target) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	BOOL held = _hold(p_owner);
	p_owner->_target = target;
	// The on-time is kept in ticks, so it follows the period to keep the ratio.
	if (p_owner->_type == TT_PWM) {
		p_owner->__pwm_duty = target * p_owner->__pwm_ratio / PIF_PWM_MAX_DUTY;
	}
	_release(p_owner, held);
	return TRUE;
}

void pifTimer_SetPwmDuty(PifTimer* p_owner, uint16_t duty)
{
	uint32_t target = p_owner->_target;
	uint32_t on_time;
	BOOL held;

	// More than the full period would never meet the count and leave the output off.
	if (duty > PIF_PWM_MAX_DUTY) duty = PIF_PWM_MAX_DUTY;
	p_owner->__pwm_ratio = duty;

	// Convert duty ratio to an absolute on-time based on target period.
	on_time = target * duty / PIF_PWM_MAX_DUTY;
	held = _hold(p_owner);
	p_owner->__pwm_duty = on_time;
	_release(p_owner, held);

	if (on_time == target) {
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
	else return p_owner->_target - p_owner->__current;
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
