// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_dyn_notch.h"


#define SDFT_DAMPING		0.9999f
#define BIN_BATCHES			3

typedef enum EnStep
{
	STEP_IDLE			= 0,
	STEP_BINS			= 1,	// One of BIN_BATCHES parts of the DFT update
	STEP_PEAKS			= 2,
	STEP_NOTCHES		= 3
} Step;


/**
 * @fn _updateBins
 * @brief Updates the next part of the DFT bins.
 * @param p_owner Pointer to the instance.
 */
static void _updateBins(PifDynNotch* p_owner)
{
	uint16_t per_batch = (p_owner->__sdft._bin_count + BIN_BATCHES - 1) / BIN_BATCHES;

	pifSdft_UpdateBins(&p_owner->__sdft, p_owner->__batch * per_batch, per_batch);
	if (++p_owner->__batch >= BIN_BATCHES) {
		// Peaks mean nothing until the window has filled once.
		p_owner->__step = p_owner->__sdft._sample_count >= PIF_DYN_NOTCH_SDFT_SIZE ? STEP_PEAKS : STEP_IDLE;
	}
}

/**
 * @fn _findPeaks
 * @brief Finds the strongest local maxima of the windowed spectrum, then keeps those that stand
 *        above the noise floor times the threshold and lie within the range.
 * @param p_owner Pointer to the instance.
 */
static void _findPeaks(PifDynNotch* p_owner)
{
	const PifSdft* p_sdft = &p_owner->__sdft;
	float power[PIF_DYN_NOTCH_MAX_BINS];
	uint16_t bin[PIF_DYN_NOTCH_MAX_NOTCHES];
	float sum = 0.0f, floor_sum, hz, offset, denominator, y0, y1, y2;
	uint16_t i, last = p_sdft->_bin_count - 2, floor_bins;
	uint8_t j, k, found = 0, kept = 0;

	// Windowed power exists for bins 1 to _bin_count - 2.
	for (i = 1; i <= last; i++) {
		power[i] = pifSdft_GetHannPower(p_sdft, i);
		sum += power[i];
	}

	// The notch_count strongest local maxima, strongest first.
	for (i = 2; i < last; i++) {
		if (power[i] <= power[i - 1] || power[i] < power[i + 1]) continue;
		j = 0;
		while (j < found && p_owner->__peak_power[j] >= power[i]) j++;
		if (j >= p_owner->_notch_count) continue;
		if (found < p_owner->_notch_count) found++;
		for (k = found - 1; k > j; k--) {
			bin[k] = bin[k - 1];
			p_owner->__peak_power[k] = p_owner->__peak_power[k - 1];
		}
		bin[j] = i;
		p_owner->__peak_power[j] = power[i];
	}

	// Noise floor: the mean power, optionally without the peaks and their shoulders.
	floor_sum = sum;
	floor_bins = last;
	if (p_owner->__config.floor_excludes_peaks) {
		for (j = 0; j < found; j++) {
			floor_sum -= 0.75f * power[bin[j] - 1] + power[bin[j]] + 0.75f * power[bin[j] + 1];
		}
		if (floor_sum < 0.0f) floor_sum = 0.0f;
		floor_bins -= found;
	}
	p_owner->__threshold = p_owner->__config.threshold * floor_sum / floor_bins;

	for (j = 0; j < found; j++) {
		if (p_owner->__peak_power[j] <= p_owner->__threshold) continue;

		// Parabola through the peak and its neighbours.
		y0 = power[bin[j] - 1];
		y1 = power[bin[j]];
		y2 = power[bin[j] + 1];
		denominator = y0 - 2.0f * y1 + y2;
		offset = denominator != 0.0f ? 0.5f * (y0 - y2) / denominator : 0.0f;
		if (offset > 0.5f) offset = 0.5f;
		else if (offset < -0.5f) offset = -0.5f;
		hz = (p_sdft->_start_bin + bin[j] + offset) * p_owner->_resolution_hz;
		if (hz < p_owner->__config.min_hz || hz > p_owner->__config.max_hz) continue;

		p_owner->_peak_hz[kept] = hz;
		p_owner->__peak_power[kept] = p_owner->__peak_power[j];
		kept++;
	}
	p_owner->_peak_count = kept;
}

/**
 * @fn _updateNotches
 * @brief Moves the nearest free notch toward each peak, strongest peak first.
 * @param p_owner Pointer to the instance.
 */
static void _updateNotches(PifDynNotch* p_owner)
{
	BOOL taken[PIF_DYN_NOTCH_MAX_NOTCHES] = { FALSE };
	float distance, best_distance, cutoff;
	uint8_t i, j, best;

	for (i = 0; i < p_owner->_peak_count; i++) {
		best = p_owner->_notch_count;
		best_distance = 0.0f;
		for (j = 0; j < p_owner->_notch_count; j++) {
			if (taken[j]) continue;
			distance = p_owner->_center_hz[j] - p_owner->_peak_hz[i];
			if (distance < 0.0f) distance = -distance;
			if (best == p_owner->_notch_count || distance < best_distance) {
				best = j;
				best_distance = distance;
			}
		}
		if (best == p_owner->_notch_count) break;
		taken[best] = TRUE;

		if (p_owner->__config.smoothing_hz > 0.0f) {
			if (p_owner->__config.smoothing_max_hz > p_owner->__config.smoothing_hz) {
				// Follow a peak faster the more it stands out.
				cutoff = p_owner->__threshold > 0.0f
						? p_owner->__config.smoothing_hz * p_owner->__peak_power[i] / p_owner->__threshold
						: p_owner->__config.smoothing_max_hz;
				if (cutoff < p_owner->__config.smoothing_hz) cutoff = p_owner->__config.smoothing_hz;
				else if (cutoff > p_owner->__config.smoothing_max_hz) cutoff = p_owner->__config.smoothing_max_hz;
				pifPtFilter_SetCutoff(&p_owner->__smooth[best], cutoff, 1.0f / p_owner->_analysis_rate_hz);
			}
			p_owner->_center_hz[best] = pifPtFilter_Apply(&p_owner->__smooth[best], p_owner->_peak_hz[i]);
		}
		else {
			p_owner->_center_hz[best] = p_owner->_peak_hz[i];
		}
		pifBiquadFilter_Update(&p_owner->__notch[best], BQFT_NOTCH, p_owner->_center_hz[best],
				p_owner->_sample_rate_hz, p_owner->__config.q);
	}
}

/**
 * @fn _runStep
 * @brief Runs the current step of the analysis.
 * @param p_owner Pointer to the instance.
 */
static void _runStep(PifDynNotch* p_owner)
{
	switch (p_owner->__step) {
	case STEP_BINS:
		_updateBins(p_owner);
		break;

	case STEP_PEAKS:
		_findPeaks(p_owner);
		p_owner->__step = STEP_NOTCHES;
		break;

	case STEP_NOTCHES:
		_updateNotches(p_owner);
		p_owner->__step = STEP_IDLE;
		break;

	default:
		break;
	}
}

BOOL pifDynNotch_Init(PifDynNotch* p_owner, float sample_rate_hz, const PifDynNotchConfig* p_config)
{
	float spacing;
	uint32_t decimation, start_bin, end_bin;
	uint8_t i;

	if (!p_owner || !p_config || sample_rate_hz <= 0.0f || p_config->min_hz <= 0.0f ||
			p_config->min_hz >= p_config->max_hz || p_config->max_hz >= sample_rate_hz / 2 ||
			!p_config->notch_count || p_config->notch_count > PIF_DYN_NOTCH_MAX_NOTCHES || p_config->q <= 0.0f ||
			p_config->smoothing_hz < 0.0f || p_config->smoothing_max_hz < 0.0f || p_config->threshold < 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifDynNotch));
	p_owner->__config = *p_config;
	p_owner->_sample_rate_hz = sample_rate_hz;
	p_owner->_notch_count = p_config->notch_count;

	// Average down to the lowest rate that still sees max_hz.
	decimation = (uint32_t)(sample_rate_hz / (2.0f * p_config->max_hz));
	if (decimation < 1) decimation = 1;
	if (decimation > 0xFFFF) decimation = 0xFFFF;
	p_owner->_decimation = decimation;
	p_owner->_analysis_rate_hz = sample_rate_hz / decimation;
	p_owner->_resolution_hz = p_owner->_analysis_rate_hz / PIF_DYN_NOTCH_SDFT_SIZE;

	// Bins covering the range, with two more on each side: one for the Hann window and one for
	// the neighbour a peak is compared with.
	start_bin = (uint32_t)(p_config->min_hz / p_owner->_resolution_hz);
	start_bin = start_bin > 2 ? start_bin - 2 : 0;
	end_bin = (uint32_t)(p_config->max_hz / p_owner->_resolution_hz + 0.999f) + 2;
	if (end_bin > PIF_DYN_NOTCH_SDFT_SIZE / 2) end_bin = PIF_DYN_NOTCH_SDFT_SIZE / 2;
	if (end_bin < start_bin + 4) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!pifSdft_Init(&p_owner->__sdft, PIF_DYN_NOTCH_SDFT_SIZE, p_owner->__samples, start_bin,
			end_bin - start_bin + 1, p_owner->__bins, SDFT_DAMPING)) return FALSE;

	spacing = (p_config->max_hz - p_config->min_hz) / p_config->notch_count;
	for (i = 0; i < p_config->notch_count; i++) {
		p_owner->_center_hz[i] = p_config->min_hz + (i + 0.5f) * spacing;
		pifBiquadFilter_Init(&p_owner->__notch[i], BQFT_NOTCH, p_owner->_center_hz[i], sample_rate_hz, p_config->q);
		pifPtFilter_Init(&p_owner->__smooth[i], 1, p_config->smoothing_hz > 0.0f ? p_config->smoothing_hz : 1.0f,
				1.0f / p_owner->_analysis_rate_hz);
		pifPtFilter_Reset(&p_owner->__smooth[i], p_owner->_center_hz[i]);
	}
	return TRUE;
}

void pifDynNotch_SetPhase(PifDynNotch* p_owner, uint16_t phase)
{
	p_owner->__accumulator = 0.0f;
	p_owner->__accumulated = phase % p_owner->_decimation;
}

float pifDynNotch_Apply(PifDynNotch* p_owner, float input)
{
	float output = input;
	uint8_t i;

	p_owner->__accumulator += input;
	if (++p_owner->__accumulated >= p_owner->_decimation) {
		p_owner->__pending = p_owner->__accumulator / p_owner->_decimation;
		p_owner->__pending_ready = TRUE;
		p_owner->__accumulator = 0.0f;
		p_owner->__accumulated = 0;
	}

	for (i = 0; i < p_owner->_notch_count; i++) {
		output = pifBiquadFilter_Apply(&p_owner->__notch[i], output);
	}
	return output;
}

void pifDynNotch_Update(PifDynNotch* p_owner)
{
	if (p_owner->__pending_ready) {
		// Whatever is left of the last analysis has to be done before the next sample goes in.
		while (p_owner->__step != STEP_IDLE) _runStep(p_owner);

		pifSdft_PushSample(&p_owner->__sdft, p_owner->__pending);
		p_owner->__pending_ready = FALSE;
		p_owner->__step = STEP_BINS;
		p_owner->__batch = 0;
	}
	_runStep(p_owner);
}
