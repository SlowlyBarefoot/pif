// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_pwm_gen.h"


// Software PWM output on a one-shot timer.
//
// The finish callback runs in the tick interrupt and starts the timer again for the next phase,
// so the tick handler owns the phases while the timer runs. The main context takes them over only
// while the timer is stopped, and otherwise hands new ones over through __update.

// Begins a period with the phases waiting for it. Runs in the tick interrupt, or in the main
// context while the timer is stopped.
static void _beginPeriod(PifPwmGen* p_owner)
{
	if (p_owner->__update) {
		p_owner->__update = FALSE;
		p_owner->__on_time = p_owner->__next_on_time;
		p_owner->__off_time = p_owner->__next_off_time;
	}

	// A duty of 0 or the maximum holds the output, and the timer stays stopped.
	if (!p_owner->__on_time) {
		(*p_owner->__act_pwm)(OFF);
		return;
	}
	(*p_owner->__act_pwm)(ON);
	if (!p_owner->__off_time) return;

	p_owner->__phase_on = TRUE;
	pifTimer_Start(p_owner->__p_timer, p_owner->__on_time);
}

static void _evtFinish(PifIssuerP p_issuer)
{
	PifPwmGen* p_owner = (PifPwmGen*)p_issuer;

	if (p_owner->__phase_on) {
		p_owner->__phase_on = FALSE;
		(*p_owner->__act_pwm)(OFF);
		pifTimer_Start(p_owner->__p_timer, p_owner->__off_time);
	}
	else {
		_beginPeriod(p_owner);
	}
}

static uint32_t _onTime(PifPwmGen* p_owner)
{
	return p_owner->_period * p_owner->_duty / PIF_PWM_GEN_MAX_DUTY;
}

static void _update(PifPwmGen* p_owner)
{
	uint32_t on_time;

	if (!p_owner->_running) return;

	on_time = _onTime(p_owner);
	p_owner->__update = FALSE;
	p_owner->__next_on_time = on_time;
	p_owner->__next_off_time = p_owner->_period - on_time;
	p_owner->__update = TRUE;

	// A held output has no period under way to wait for. Once the timer is stopped the tick
	// handler leaves the phases alone.
	if (p_owner->__p_timer->_step != TS_RUNNING) _beginPeriod(p_owner);
}

BOOL pifPwmGen_Init(PifPwmGen* p_owner, PifId id, PifTimerManager* p_timer_manager,
		PifActPwmGen act_pwm)
{
	if (!p_owner || !p_timer_manager || !act_pwm) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifPwmGen));

	p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
	if (!p_owner->__p_timer) return FALSE;
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtFinish, p_owner, TRUE);

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->__act_pwm = act_pwm;
	return TRUE;
}

void pifPwmGen_Clear(PifPwmGen* p_owner)
{
	if (!p_owner->__p_timer) return;

	pifPwmGen_Stop(p_owner);
	pifTimerManager_Remove(p_owner->__p_timer);
	p_owner->__p_timer = NULL;
}

BOOL pifPwmGen_Start(PifPwmGen* p_owner, uint32_t period)
{
	uint32_t on_time;

	if (!period) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	pifTimer_Stop(p_owner->__p_timer);
	p_owner->_period = period;
	on_time = _onTime(p_owner);
	p_owner->__on_time = on_time;
	p_owner->__off_time = period - on_time;
	p_owner->__update = FALSE;
	p_owner->_running = TRUE;
	_beginPeriod(p_owner);
	return TRUE;
}

void pifPwmGen_Stop(PifPwmGen* p_owner)
{
	pifTimer_Stop(p_owner->__p_timer);
	p_owner->_running = FALSE;
	p_owner->__update = FALSE;
	(*p_owner->__act_pwm)(OFF);
}

BOOL pifPwmGen_SetPeriod(PifPwmGen* p_owner, uint32_t period)
{
	if (!period) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_period = period;
	_update(p_owner);
	return TRUE;
}

void pifPwmGen_SetDuty(PifPwmGen* p_owner, uint16_t duty)
{
	// More than the full period would leave a negative off-time.
	if (duty > PIF_PWM_GEN_MAX_DUTY) duty = PIF_PWM_GEN_MAX_DUTY;
	p_owner->_duty = duty;
	_update(p_owner);
}
