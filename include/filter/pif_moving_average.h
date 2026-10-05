// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MOVING_AVERAGE_H
#define PIF_MOVING_AVERAGE_H


#include "core/pif.h"


/**
 * @class StPifMovingAverage
 * @brief Float moving average over the last size samples, kept in a caller-provided buffer.
 *
 * Each sample costs O(1). The running sum is rebuilt from the buffer once per
 * window so float rounding errors do not accumulate.
 */
typedef struct StPifMovingAverage
{
	// Public Member Variable

	// Read-only Member Variable
	uint16_t _size;				// Window length in samples
	uint16_t _count;			// Samples in the window, up to _size

	// Private Member Variable
	float* __p_buffer;
	uint16_t __index;
	float __sum;
} PifMovingAverage;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMovingAverage_Init
 * @brief Initializes the filter with an empty window.
 * @param p_owner Pointer to the target object instance.
 * @param p_buffer Buffer of size floats. It must stay valid while the filter is used.
 * @param size Window length in samples, 1 or more.
 * @return TRUE if successful, FALSE if p_buffer is NULL or size is 0.
 */
BOOL pifMovingAverage_Init(PifMovingAverage* p_owner, float* p_buffer, uint16_t size);

/**
 * @fn pifMovingAverage_Reset
 * @brief Empties the window.
 * @param p_owner Pointer to the target object instance.
 */
void pifMovingAverage_Reset(PifMovingAverage* p_owner);

/**
 * @fn pifMovingAverage_Apply
 * @brief Adds a sample, dropping the oldest when the window is full.
 * @param p_owner Pointer to the target object instance.
 * @param input Input sample.
 * @return Average of the samples in the window. Until the window is full, this is
 *         the average of the samples added so far.
 */
float pifMovingAverage_Apply(PifMovingAverage* p_owner, float input);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOVING_AVERAGE_H
