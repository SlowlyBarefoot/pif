// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_pulse.h"

// Pulse input abstraction with edge tracking and optional signal collection.

/**
 * @brief Copies the last falling-edge slot and the one `back` slots before it as one consistent set.
 *        pifPulse_sigEdge changes __seq after it writes, so a copy that an edge interrupted is taken again.
 * @param p_owner Pointer to the pulse instance.
 * @param back How many slots before the last one p_back is, below PIF_PULSE_DATA_SIZE.
 * @param p_last Receives the last slot.
 * @param p_back Receives the earlier slot.
 * @param p_has_rising Receives whether the last slot holds a rising edge.
 * @return Number of falling edges kept.
 */
static uint8_t _readEdges(PifPulse* p_owner, uint8_t back, PifPulseData* p_last, PifPulseData* p_back,
		BOOL* p_has_rising)
{
	uint8_t seq, count, last, prev;

	do {
		seq = p_owner->__seq;
		count = p_owner->__count;
		last = p_owner->__last_ptr;
		prev = (last + PIF_PULSE_DATA_SIZE - back) & PIF_PULSE_DATA_MASK;
		p_last->rising = p_owner->__data[last].rising;
		p_last->falling = p_owner->__data[last].falling;
		p_back->rising = p_owner->__data[prev].rising;
		p_back->falling = p_owner->__data[prev].falling;
		*p_has_rising = p_owner->__last_has_rising;
	} while (seq != p_owner->__seq);
	return count;
}

/**
 * @brief Tells whether no falling edge has come for longer than the timeout.
 * @param p_owner Pointer to the pulse instance.
 * @param last_falling Time of the last falling edge.
 * @return TRUE if the timeout is set and has passed.
 */
static BOOL _isTimeout(PifPulse* p_owner, uint32_t last_falling)
{
	if (!p_owner->__timeout) return FALSE;
	return (*pif_act_timer1us)() - last_falling > p_owner->__timeout;
}

/**
 * @brief Tells whether a measurement is inside its valid range.
 * @param p_owner Pointer to the pulse instance.
 * @param index 0 for the period, 1 for the low width, 2 for the high width.
 * @param value Measured value.
 * @return TRUE if no range is set or value is inside it.
 */
static BOOL _isInRange(PifPulse* p_owner, int index, uint32_t value)
{
	if (!p_owner->__valid_range[index].check) return TRUE;
	return value >= p_owner->__valid_range[index].min && value <= p_owner->__valid_range[index].max;
}

/**
 * @brief Measures one value without the valid range.
 * @param p_owner Pointer to the pulse instance.
 * @param value Measurement to take.
 * @param p_value Receives the measured value with PULSE_R_OK.
 * @return PULSE_R_OK, PULSE_R_NO_DATA or PULSE_R_TIMEOUT.
 */
static PifPulseResult _measure(PifPulse* p_owner, PifPulseValue value, uint32_t* p_value)
{
	PifPulseData last, prev;
	BOOL has_rising;
	uint8_t count;

	switch (value) {
	case PULSE_V_PERIOD:
		if (_readEdges(p_owner, 1, &last, &prev, &has_rising) < 2) return PULSE_R_NO_DATA;
		*p_value = last.falling - prev.falling;
		break;

	case PULSE_V_AVERAGE_PERIOD:
		// The slot before the oldest is read as well; a count below 2 discards the result.
		do {
			count = p_owner->__count;
			if (count < 2) return PULSE_R_NO_DATA;
		} while (_readEdges(p_owner, count - 1, &last, &prev, &has_rising) != count);
		*p_value = (last.falling - prev.falling) / (count - 1);
		break;

	case PULSE_V_LOW_WIDTH:
		if (_readEdges(p_owner, 1, &last, &prev, &has_rising) < 2 || !has_rising) return PULSE_R_NO_DATA;
		*p_value = last.rising - prev.falling;
		break;

	default:
		if (_readEdges(p_owner, 1, &last, &prev, &has_rising) < 1 || !has_rising) return PULSE_R_NO_DATA;
		*p_value = last.falling - last.rising;
		break;
	}
	if (_isTimeout(p_owner, last.falling)) return PULSE_R_TIMEOUT;
	return PULSE_R_OK;
}

BOOL pifPulse_Init(PifPulse* p_owner, PifId id)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    return TRUE;
}

void pifPulse_Clear(PifPulse* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifPulse_ResetCsFlag(p_owner, PULSE_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	if (measure_mode == 0 || measure_mode > 0x0F) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->_measure_mode |= measure_mode;
	return TRUE;
}

void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	p_owner->_measure_mode &= ~measure_mode;
}

BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint32_t min, uint32_t max)
{
	int index = -1;

	switch (measure_mode) {
	case PULSE_PMM_PERIOD:
		index = 0;
		break;

	case PULSE_PMM_LOW_WIDTH:
		index = 1;
		break;

	case PULSE_PMM_HIGH_WIDTH:
		index = 2;
		break;
	}
	if (index < 0 || min > max) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->__valid_range[index].check = TRUE;
	p_owner->__valid_range[index].min = min;
	p_owner->__valid_range[index].max = max;
	return TRUE;
}

void pifPulse_SetTimeout(PifPulse* p_owner, uint32_t timeout_us)
{
	p_owner->__timeout = timeout_us;
}

void pifPulse_ResetMeasureValue(PifPulse* p_owner)
{
	p_owner->falling_count = 0UL;

	memset((void*)p_owner->__data, 0, sizeof(p_owner->__data));
	p_owner->__ptr = 0;
	p_owner->__last_ptr = 0;
	p_owner->__count = 0;
	p_owner->__rising_pending = FALSE;
	p_owner->__last_has_rising = FALSE;
	p_owner->__last_edge_time = 0;
	p_owner->__seq++;
}

PifPulseResult pifPulse_Read(PifPulse* p_owner, PifPulseValue value, uint32_t* p_value)
{
	static const uint8_t kMode[] = { PULSE_PMM_PERIOD, PULSE_PMM_PERIOD, PULSE_PMM_LOW_WIDTH, PULSE_PMM_HIGH_WIDTH };
	static const uint8_t kRange[] = { 0, 0, 1, 2 };
	PifPulseResult result;
	uint32_t measured = 0;

	if (value > PULSE_V_HIGH_WIDTH) {
		pif_error = E_INVALID_PARAM;
		result = PULSE_R_DISABLED;
	}
	else if (!(p_owner->_measure_mode & kMode[value])) {
		result = PULSE_R_DISABLED;
	}
	else {
		result = _measure(p_owner, value, &measured);
		if (result == PULSE_R_OK && !_isInRange(p_owner, kRange[value], measured)) result = PULSE_R_OUT_OF_RANGE;
		if (result != PULSE_R_OK && result != PULSE_R_OUT_OF_RANGE) measured = 0;
	}
	if (p_value) *p_value = measured;
	return result;
}

/**
 * @brief Reads a measurement as a getter does.
 * @param p_owner Pointer to the pulse instance.
 * @param value Measurement to read.
 * @return The value with PULSE_R_OK, otherwise 0.
 */
static uint32_t _get(PifPulse* p_owner, PifPulseValue value)
{
	uint32_t measured;

	return pifPulse_Read(p_owner, value, &measured) == PULSE_R_OK ? measured : 0;
}

uint32_t pifPulse_GetPeriod(PifPulse* p_owner)
{
	return _get(p_owner, PULSE_V_PERIOD);
}

uint32_t pifPulse_GetAveragePeriod(PifPulse* p_owner)
{
	return _get(p_owner, PULSE_V_AVERAGE_PERIOD);
}

uint32_t pifPulse_GetLowWidth(PifPulse* p_owner)
{
	return _get(p_owner, PULSE_V_LOW_WIDTH);
}

uint32_t pifPulse_GetHighWidth(PifPulse* p_owner)
{
	return _get(p_owner, PULSE_V_HIGH_WIDTH);
}

uint32_t pifPulse_GetLastEdgeTime(PifPulse* p_owner)
{
	uint32_t time;
	uint8_t seq;

	do {
		seq = p_owner->__seq;
		time = p_owner->__last_edge_time;
	} while (seq != p_owner->__seq);
	return time;
}

BOOL pifPulse_sigEdge(PifPulse* p_owner, PifPulseState state, uint32_t time_us)
{
	BOOL rtn = FALSE;
	uint8_t ptr = p_owner->__ptr;

	if (state == PS_RISING_EDGE) {
		p_owner->__data[ptr].rising = time_us;
		p_owner->__rising_pending = TRUE;
	}
	else {
		// After a timeout the old edges no longer belong to this signal.
		if (p_owner->__timeout && p_owner->__count &&
				time_us - p_owner->__data[p_owner->__last_ptr].falling > p_owner->__timeout) {
			p_owner->__count = 0;
		}
		p_owner->__data[ptr].falling = time_us;
		p_owner->__last_has_rising = p_owner->__rising_pending;
		p_owner->__rising_pending = FALSE;
		if (p_owner->_measure_mode & PULSE_PMM_COUNT) {
			p_owner->falling_count++;
		}
		p_owner->__last_ptr = ptr;
		p_owner->__ptr = (ptr + 1) & PIF_PULSE_DATA_MASK;
		if (p_owner->__count < PIF_PULSE_DATA_SIZE) p_owner->__count++;
		rtn = TRUE;
	}
	p_owner->__last_edge_time = time_us;
	p_owner->__seq++;

	if (rtn && p_owner->__evt_edge) {
		(*p_owner->__evt_edge)(state, p_owner->__p_issuer);
	}

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[PULSE_CSF_STATE_IDX], state == PS_RISING_EDGE);
#endif

	return rtn;
}

void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, PifIssuerP p_issuer)
{
	p_owner->__evt_edge = evt_edge;
	p_owner->__p_issuer = p_issuer;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	if (flag & PULSE_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[PULSE_CSF_STATE_IDX], "PL", p_owner->_id, CSVT_WIRE, 1,
				PS_LOW_LEVEL)) return FALSE;
	}
	return TRUE;
}

void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	if (flag & PULSE_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[PULSE_CSF_STATE_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
