// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_ppm.h"


#define PPM_STEP_WAIT_SYNC		0
#define PPM_STEP_LEARN			1
#define PPM_STEP_DECODE			2


/**
 * @brief Closes the frame ended by a sync gap, learning the channel count or checking the frame against it.
 * @param p_owner Pointer to the receiver instance to operate on.
 */
static void _processSync(PifRcPpm* p_owner)
{
	PifRc* p_parent = &p_owner->parent;
	uint8_t count;

	// Pulses past the last channel are not counted, so a longer frame counts as a full one.
	count = p_owner->_channel + 1 > p_owner->__max_channel ? p_owner->__max_channel : p_owner->_channel + 1;

	switch (p_owner->__process_step) {
	case PPM_STEP_WAIT_SYNC:
		p_owner->__process_step = PPM_STEP_LEARN;
		break;

	case PPM_STEP_LEARN:
		// One frame broken by a glitch must not set the channel count, so two in a row have to agree.
		if (count && count == p_owner->__learn_count) {
			p_parent->_channel_count = count;
			p_owner->__process_step = PPM_STEP_DECODE;
		}
		p_owner->__learn_count = count;
		break;

	case PPM_STEP_DECODE:
		if (p_owner->__frame_error || count != p_parent->_channel_count) p_parent->_error_frames++;
		break;
	}

	p_owner->_channel = -1;
	p_owner->__frame_error = FALSE;
}

/**
 * @brief Processes one channel pulse and delivers the frame when its last channel arrives.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param diff Measured pulse interval in microseconds.
 * @return Decoded pulse width for the current channel, or 0 when no value is produced.
 */
static uint16_t _processPulse(PifRcPpm* p_owner, uint16_t diff)
{
	PifRc* p_parent = &p_owner->parent;
	int8_t index;

	if (p_owner->__process_step == PPM_STEP_WAIT_SYNC) return 0;

	// Stop one past the last channel so that a missing sync cannot run the index out of range.
	if (p_owner->_channel >= p_owner->__max_channel) return 0;
	index = ++p_owner->_channel;
	if (index >= p_owner->__max_channel) return 0;

	if (p_owner->__process_step != PPM_STEP_DECODE) return 0;

	if (index >= p_parent->_channel_count) {
		p_owner->__frame_error = TRUE;
		return 0;
	}

	if (p_owner->__valid_range_max) {
		if (diff < p_owner->__valid_range_min || diff > p_owner->__valid_range_max) {
			p_owner->__frame_error = TRUE;
			return 0;
		}
	}
	p_owner->__p_work[index] = diff;

	if (index + 1 == p_parent->_channel_count && !p_owner->__frame_error) {
		memcpy(p_owner->__p_channel, p_owner->__p_work, p_parent->_channel_count * sizeof(uint16_t));
		p_parent->_good_frames++;
		p_parent->_last_frame_time = pif_cumulative_timer1ms;
		if (p_parent->__evt_receive) {
			(*p_parent->__evt_receive)(p_parent, p_owner->__p_channel, p_parent->__p_issuer);
		}
	}
	return diff;
}


BOOL pifRcPpm_Init(PifRcPpm* p_owner, PifId id, uint8_t channel_count, uint16_t threshold_1us)
{
    if (!p_owner || !channel_count || channel_count > PIF_RC_PPM_MAX_CHANNEL) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRcPpm));

    p_owner->__p_channel = calloc(2 * channel_count, sizeof(uint16_t));
    if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
        return FALSE;
	}
	p_owner->__p_work = p_owner->__p_channel + channel_count;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._failsafe = FALSE;
	p_owner->parent._max_frame_period = PIF_RC_PPM_DEFAULT_TIMEOUT_MS;
	p_owner->_channel = -1;
	p_owner->__max_channel = channel_count;
    p_owner->__threshold_1us = threshold_1us;
    return TRUE;
}

void pifRcPpm_Clear(PifRcPpm* p_owner)
{
	if (p_owner->__p_channel) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
		p_owner->__p_work = NULL;
	}
}

BOOL pifRcPpm_SetValidRange(PifRcPpm* p_owner, uint16_t min, uint16_t max)
{
	if (min > max) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__valid_range_min = min;
	p_owner->__valid_range_max = max;
	return TRUE;
}

void pifRcPpm_SetTimeout(PifRcPpm* p_owner, uint16_t timeout_ms)
{
	p_owner->parent._max_frame_period = timeout_ms;
}

void pifRcPpm_ResetMeasureValue(PifRcPpm* p_owner)
{
	p_owner->parent._channel_count = 0;
	p_owner->_channel = -1;
	p_owner->__time_valid = FALSE;
	p_owner->__frame_error = FALSE;
	p_owner->__process_step = PPM_STEP_WAIT_SYNC;
	p_owner->__learn_count = 0;
}

uint16_t pifRcPpm_sigTick(PifRcPpm* p_owner, uint32_t time_us)
{
    uint32_t diff;

	// The first edge has no previous one to measure from.
	if (!p_owner->__time_valid) {
		p_owner->__time_valid = TRUE;
		p_owner->__last_time = time_us;
		return 0;
	}

    diff = time_us - p_owner->__last_time;
	p_owner->__last_time = time_us;

	if (diff >= p_owner->__threshold_1us) {
		_processSync(p_owner);
		return 0;
	}
	return _processPulse(p_owner, (uint16_t)diff);
}
