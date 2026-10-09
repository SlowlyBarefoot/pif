// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_pid_control.h"

// PID controller initialization and output calculation.

// Cutoff of each of n identical first-order stages that put the whole filter 3 dB down at the
// requested cutoff: 1 / sqrt(2^(1/n) - 1).
static const float c_stage_scale[2] = { 1.0f, 1.55377397f };

/**
 * @fn _lowPass
 * @brief Runs a sample through a first-order low-pass stage: y += k (x - y), k = w / (w + 1),
 *        w = 2 pi fc dt.
 * @param p_state Stage output.
 * @param input Sample.
 * @param cutoff_hz Cutoff of the stage.
 * @param dt Sample period in seconds.
 * @return Stage output.
 */
static float _lowPass(float* p_state, float input, float cutoff_hz, float dt)
{
	float w = 2.0f * PIF_PI * cutoff_hz * dt;

	*p_state += w / (w + 1.0f) * (input - *p_state);
	return *p_state;
}

/**
 * @fn _limit
 * @param p_owner Pointer to the target object instance.
 * @param value Output before the limits.
 * @return Output within the limits, if set.
 */
static float _limit(PifPidControl *p_owner, float value)
{
	if (!p_owner->__limited) return value;
	if (value > p_owner->__out_max) return p_owner->__out_max;
	if (value < p_owner->__out_min) return p_owner->__out_min;
	return value;
}

void pifPidControl_Init(PifPidControl *p_owner, float kp, float ki, float kd, float max_integration)
{
	memset(p_owner, 0, sizeof(PifPidControl));
	p_owner->kp = kp;
	p_owner->ki = ki;
	p_owner->kd = kd;
	p_owner->max_integration = max_integration;
	p_owner->err_sum = 0;
	p_owner->err_prev = 0;
	p_owner->__anti_windup = PID_AW_NONE;
}

void pifPidControl_Clear(PifPidControl *p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifPidControl_ResetCsFlag(p_owner, PID_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
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

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_ERROR_IDX], err);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_P_TERM_IDX], up);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_I_TERM_IDX], ui);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_D_TERM_IDX], ud);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_OUTPUT_IDX], out);
#endif

	return out;
}

void pifPidControl_SetDerivativeOnMeasurement(PifPidControl *p_owner, BOOL enable)
{
	p_owner->__d_on_measurement = enable;
}

BOOL pifPidControl_SetDtermFilter(PifPidControl *p_owner, float cutoff_hz, uint8_t order)
{
	if (cutoff_hz < 0.0f || (cutoff_hz > 0.0f && (order < 1 || order > 2))) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__d_cutoff_hz = cutoff_hz;
	p_owner->__d_order = cutoff_hz > 0.0f ? order : 0;
	return TRUE;
}

BOOL pifPidControl_SetOutputLimit(PifPidControl *p_owner, float min, float max)
{
	if (min >= max) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__out_min = min;
	p_owner->__out_max = max;
	p_owner->__limited = TRUE;
	return TRUE;
}

void pifPidControl_ClearOutputLimit(PifPidControl *p_owner)
{
	p_owner->__limited = FALSE;
}

BOOL pifPidControl_SetAntiWindup(PifPidControl *p_owner, PifPidAntiWindup mode, float tracking_gain)
{
	if (mode > PID_AW_BACK_CALCULATION || (mode == PID_AW_BACK_CALCULATION && tracking_gain <= 0.0f)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__anti_windup = mode;
	p_owner->__tracking_gain = tracking_gain;
	return TRUE;
}

void pifPidControl_SetFeedforward(PifPidControl *p_owner, float kf, float kfd, float cutoff_hz)
{
	p_owner->__kf = kf;
	p_owner->__kfd = kfd;
	p_owner->__f_cutoff_hz = cutoff_hz > 0.0f ? cutoff_hz : 0.0f;
}

void pifPidControl_Reset(PifPidControl *p_owner)
{
	p_owner->err_sum = 0.0f;
	p_owner->err_prev = 0.0f;
	p_owner->_p_term = p_owner->_i_term = p_owner->_d_term = p_owner->_f_term = 0.0f;
	p_owner->_output = 0.0f;
	p_owner->_saturated = FALSE;
	p_owner->__primed = FALSE;
	p_owner->__d_state[0] = p_owner->__d_state[1] = 0.0f;
	p_owner->__f_state = 0.0f;
}

float pifPidControl_Update(PifPidControl *p_owner, float setpoint, float measurement, float dt)
{
	float error = setpoint - measurement;
	float derivative = 0.0f, rate = 0.0f, i_term, unlimited, output, cutoff;
	BOOL step = p_owner->__primed && dt > 0.0f;
	uint8_t i;

	// P
	p_owner->_p_term = p_owner->kp * error;

	// D: on the measurement leaves out the setpoint change, which is what kicks on a step.
	if (step) {
		derivative = p_owner->__d_on_measurement ? -(measurement - p_owner->__prev_measurement) / dt
				: (error - p_owner->__prev_error) / dt;
		if (p_owner->__d_order) {
			cutoff = p_owner->__d_cutoff_hz * c_stage_scale[p_owner->__d_order - 1];
			for (i = 0; i < p_owner->__d_order; i++) {
				derivative = _lowPass(&p_owner->__d_state[i], derivative, cutoff, dt);
			}
		}
	}
	p_owner->_d_term = p_owner->kd * derivative;

	// Feedforward of the setpoint and of its rate.
	if (step && p_owner->__kfd != 0.0f) {
		rate = (setpoint - p_owner->__prev_setpoint) / dt;
		if (p_owner->__f_cutoff_hz > 0.0f) rate = _lowPass(&p_owner->__f_state, rate, p_owner->__f_cutoff_hz, dt);
	}
	p_owner->_f_term = p_owner->__kf * setpoint + p_owner->__kfd * rate;

	// I, with the anti-windup chosen.
	i_term = p_owner->_i_term;
	if (dt > 0.0f) i_term += p_owner->ki * error * dt;
	unlimited = p_owner->_p_term + i_term + p_owner->_d_term + p_owner->_f_term;
	output = _limit(p_owner, unlimited);
	if (output != unlimited) {
		switch (p_owner->__anti_windup) {
		case PID_AW_CLAMP:
			// Keep the old I term if integrating would push further into the limit.
			if ((unlimited > output && error > 0.0f) || (unlimited < output && error < 0.0f)) {
				i_term = p_owner->_i_term;
			}
			break;

		case PID_AW_BACK_CALCULATION:
			i_term += p_owner->__tracking_gain * (output - unlimited) * dt;
			break;

		default:
			break;
		}
	}
	if (p_owner->max_integration > 0.0f) {
		if (i_term > p_owner->max_integration) i_term = p_owner->max_integration;
		else if (i_term < -p_owner->max_integration) i_term = -p_owner->max_integration;
	}
	p_owner->_i_term = i_term;

	unlimited = p_owner->_p_term + i_term + p_owner->_d_term + p_owner->_f_term;
	output = _limit(p_owner, unlimited);
	p_owner->_saturated = output != unlimited;
	p_owner->_output = output;

	p_owner->__prev_error = error;
	p_owner->__prev_measurement = measurement;
	p_owner->__prev_setpoint = setpoint;
	p_owner->__primed = TRUE;

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_SETPOINT_IDX], setpoint);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_MEASUREMENT_IDX], measurement);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_ERROR_IDX], error);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_P_TERM_IDX], p_owner->_p_term);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_I_TERM_IDX], p_owner->_i_term);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_D_TERM_IDX], p_owner->_d_term);
	pifCollectSignal_PutReal(&p_owner->__cs[PID_CSF_OUTPUT_IDX], output);
#endif
	return output;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifPidControl_SetCsFlag(PifPidControl *p_owner, PifPidControlCsFlag flag, PifId id)
{
	static const char *c_name[PID_CSF_COUNT] = { "PIDSP", "PIDMV", "PIDE", "PIDO", "PIDP", "PIDI", "PIDD" };
	PifCollectSignalChannel *p_channel;
	int i;

	for (i = 0; i < PID_CSF_COUNT; i++) {
		if (!(flag & (1 << i))) continue;
		p_channel = &p_owner->__cs[i];
		// The last value is kept by the channel itself, so the value it starts with is its own.
		if (!pifCollectSignal_AddChannel(p_channel, c_name[i], id, CSVT_REAL, 0, p_channel->_value)) return FALSE;
	}
	return TRUE;
}

void pifPidControl_ResetCsFlag(PifPidControl *p_owner, PifPidControlCsFlag flag)
{
	int i;

	for (i = 0; i < PID_CSF_COUNT; i++) {
		if (flag & (1 << i)) pifCollectSignal_RemoveChannel(&p_owner->__cs[i]);
	}
}

#endif	// PIF_COLLECT_SIGNAL
