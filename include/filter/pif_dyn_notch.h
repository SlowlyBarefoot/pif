// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_DYN_NOTCH_H
#define PIF_DYN_NOTCH_H


#include "filter/pif_biquad_filter.h"
#include "filter/pif_pt_filter.h"
#include "filter/pif_sdft.h"


/*
 * Dynamic notch filter: finds the strongest narrow-band noise in a signal, such as the vibration of
 * a motor or a propeller whose frequency follows its speed, and keeps notch filters on it.
 *
 *   pifDynNotch_Apply()   runs every sample: it filters the sample through the notches and
 *                         collects it for the analysis, averaging decimation samples into one.
 *   pifDynNotch_Update()  runs the analysis one step per call: the sliding DFT of the collected
 *                         samples in three parts, then the peak search, then the notch update.
 *                         Calling it once per sample keeps every call short as long as the
 *                         decimation is at least the 5 steps, which holds while max_hz is at most
 *                         sample rate / 10. Otherwise a step left over when the next analysis
 *                         sample arrives runs at once, in the same call.
 *
 * The analysis takes the Hann-windowed spectrum between min_hz and max_hz and its notch_count
 * strongest local maxima, and keeps those above threshold times the noise floor, the mean power of
 * the spectrum. Each is placed between its bins by parabolic interpolation, and the nearest notch
 * moves toward it through a low-pass filter. Notches without a peak stay where they are.
 */


// Samples in the analysis window. The frequency resolution is the analysis rate divided by this.
#ifndef PIF_DYN_NOTCH_SDFT_SIZE
#define PIF_DYN_NOTCH_SDFT_SIZE		72
#endif

#ifndef PIF_DYN_NOTCH_MAX_NOTCHES
#define PIF_DYN_NOTCH_MAX_NOTCHES	7
#endif

#define PIF_DYN_NOTCH_MAX_BINS		(PIF_DYN_NOTCH_SDFT_SIZE / 2 + 1)


/**
 * @class StPifDynNotchConfig
 * @brief Range and behaviour of a dynamic notch.
 */
typedef struct StPifDynNotchConfig
{
	float min_hz;				// Lowest frequency a notch goes to
	float max_hz;				// Highest frequency a notch goes to, below half the sample rate
	uint8_t notch_count;		// 1 to PIF_DYN_NOTCH_MAX_NOTCHES
	float q;					// Quality factor of each notch; higher is narrower
	float smoothing_hz;			// Cutoff of the low-pass filter on each notch frequency, or 0 for none
	float smoothing_max_hz;		// Above smoothing_hz: the cutoff grows with peak / (threshold * floor) up to this
	float threshold;			// A peak counts above this times the noise floor
	BOOL floor_excludes_peaks;	// Leave the peaks and their shoulders out of the noise floor
} PifDynNotchConfig;

/**
 * @class StPifDynNotch
 * @brief Notch filters that track the strongest peaks of a signal's spectrum.
 */
typedef struct StPifDynNotch
{
	// Read-only Member Variable
	float _sample_rate_hz;
	float _analysis_rate_hz;	// Rate of the samples taken into the sliding DFT
	float _resolution_hz;		// Width of one DFT bin
	uint16_t _decimation;		// Samples averaged into one analysis sample
	uint8_t _notch_count;
	uint8_t _peak_count;		// Peaks found by the last analysis
	float _center_hz[PIF_DYN_NOTCH_MAX_NOTCHES];	// Frequency of each notch
	float _peak_hz[PIF_DYN_NOTCH_MAX_NOTCHES];		// Peaks found by the last analysis, strongest first

	// Private Member Variable
	PifDynNotchConfig __config;
	PifSdft __sdft;
	float __samples[PIF_DYN_NOTCH_SDFT_SIZE];
	PifSdftBin __bins[PIF_DYN_NOTCH_MAX_BINS];
	PifBiquadFilter __notch[PIF_DYN_NOTCH_MAX_NOTCHES];
	PifPtFilter __smooth[PIF_DYN_NOTCH_MAX_NOTCHES];
	float __peak_power[PIF_DYN_NOTCH_MAX_NOTCHES];
	float __threshold;			// Peak power threshold of the last analysis
	float __accumulator;
	uint16_t __accumulated;
	float __pending;
	BOOL __pending_ready;
	uint8_t __step;
	uint8_t __batch;
} PifDynNotch;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDynNotch_Init
 * @brief Initializes a dynamic notch with its notches spread evenly over the range.
 * @param p_owner Pointer to the instance.
 * @param sample_rate_hz Rate at which pifDynNotch_Apply() is called.
 * @param p_config Range and behaviour. It is copied.
 * @return TRUE on success, otherwise FALSE (E_INVALID_PARAM for a config that does not fit the
 *         sample rate or the compile-time limits).
 */
BOOL pifDynNotch_Init(PifDynNotch* p_owner, float sample_rate_hz, const PifDynNotchConfig* p_config);

/**
 * @fn pifDynNotch_SetPhase
 * @brief Shifts when the analysis samples are taken, for instances fed together, such as the three
 *        axes of a gyro: given different phases, they run their heavier analysis steps on different
 *        calls instead of all on the same one.
 * @param p_owner Pointer to the instance.
 * @param phase Samples to shift by, taken modulo _decimation.
 */
void pifDynNotch_SetPhase(PifDynNotch* p_owner, uint16_t phase);

/**
 * @fn pifDynNotch_Apply
 * @brief Filters one sample through the notches and collects it for the analysis.
 * @param p_owner Pointer to the instance.
 * @param input Sample.
 * @return Filtered sample.
 */
float pifDynNotch_Apply(PifDynNotch* p_owner, float input);

/**
 * @fn pifDynNotch_Update
 * @brief Runs one step of the analysis. Call it once per sample, or at least once per decimation
 *        times the number of steps (5).
 * @param p_owner Pointer to the instance.
 */
void pifDynNotch_Update(PifDynNotch* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DYN_NOTCH_H
