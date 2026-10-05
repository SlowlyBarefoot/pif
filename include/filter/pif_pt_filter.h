// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_PT_FILTER_H
#define PIF_PT_FILTER_H


#include "core/pif.h"


#define PIF_PT_FILTER_MAX_ORDER		3


/**
 * @class StPifPtFilter
 * @brief Low-pass filter of 1 to 3 identical first-order (RC) stages in series (PT1, PT2, PT3).
 *
 * The stage cutoff is raised for orders 2 and 3 so that the whole filter is
 * 3 dB down at the requested cutoff frequency. The stage gain uses the cheap
 * k = w / (w + 1) discretization, so it can be recalculated every sample; the
 * gain at the cutoff is close to -3 dB while the cutoff is far below the sample
 * rate (about 2 % low for PT1 and 5 % low for PT3 at 1/80 of the sample rate).
 *
 * A zero-filled filter that was never initialized outputs 0.
 */
typedef struct StPifPtFilter
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t _order;
	float _k;					// Gain of each stage, 0 to 1

	// Private Member Variable
	float __state[PIF_PT_FILTER_MAX_ORDER];
} PifPtFilter;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPtFilter_Gain
 * @brief Calculates the gain of one stage for an order and a cutoff frequency.
 * @param order Number of stages, 1 to PIF_PT_FILTER_MAX_ORDER.
 * @param cutoff_hz Cutoff (-3 dB) frequency of the whole filter in Hz.
 * @param dt Sample period in seconds.
 * @return Stage gain, 0 to 1.
 */
float pifPtFilter_Gain(uint8_t order, float cutoff_hz, float dt);

/**
 * @fn pifPtFilter_Init
 * @brief Initializes the filter and clears its state.
 * @param p_owner Pointer to the target object instance.
 * @param order Number of stages, 1 to PIF_PT_FILTER_MAX_ORDER.
 * @param cutoff_hz Cutoff (-3 dB) frequency in Hz.
 * @param dt Sample period in seconds.
 * @return TRUE if successful, FALSE if order is out of range.
 */
BOOL pifPtFilter_Init(PifPtFilter* p_owner, uint8_t order, float cutoff_hz, float dt);

/**
 * @fn pifPtFilter_SetCutoff
 * @brief Changes the cutoff frequency and keeps the state.
 * @param p_owner Pointer to the target object instance.
 * @param cutoff_hz Cutoff (-3 dB) frequency in Hz.
 * @param dt Sample period in seconds.
 */
void pifPtFilter_SetCutoff(PifPtFilter* p_owner, float cutoff_hz, float dt);

/**
 * @fn pifPtFilter_SetGain
 * @brief Sets the stage gain directly, for example one calculated in advance with pifPtFilter_Gain().
 * @param p_owner Pointer to the target object instance.
 * @param k Stage gain, 0 to 1.
 */
void pifPtFilter_SetGain(PifPtFilter* p_owner, float k);

/**
 * @fn pifPtFilter_Reset
 * @brief Sets every stage to a value so the output starts there without a transient.
 * @param p_owner Pointer to the target object instance.
 * @param value Initial output value.
 */
void pifPtFilter_Reset(PifPtFilter* p_owner, float value);

/**
 * @fn pifPtFilter_Output
 * @brief Returns the last output without filtering a new sample.
 * @param p_owner Pointer to the target object instance.
 * @return Last output, or the value set by pifPtFilter_Reset().
 */
float pifPtFilter_Output(const PifPtFilter* p_owner);

/**
 * @fn pifPtFilter_Apply
 * @brief Filters one sample.
 * @param p_owner Pointer to the target object instance.
 * @param input Input sample.
 * @return Filtered output.
 */
float pifPtFilter_Apply(PifPtFilter* p_owner, float input);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PT_FILTER_H
