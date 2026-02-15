#include "core/pif_pid_control.h"

// PID controller initialization and output calculation.

void pifPidControl_Init(PifPidControl *p_owner, float kp, float ki, float kd, float max_integration)
{
	p_owner->kp = kp;
	p_owner->ki = ki;
	p_owner->kd = kd;
	p_owner->max_integration = max_integration;
	p_owner->err_sum = 0;
	p_owner->err_prev = 0;
}

float pifPidControl_Calcurate(PifPidControl *p_owner, float err)
{
	float up;			// Variable: Proportional output
	float ui;			// Variable: Integral output
	float ud;			// Variable: Derivative output
	float ed;
	float out;   		// Output: PID output

	// Compute the error sum
	p_owner->err_sum += err;

	// Compute the proportional output
	up = p_owner->kp * err;

	// Compute the integral output
	ui = p_owner->ki * p_owner->err_sum;
	if (ui > p_owner->max_integration) 					ui = p_owner->max_integration;
	else if (ui < (-1.0f * p_owner->max_integration))	ui = -1.0f * p_owner->max_integration;

	// Compute the derivative output
	ed = err - p_owner->err_prev;
	ud = ((err > 0 && ed > 0) || (err < 0 && ed < 0)) ? p_owner->kd * ed : 0;

	// Compute the pre-saturated output
	out = up + ui + ud;

	p_owner->err_prev = err;

	return out;
}
