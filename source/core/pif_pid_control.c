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

float pifPidControl_Calculate(PifPidControl *p_owner, float err)
{
	float up;			// Variable: Proportional output
	float ui;			// Variable: Integral output
	float ud;			// Variable: Derivative output
	float ed;
    float max_sum;
	float out;   		// Output: PID output

	// Compute the error sum
	p_owner->err_sum += err;
    if (p_owner->ki != 0.0f) {
        max_sum = p_owner->max_integration / p_owner->ki;
    	if (p_owner->err_sum > max_sum) p_owner->err_sum = max_sum;
        else if (p_owner->err_sum < -max_sum) p_owner->err_sum = -max_sum;
    }

	// Compute the proportional output
	up = p_owner->kp * err;

	// Compute the integral output
	ui = p_owner->ki * p_owner->err_sum;

	// Compute the derivative output
	ed = err - p_owner->err_prev;
	ud = p_owner->kd * ed;

	// Compute the pre-saturated output
	out = up + ui + ud;

	p_owner->err_prev = err;

	return out;
}
