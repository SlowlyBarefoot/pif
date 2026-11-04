// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_quad_encoder.h"

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
static int32_t _toCounts(PifQuadEncoder *p_owner, int32_t raw)
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
static int32_t _readRaw(PifQuadEncoder *p_owner)
{
	int32_t raw;
	uint8_t seq;

	do {
		seq = p_owner->__seq;
		raw = p_owner->__raw;
	} while (seq != p_owner->__seq);
	return raw;
}

BOOL pifQuadEncoder_Init(PifQuadEncoder *p_owner, PifId id, PifQuadEncoderResolution resolution)
{
	if (!p_owner || resolution > QUAD_ENCODER_RES_X4) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifQuadEncoder));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_resolution = resolution;
	return TRUE;
}

void pifQuadEncoder_Clear(PifQuadEncoder *p_owner)
{
#ifdef COLLECT_SIGNAL_QUAD_ENCODER
	pifQuadEncoder_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, QUAD_ENCODER_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifQuadEncoder_SetReverse(PifQuadEncoder *p_owner, BOOL reverse)
{
	p_owner->__reverse = reverse;
}

void pifQuadEncoder_SetTimeout(PifQuadEncoder *p_owner, uint32_t timeout_us)
{
	p_owner->__timeout = timeout_us;
}

void pifQuadEncoder_SetPosition(PifQuadEncoder *p_owner, int32_t position)
{
	p_owner->__offset = position - _toCounts(p_owner, _readRaw(p_owner));
}

int32_t pifQuadEncoder_GetPosition(PifQuadEncoder *p_owner)
{
	return _toCounts(p_owner, _readRaw(p_owner)) + p_owner->__offset;
}

int8_t pifQuadEncoder_GetDirection(PifQuadEncoder *p_owner)
{
	return p_owner->__direction;
}

uint32_t pifQuadEncoder_GetErrorCount(PifQuadEncoder *p_owner)
{
	uint32_t count;
	uint8_t seq;

	do {
		seq = p_owner->__seq;
		count = p_owner->__error_count;
	} while (seq != p_owner->__seq);
	return count;
}

PifQuadEncoderResult pifQuadEncoder_ReadSpeed(PifQuadEncoder *p_owner, float *p_speed)
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
		last = p_owner->__time[(ptr + PIF_QUAD_ENCODER_DATA_MASK) & PIF_QUAD_ENCODER_DATA_MASK];
		first = p_owner->__time[(ptr + PIF_QUAD_ENCODER_DATA_MASK - intervals) & PIF_QUAD_ENCODER_DATA_MASK];
	} while (seq != p_owner->__seq);

	if (!count) return QUAD_ENCODER_R_NO_DATA;
	elapsed = (*pif_act_timer1us)() - last;
	if (p_owner->__timeout && elapsed > p_owner->__timeout) return QUAD_ENCODER_R_TIMEOUT;
	if (!intervals) return QUAD_ENCODER_R_NO_DATA;

	// A step later than the measured period shows that the encoder has slowed down to at most this.
	period = (float)(last - first) / intervals;
	if (elapsed > period) period = elapsed;
	if (period < 1.0f) period = 1.0f;

	if (p_speed) *p_speed = direction * 1000000.0f / period / (4 >> p_owner->_resolution);
	return QUAD_ENCODER_R_OK;
}

float pifQuadEncoder_GetSpeed(PifQuadEncoder *p_owner)
{
	float speed;

	pifQuadEncoder_ReadSpeed(p_owner, &speed);
	return speed;
}

BOOL pifQuadEncoder_sigState(PifQuadEncoder *p_owner, uint8_t state, uint32_t time_us)
{
	int8_t step;
	int32_t counts;
	BOOL changed = FALSE;

	state &= QUAD_ENCODER_PHASE_A | QUAD_ENCODER_PHASE_B;
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
				time_us - p_owner->__time[(p_owner->__ptr + PIF_QUAD_ENCODER_DATA_MASK) & PIF_QUAD_ENCODER_DATA_MASK] >
				p_owner->__timeout)) {
			p_owner->__count = 0;
		}
		p_owner->__direction = step;
		p_owner->__time[p_owner->__ptr] = time_us;
		p_owner->__ptr = (p_owner->__ptr + 1) & PIF_QUAD_ENCODER_DATA_MASK;
		if (p_owner->__count < PIF_QUAD_ENCODER_DATA_SIZE) p_owner->__count++;
	}
	p_owner->__seq++;

	if (changed && p_owner->__evt_step) {
		(*p_owner->__evt_step)(step, p_owner->__p_issuer);
	}

#ifdef COLLECT_SIGNAL_QUAD_ENCODER
	pifCollectSignal_Put(&p_owner->__cs[QUAD_ENCODER_CSF_STATE_IDX], state);
	if (changed) {
		pifCollectSignal_Put(&p_owner->__cs[QUAD_ENCODER_CSF_POSITION_IDX],
				(uint32_t)(_toCounts(p_owner, p_owner->__raw) + p_owner->__offset));
	}
#endif

	return step != 0 && step != INVALID;
}

void pifQuadEncoder_AttachEvtStep(PifQuadEncoder *p_owner, PifEvtQuadEncoderStep evt_step, PifIssuerP p_issuer)
{
	p_owner->__evt_step = evt_step;
	p_owner->__p_issuer = p_issuer;
}

#ifdef COLLECT_SIGNAL_QUAD_ENCODER

BOOL pifQuadEncoder_SetCsFlag(PifQuadEncoder *p_owner, uint8_t index, PifQuadEncoderCsFlag flag)
{
	(void)index;
	if (flag & QUAD_ENCODER_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[QUAD_ENCODER_CSF_STATE_IDX], "EC", p_owner->_id, CSVT_WIRE, 2,
				p_owner->__state)) return FALSE;
	}
	if (flag & QUAD_ENCODER_CSF_POSITION_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[QUAD_ENCODER_CSF_POSITION_IDX], "ECPos", p_owner->_id,
				CSVT_INTEGER, 32, (uint32_t)pifQuadEncoder_GetPosition(p_owner))) return FALSE;
	}
	return TRUE;
}

void pifQuadEncoder_ResetCsFlag(PifQuadEncoder *p_owner, uint8_t index, PifQuadEncoderCsFlag flag)
{
	(void)index;
	if (flag & QUAD_ENCODER_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[QUAD_ENCODER_CSF_STATE_IDX]);
	if (flag & QUAD_ENCODER_CSF_POSITION_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[QUAD_ENCODER_CSF_POSITION_IDX]);
}

#endif	// COLLECT_SIGNAL_QUAD_ENCODER
