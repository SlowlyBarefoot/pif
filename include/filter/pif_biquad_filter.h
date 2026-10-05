// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_BIQUAD_FILTER_H
#define PIF_BIQUAD_FILTER_H


#include "core/pif.h"


// Q of a second-order Butterworth low-pass, 1 / sqrt(2).
#define PIF_BIQUAD_Q_BUTTERWORTH	0.70710678f


typedef enum EnPifBiquadFilterType
{
	BQFT_LOWPASS,
	BQFT_NOTCH,
	BQFT_BANDPASS			// 0 dB gain at the center frequency
} PifBiquadFilterType;


/**
 * @class StPifBiquadFilter
 * @brief Second-order IIR filter with coefficients from the RBJ Audio EQ Cookbook.
 *
 * The filter runs in direct form I, so the coefficients can be changed with
 * pifBiquadFilter_Update() while it runs without a large transient.
 */
typedef struct StPifBiquadFilter
{
	// Public Member Variable

	// Read-only Member Variable
	PifBiquadFilterType _type;
	float _b0, _b1, _b2;		// Feed-forward coefficients, normalized by a0
	float _a1, _a2;				// Feedback coefficients, normalized by a0

	// Private Member Variable
	float __x1, __x2;			// Previous inputs
	float __y1, __y2;			// Previous outputs
} PifBiquadFilter;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBiquadFilter_Init
 * @brief Initializes the filter, calculates its coefficients and clears its state.
 * @param p_owner Pointer to the target object instance.
 * @param type Filter response.
 * @param freq_hz Cutoff frequency (low-pass) or center frequency (notch, band-pass) in Hz.
 * @param sample_rate_hz Sample rate in Hz.
 * @param q Quality factor. Use PIF_BIQUAD_Q_BUTTERWORTH for a flat low-pass,
 *          or pifBiquadFilter_NotchQ() for a notch.
 * @return TRUE if successful, FALSE if freq_hz is not between 0 and half the sample rate or q is not positive.
 *         The filter then passes the input through unchanged.
 */
BOOL pifBiquadFilter_Init(PifBiquadFilter* p_owner, PifBiquadFilterType type, float freq_hz, float sample_rate_hz, float q);

/**
 * @fn pifBiquadFilter_Update
 * @brief Recalculates the coefficients and keeps the state.
 * @param p_owner Pointer to the target object instance.
 * @param type Filter response.
 * @param freq_hz Cutoff or center frequency in Hz.
 * @param sample_rate_hz Sample rate in Hz.
 * @param q Quality factor.
 * @return TRUE if successful, FALSE if the parameters are out of range. The coefficients are left unchanged then.
 */
BOOL pifBiquadFilter_Update(PifBiquadFilter* p_owner, PifBiquadFilterType type, float freq_hz, float sample_rate_hz, float q);

/**
 * @fn pifBiquadFilter_CopyCoefficients
 * @brief Copies the response of another filter and keeps the state, for filters that share one tuning.
 * @param p_owner Pointer to the target object instance.
 * @param p_src Filter whose coefficients are copied.
 */
void pifBiquadFilter_CopyCoefficients(PifBiquadFilter* p_owner, const PifBiquadFilter* p_src);

/**
 * @fn pifBiquadFilter_NotchQ
 * @brief Calculates the Q of a notch from its center frequency and its lower -3 dB frequency.
 * @param center_hz Center frequency in Hz.
 * @param cutoff_hz Lower -3 dB frequency in Hz, below center_hz.
 * @return Quality factor, center * cutoff / (center^2 - cutoff^2).
 */
float pifBiquadFilter_NotchQ(float center_hz, float cutoff_hz);

/**
 * @fn pifBiquadFilter_Reset
 * @brief Clears the state so the next output starts from zero history.
 * @param p_owner Pointer to the target object instance.
 */
void pifBiquadFilter_Reset(PifBiquadFilter* p_owner);

/**
 * @fn pifBiquadFilter_Apply
 * @brief Filters one sample.
 * @param p_owner Pointer to the target object instance.
 * @param input Input sample.
 * @return Filtered output.
 */
float pifBiquadFilter_Apply(PifBiquadFilter* p_owner, float input);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BIQUAD_FILTER_H
