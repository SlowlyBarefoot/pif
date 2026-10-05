// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_sdft.h"
#include "core/pif_math.h"

// Damped sliding DFT (Jacobsen and Lyons, 2003).

BOOL pifSdft_Init(PifSdft* p_owner, uint16_t window_size, float* p_samples, uint16_t start_bin, uint16_t bin_count,
		PifSdftBin* p_bins, float damping)
{
	uint16_t i;
	float angle;

	if (!p_owner || window_size < 2 || !p_samples || !bin_count || !p_bins ||
			(uint32_t)start_bin + bin_count > window_size / 2U + 1 || damping <= 0.0f || damping > 1.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_window_size = window_size;
	p_owner->_start_bin = start_bin;
	p_owner->_bin_count = bin_count;
	p_owner->__p_samples = p_samples;
	p_owner->__p_bins = p_bins;
	p_owner->__damping = damping;

	p_owner->__damping_n = 1.0f;
	for (i = 0; i < window_size; i++) p_owner->__damping_n *= damping;

	for (i = 0; i < bin_count; i++) {
		angle = 2.0f * PIF_PI * (start_bin + i) / window_size;
		p_bins[i].tw_re = pifMath_CosApprox(angle);
		p_bins[i].tw_im = pifMath_SinApprox(angle);
	}

	pifSdft_Reset(p_owner);
	return TRUE;
}

void pifSdft_Reset(PifSdft* p_owner)
{
	uint16_t i;

	for (i = 0; i < p_owner->_window_size; i++) p_owner->__p_samples[i] = 0.0f;
	for (i = 0; i < p_owner->_bin_count; i++) {
		p_owner->__p_bins[i].re = 0.0f;
		p_owner->__p_bins[i].im = 0.0f;
	}
	p_owner->__index = 0;
	p_owner->__delta = 0.0f;
	p_owner->_sample_count = 0;
}

void pifSdft_PushSample(PifSdft* p_owner, float sample)
{
	float oldest = p_owner->__p_samples[p_owner->__index];

	p_owner->__p_samples[p_owner->__index] = sample;
	if (++p_owner->__index >= p_owner->_window_size) p_owner->__index = 0;
	p_owner->__delta = sample - p_owner->__damping_n * oldest;
	if (p_owner->_sample_count < 0xFFFFFFFFUL) p_owner->_sample_count++;
}

void pifSdft_UpdateBins(PifSdft* p_owner, uint16_t first, uint16_t count)
{
	PifSdftBin* p_bin;
	float re, im;
	uint16_t i;

	if (first >= p_owner->_bin_count) return;
	if (count > p_owner->_bin_count - first) count = p_owner->_bin_count - first;

	for (i = 0; i < count; i++) {
		p_bin = &p_owner->__p_bins[first + i];
		re = p_owner->__damping * p_bin->re + p_owner->__delta;
		im = p_owner->__damping * p_bin->im;
		p_bin->re = re * p_bin->tw_re - im * p_bin->tw_im;
		p_bin->im = re * p_bin->tw_im + im * p_bin->tw_re;
	}
}

void pifSdft_Push(PifSdft* p_owner, float sample)
{
	pifSdft_PushSample(p_owner, sample);
	pifSdft_UpdateBins(p_owner, 0, p_owner->_bin_count);
}

float pifSdft_GetPower(const PifSdft* p_owner, uint16_t index)
{
	const PifSdftBin* p_bin;

	if (index >= p_owner->_bin_count) return 0.0f;
	p_bin = &p_owner->__p_bins[index];
	return p_bin->re * p_bin->re + p_bin->im * p_bin->im;
}

float pifSdft_GetHannPower(const PifSdft* p_owner, uint16_t index)
{
	const PifSdftBin* p_bins = p_owner->__p_bins;
	float re, im;

	if (index < 1 || index + 1 >= p_owner->_bin_count) return 0.0f;

	// The window is referenced to its oldest sample, so the Hann window is the convolution
	// 0.5 X_k - 0.25 X_k-1 - 0.25 X_k+1.
	re = 0.5f * p_bins[index].re - 0.25f * (p_bins[index - 1].re + p_bins[index + 1].re);
	im = 0.5f * p_bins[index].im - 0.25f * (p_bins[index - 1].im + p_bins[index + 1].im);
	return re * re + im * im;
}
