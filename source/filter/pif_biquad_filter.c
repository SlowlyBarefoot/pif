// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_biquad_filter.h"
#include "core/pif_math.h"

// Biquad coefficients from R. Bristow-Johnson, "Cookbook formulae for audio EQ
// biquad filter coefficients":
//
//   w0 = 2 * pi * f0 / fs, alpha = sin(w0) / (2 * Q)
//   a0 = 1 + alpha, a1 = -2 * cos(w0), a2 = 1 - alpha
//   LPF:  b0 = b2 = (1 - cos(w0)) / 2, b1 = 1 - cos(w0)
//   notch: b0 = b2 = 1, b1 = -2 * cos(w0)
//   BPF (0 dB peak): b0 = alpha, b1 = 0, b2 = -alpha

BOOL pifBiquadFilter_Init(PifBiquadFilter* p_owner, PifBiquadFilterType type, float freq_hz, float sample_rate_hz, float q)
{
	BOOL result;

	result = pifBiquadFilter_Update(p_owner, type, freq_hz, sample_rate_hz, q);
	if (!result) {
		// Pass the input through rather than run with undefined coefficients.
		p_owner->_type = type;
		p_owner->_b0 = 1.0f;
		p_owner->_b1 = p_owner->_b2 = 0.0f;
		p_owner->_a1 = p_owner->_a2 = 0.0f;
	}
	pifBiquadFilter_Reset(p_owner);
	return result;
}

BOOL pifBiquadFilter_Update(PifBiquadFilter* p_owner, PifBiquadFilterType type, float freq_hz, float sample_rate_hz, float q)
{
	float w0, cs, alpha, a0;

	if (freq_hz <= 0.0f || freq_hz >= sample_rate_hz / 2 || q <= 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	w0 = 2.0f * PIF_PI * freq_hz / sample_rate_hz;
	// The approximations are accurate to 3e-7 and much cheaper than the C library,
	// which matters when the frequency is retuned every sample.
	cs = pifMath_CosApprox(w0);
	alpha = pifMath_SinApprox(w0) / (2.0f * q);
	a0 = 1.0f + alpha;

	switch (type) {
	case BQFT_LOWPASS:
		p_owner->_b1 = (1.0f - cs) / a0;
		p_owner->_b0 = p_owner->_b1 / 2;
		p_owner->_b2 = p_owner->_b0;
		break;

	case BQFT_NOTCH:
		p_owner->_b0 = 1.0f / a0;
		p_owner->_b1 = -2.0f * cs / a0;
		p_owner->_b2 = p_owner->_b0;
		break;

	case BQFT_BANDPASS:
		p_owner->_b0 = alpha / a0;
		p_owner->_b1 = 0.0f;
		p_owner->_b2 = -p_owner->_b0;
		break;

	default:
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->_a1 = -2.0f * cs / a0;
	p_owner->_a2 = (1.0f - alpha) / a0;
	p_owner->_type = type;
	return TRUE;
}

void pifBiquadFilter_CopyCoefficients(PifBiquadFilter* p_owner, const PifBiquadFilter* p_src)
{
	p_owner->_type = p_src->_type;
	p_owner->_b0 = p_src->_b0;
	p_owner->_b1 = p_src->_b1;
	p_owner->_b2 = p_src->_b2;
	p_owner->_a1 = p_src->_a1;
	p_owner->_a2 = p_src->_a2;
}

float pifBiquadFilter_NotchQ(float center_hz, float cutoff_hz)
{
	return center_hz * cutoff_hz / (center_hz * center_hz - cutoff_hz * cutoff_hz);
}

void pifBiquadFilter_Reset(PifBiquadFilter* p_owner)
{
	p_owner->__x1 = p_owner->__x2 = 0.0f;
	p_owner->__y1 = p_owner->__y2 = 0.0f;
}

float pifBiquadFilter_Apply(PifBiquadFilter* p_owner, float input)
{
	float output;

	output = p_owner->_b0 * input + p_owner->_b1 * p_owner->__x1 + p_owner->_b2 * p_owner->__x2
			- p_owner->_a1 * p_owner->__y1 - p_owner->_a2 * p_owner->__y2;

	p_owner->__x2 = p_owner->__x1;
	p_owner->__x1 = input;
	p_owner->__y2 = p_owner->__y1;
	p_owner->__y1 = output;
	return output;
}
