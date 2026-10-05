// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SLEW_FILTER_H
#define PIF_SLEW_FILTER_H


#include "core/pif.h"


/**
 * @class StPifSlewFilter
 * @brief Slew-rate limiter: the output moves toward the input by at most max_step per sample.
 */
typedef struct StPifSlewFilter
{
	// Public Member Variable
	float max_step;				// Largest change of the output per sample, > 0

	// Read-only Member Variable
	float _output;
} PifSlewFilter;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSlewFilter_Init
 * @brief Initializes the limiter.
 * @param p_owner Pointer to the target object instance.
 * @param max_step Largest change of the output per sample. For a rate in units
 *                 per second, pass rate * sample period.
 * @param initial Initial output value.
 */
void pifSlewFilter_Init(PifSlewFilter* p_owner, float max_step, float initial);

/**
 * @fn pifSlewFilter_Reset
 * @brief Sets the output to a value without limiting.
 * @param p_owner Pointer to the target object instance.
 * @param value New output value.
 */
void pifSlewFilter_Reset(PifSlewFilter* p_owner, float value);

/**
 * @fn pifSlewFilter_Apply
 * @brief Moves the output toward the input by at most max_step.
 * @param p_owner Pointer to the target object instance.
 * @param input Input sample.
 * @return Limited output.
 */
float pifSlewFilter_Apply(PifSlewFilter* p_owner, float input);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SLEW_FILTER_H
