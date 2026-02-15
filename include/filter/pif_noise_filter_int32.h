#ifndef PIF_NOISE_FILTER_INT32_H
#define PIF_NOISE_FILTER_INT32_H


#include "filter/pif_noise_filter_manager.h"


/**
 * @class StPifNfInt32Common
 * @brief Shared runtime fields used by 32-bit noise filters.
 */
typedef struct StPifNfInt32Common
{
	uint8_t size;
	uint8_t current;
	int32_t result;
	int32_t* p_buffer;
} PifNfInt32Common;

/**
 * @class StPifNfInt32Average
 * @brief 32-bit trimmed-average filter state.
 *
 * The algorithm stores a fixed-size window and computes an average after
 * excluding the minimum and maximum sample values.
 */
typedef struct StPifNfInt32Average
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt32Common common;

	uint8_t len;
} PifNfInt32Average;

/**
 * @class StPifNfInt32WeightFactor
 * @brief 32-bit weighted moving filter state.
 *
 * Each sample in the window is multiplied by a configured weight and divided
 * by the total weight sum.
 */
typedef struct StPifNfInt32WeightFactor
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt32Common common;

	int8_t* value;
	int16_t total;
} PifNfInt32WeightFactor;

/**
 * @class StPifNfInt32NoiseCancel
 * @brief 32-bit adaptive noise-cancel filter state.
 *
 * The filter tracks per-position differences and reduces transient spikes
 * before generating the resulting average value.
 */
typedef struct StPifNfInt32NoiseCancel
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt32Common common;

	int8_t before;
	int16_t* diff;
} PifNfInt32NoiseCancel;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterInt32_AddAverage
 * @brief Adds a 32-bit trimmed-average filter.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of samples in the moving window. Must be greater than 0.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt32_AddAverage(PifNoiseFilterManager* p_manager, uint8_t size);

/**
 * @fn pifNoiseFilterInt32_AddWeightFactor
 * @brief Adds a 32-bit weighted filter with variadic weight factors.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of samples and weight elements. Must be an odd value.
 * @param weight_factor... Sequence of signed integer weights, one per sample.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt32_AddWeightFactor(PifNoiseFilterManager* p_manager, uint8_t size, ...);

/**
 * @fn pifNoiseFilterInt32_AddNoiseCancel
 * @brief Adds a 32-bit noise-cancel filter.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Internal ring-buffer length. Valid range is 3 to 32.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt32_AddNoiseCancel(PifNoiseFilterManager* p_manager, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT32_H
