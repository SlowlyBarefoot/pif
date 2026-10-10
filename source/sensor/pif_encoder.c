// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_encoder.h"

// Quadrature encoder input with X1/X2/X4 position, speed and lost-step detection.

#define INVALID		2

// Step for each (previous state << 2 | state), with A in bit 1 and B in bit 0.
// A leading B goes 00 -> 10 -> 11 -> 01 -> 00.
static const int8_t kStep[16] = {
	0, -1, 1, INVALID,
	1, 0, INVALID, -1,
	-1, INVALID, 0, 1,
	INVALID, 1, -1, 0
};


/**
 * @brief Converts quarter steps to counts of the resolution, rounding down so that the count changes at
 *        the same place in both directions.
 * @param p_owner Pointer to the encoder instance.
 * @param raw Quarter steps.
 * @return Counts.
 */
static int32_t _toCounts(PifEncoder* p_owner, int32_t raw)
{
	int32_t div = 4 >> p_owner->_resolution;

	if (raw >= 0) return raw / div;
	return -((-(raw + 1)) / div) - 1;
}

/**
 * @brief Copies the quarter steps as one consistent value.
 * @param p_owner Pointer to the encoder instance.
 * @return Quarter steps.
 */
static int32_t _readRaw(PifEncoder* p_owner)
{
	int32_t raw;
	uint8_t seq;

	do {
		seq = p_owner->__seq;
		raw = p_owner->__raw;
	} while (seq != p_owner->__seq);
	return raw;
}

BOOL pifEncoder_Init(PifEncoder* p_owner, PifId id, PifEncoderResolution resolution)
{
	if (!p_owner || resolution > ENCODER_RES_X4) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifEncoder));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_resolution = resolution;
	return TRUE;
}

void pifEncoder_Clear(PifEncoder* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifEncoder_ResetCsFlag(p_owner, ENCODER_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifEncoder_SetReverse(PifEncoder* p_owner, BOOL reverse)
{
	p_owner->__reverse = reverse;
}

void pifEncoder_SetTimeout(PifEncoder* p_owner, uint32_t timeout_us)
{
	p_owner->__timeout = timeout_us;
}

void pifEncoder_SetPosition(PifEncoder* p_owner, int32_t position)
{
	p_owner->__offset = position - _toCounts(p_owner, _readRaw(p_owner));
}

int32_t pifEncoder_GetPosition(PifEncoder* p_owner)
{
	return _toCounts(p_owner, _readRaw(p_owner)) + p_owner->__offset;
}

int8_t pifEncoder_GetDirection(PifEncoder* p_owner)
{
	return p_owner->__direction;
}

uint32_t pifEncoder_GetErrorCount(PifEncoder* p_owner)
{
	uint32_t count;
	uint8_t seq;

	do {
		seq = p_owner->__seq;
		count = p_owner->__error_count;
	} while (seq != p_owner->__seq);
	return count;
}

PifEncoderResult pifEncoder_ReadSpeed(PifEncoder* p_owner, float* p_speed)
{
	uint32_t first, last, elapsed;
	uint8_t seq, count, intervals, ptr;
	int8_t direction;
	float period;

	if (p_speed) *p_speed = 0.0f;

	do {
		seq = p_owner->__seq;
		count = p_owner->__count;
		ptr = p_owner->__ptr;
		direction = p_owner->__direction;
		// Whole encoder cycles once there are enough, since the quarter steps of one are seldom even.
		intervals = count > 1 ? count - 1 : 0;
		if (intervals >= 4) intervals &= ~3;
		last = p_owner->__time[(ptr + PIF_ENCODER_DATA_MASK) & PIF_ENCODER_DATA_MASK];
		first = p_owner->__time[(ptr + PIF_ENCODER_DATA_MASK - intervals) & PIF_ENCODER_DATA_MASK];
	} while (seq != p_owner->__seq);

	if (!count) return ENCODER_R_NO_DATA;
	elapsed = (*pif_act_timer1us)() - last;
	if (p_owner->__timeout && elapsed > p_owner->__timeout) return ENCODER_R_TIMEOUT;
	if (!intervals) return ENCODER_R_NO_DATA;

	// A step later than the measured period shows that the encoder has slowed down to at most this.
	period = (float)(last - first) / intervals;
	if (elapsed > period) period = elapsed;
	if (period < 1.0f) period = 1.0f;

	if (p_speed) *p_speed = direction * 1000000.0f / period / (4 >> p_owner->_resolution);
	return ENCODER_R_OK;
}

float pifEncoder_GetSpeed(PifEncoder* p_owner)
{
	float speed;

	pifEncoder_ReadSpeed(p_owner, &speed);
	return speed;
}

BOOL pifEncoder_sigState(PifEncoder* p_owner, uint8_t state, uint32_t time_us)
{
	int8_t step;
	int32_t counts;
	BOOL changed = FALSE;

	state &= ENCODER_PHASE_A | ENCODER_PHASE_B;
	if (!p_owner->__has_state) {
		p_owner->__has_state = TRUE;
		step = 0;
	}
	else {
		step = kStep[(p_owner->__state << 2) | state];
	}
	p_owner->__state = state;

	if (step == INVALID) {
		// The step in between was lost, so its direction is unknown.
		p_owner->__error_count++;
		p_owner->__count = 0;
	}
	else if (step) {
		if (p_owner->__reverse) step = -step;
		counts = _toCounts(p_owner, p_owner->__raw);
		p_owner->__raw += step;
		changed = _toCounts(p_owner, p_owner->__raw) != counts;

		// The times of the other direction or from before a stop do not belong to this speed.
		if (step != p_owner->__direction || (p_owner->__timeout && p_owner->__count &&
				time_us - p_owner->__time[(p_owner->__ptr + PIF_ENCODER_DATA_MASK) & PIF_ENCODER_DATA_MASK] >
				p_owner->__timeout)) {
			p_owner->__count = 0;
		}
		p_owner->__direction = step;
		p_owner->__time[p_owner->__ptr] = time_us;
		p_owner->__ptr = (p_owner->__ptr + 1) & PIF_ENCODER_DATA_MASK;
		if (p_owner->__count < PIF_ENCODER_DATA_SIZE) p_owner->__count++;
	}
	p_owner->__seq++;

	if (changed && p_owner->__evt_step) {
		(*p_owner->__evt_step)(step, p_owner->__p_issuer);
	}

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[ENCODER_CSF_STATE_IDX], state);
	if (changed) {
		pifCollectSignal_Put(&p_owner->__cs[ENCODER_CSF_POSITION_IDX],
				(uint32_t)(_toCounts(p_owner, p_owner->__raw) + p_owner->__offset));
	}
#endif

	return step != 0 && step != INVALID;
}

void pifEncoder_AttachEvtStep(PifEncoder* p_owner, PifEvtEncoderStep evt_step, PifIssuerP p_issuer)
{
	p_owner->__evt_step = evt_step;
	p_owner->__p_issuer = p_issuer;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifEncoder_SetCsFlag(PifEncoder* p_owner, PifEncoderCsFlag flag)
{
	if (flag & ENCODER_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[ENCODER_CSF_STATE_IDX], "EC", p_owner->_id, CSVT_WIRE, 2,
				p_owner->__state)) return FALSE;
	}
	if (flag & ENCODER_CSF_POSITION_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[ENCODER_CSF_POSITION_IDX], "ECPos", p_owner->_id,
				CSVT_INTEGER, 32, (uint32_t)pifEncoder_GetPosition(p_owner))) return FALSE;
	}
	return TRUE;
}

void pifEncoder_ResetCsFlag(PifEncoder* p_owner, PifEncoderCsFlag flag)
{
	if (flag & ENCODER_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[ENCODER_CSF_STATE_IDX]);
	if (flag & ENCODER_CSF_POSITION_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[ENCODER_CSF_POSITION_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
