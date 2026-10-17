// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_pwm.h"


/**
 * @brief Measures the pulse ended by a falling edge and closes the frame on the last channel.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param channel Zero-based channel index.
 * @param time_us Edge timestamp in microseconds.
 * @return Decoded pulse width for the channel, or 0 when no valid update is produced.
 */
static uint16_t _processFalling(PifRcPwm* p_owner, uint8_t channel, uint32_t time_us)
{
	PifRc* p_parent = &p_owner->parent;
	uint16_t rtn = 0;
	uint32_t rising = p_owner->__p_rising[channel];
	uint32_t width;

	if (rising) {
		p_owner->__p_rising[channel] = 0;

		// Compared before narrowing, so a gap of 64ms or more cannot wrap into the range.
		width = time_us - rising;
		if (p_owner->__valid_range_max &&
				(width < p_owner->__valid_range_min || width > p_owner->__valid_range_max)) {
			p_owner->__frame_error = TRUE;
		}
		else if (width > 0xFFFF) {
			p_owner->__frame_error = TRUE;
		}
		else {
			rtn = (uint16_t)width;
			p_owner->__p_channel[channel] = rtn;
		}
	}
	else {
		// The rising edge was missed.
		p_owner->__frame_error = TRUE;
	}

	if (channel + 1 == p_owner->__max_channel) {
		if (p_owner->__frame_error) {
			p_parent->_error_frames++;
		}
		else {
			p_parent->_good_frames++;
			p_parent->_last_frame_time = pif_cumulative_timer1ms;
			if (p_parent->__evt_receive) {
				(*p_parent->__evt_receive)(p_parent, p_owner->__p_channel, p_parent->__p_issuer);
			}
		}
		p_owner->__frame_error = FALSE;
	}
	return rtn;
}


BOOL pifRcPwm_Init(PifRcPwm* p_owner, PifId id, uint8_t channel_count)
{
    if (!p_owner || !channel_count) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRcPwm));

	// One block holds the rising times followed by the channel values.
	p_owner->__p_rising = calloc(channel_count, sizeof(uint32_t) + sizeof(uint16_t));
    if (!p_owner->__p_rising) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}
	p_owner->__p_channel = (uint16_t*)(p_owner->__p_rising + channel_count);

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._channel_count = channel_count;
	p_owner->parent._failsafe = FALSE;
	p_owner->parent._max_frame_period = PIF_RC_PWM_DEFAULT_TIMEOUT_MS;
	p_owner->__max_channel = channel_count;
    return TRUE;
}

void pifRcPwm_Clear(PifRcPwm* p_owner)
{
	if (p_owner->__p_rising) {
		free(p_owner->__p_rising);
		p_owner->__p_rising = NULL;
		p_owner->__p_channel = NULL;
	}
}

BOOL pifRcPwm_SetValidRange(PifRcPwm* p_owner, uint16_t min, uint16_t max)
{
	if (min > max) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__valid_range_min = min;
	p_owner->__valid_range_max = max;
	return TRUE;
}

void pifRcPwm_SetTimeout(PifRcPwm* p_owner, uint16_t timeout_ms)
{
	p_owner->parent._max_frame_period = timeout_ms;
}

void pifRcPwm_ResetMeasureValue(PifRcPwm* p_owner)
{
	int i;

	for (i = 0; i < p_owner->__max_channel; i++) {
		p_owner->__p_rising[i] = 0;
	}
	p_owner->__frame_error = FALSE;
}

uint16_t pifRcPwm_sigEdge(PifRcPwm* p_owner, uint8_t channel, PifPulseState state, uint32_t time_us)
{
	if (channel >= p_owner->__max_channel) return 0;

	if (state == PS_RISING_EDGE) {
		// 0 marks no pending rising edge, so a rising edge at time 0 is kept as 1us.
		p_owner->__p_rising[channel] = time_us ? time_us : 1;
		return 0;
	}
	return _processFalling(p_owner, channel, time_us);
}
