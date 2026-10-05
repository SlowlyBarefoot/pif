// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_pt_filter.h"

// Cascaded first-order low-pass filters.
//
// One RC stage sampled every dt is y += k * (x - y) with k = w / (w + 1) and
// w = 2 * pi * fc * dt. n identical stages are 3 dB down where
// (1 + (f / fs)^2)^n = 2, so each stage gets fs = fc / sqrt(2^(1/n) - 1).

static const float c_stage_scale[PIF_PT_FILTER_MAX_ORDER] = {
	1.0f,			// 1 / sqrt(2^(1/1) - 1)
	1.55377397f,	// 1 / sqrt(2^(1/2) - 1)
	1.96145918f		// 1 / sqrt(2^(1/3) - 1)
};

float pifPtFilter_Gain(uint8_t order, float cutoff_hz, float dt)
{
	float w;

	if (order < 1 || order > PIF_PT_FILTER_MAX_ORDER) order = 1;
	w = 2.0f * PIF_PI * cutoff_hz * c_stage_scale[order - 1] * dt;
	return w / (w + 1.0f);
}

BOOL pifPtFilter_Init(PifPtFilter* p_owner, uint8_t order, float cutoff_hz, float dt)
{
	if (order < 1 || order > PIF_PT_FILTER_MAX_ORDER) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_order = order;
	p_owner->_k = pifPtFilter_Gain(order, cutoff_hz, dt);
	pifPtFilter_Reset(p_owner, 0.0f);
	return TRUE;
}

void pifPtFilter_SetCutoff(PifPtFilter* p_owner, float cutoff_hz, float dt)
{
	p_owner->_k = pifPtFilter_Gain(p_owner->_order, cutoff_hz, dt);
}

void pifPtFilter_SetGain(PifPtFilter* p_owner, float k)
{
	p_owner->_k = k;
}

void pifPtFilter_Reset(PifPtFilter* p_owner, float value)
{
	uint8_t i;

	for (i = 0; i < PIF_PT_FILTER_MAX_ORDER; i++) {
		p_owner->__state[i] = value;
	}
}

float pifPtFilter_Output(const PifPtFilter* p_owner)
{
	return p_owner->__state[p_owner->_order ? p_owner->_order - 1 : 0];
}

float pifPtFilter_Apply(PifPtFilter* p_owner, float input)
{
	uint8_t i = 0;

	// At least one stage runs, so a zero-filled filter that was never
	// initialized (order 0, gain 0) holds 0 instead of passing the input through.
	do {
		p_owner->__state[i] += p_owner->_k * (input - p_owner->__state[i]);
		input = p_owner->__state[i];
	} while (++i < p_owner->_order);
	return input;
}
