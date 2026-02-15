#ifndef PIF_NOISE_FILTER_INT16_H
#define PIF_NOISE_FILTER_INT16_H


#include "filter/pif_noise_filter_manager.h"


/**
 * @class StPifNfInt16Common
 * @brief Shared runtime fields used by 16-bit noise filters.
 */
typedef struct StPifNfInt16Common
{
	uint8_t size;
	uint8_t current;
	int16_t result;
	int16_t* p_buffer;
} PifNfInt16Common;

/**
 * @class StPifNfInt16Average
 * @brief 16-bit trimmed-average filter state.
 *
 * The algorithm stores a fixed-size window and computes an average after
 * excluding the minimum and maximum sample values.
 */
typedef struct StPifNfInt16Average
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt16Common common;

	uint8_t len;
} PifNfInt16Average;

/**
 * @class StPifNfInt16WeightFactor
 * @brief 16-bit weighted moving filter state.
 *
 * Each sample in the window is multiplied by a configured weight and divided
 * by the total weight sum.
 */
typedef struct StPifNfInt16WeightFactor
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt16Common common;

	int8_t* value;
	int16_t total;
} PifNfInt16WeightFactor;

/**
 * @class StPifNfInt16NoiseCancel
 * @brief 16-bit adaptive noise-cancel filter state.
 *
 * The filter tracks per-position differences and reduces transient spikes
 * before generating the resulting average value.
 */
typedef struct StPifNfInt16NoiseCancel
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;
	PifNfInt16Common common;

	int8_t before;
	int16_t* diff;
} PifNfInt16NoiseCancel;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterInt16_AddAverage
 * @brief Adds a 16-bit trimmed-average filter.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of samples in the moving window. Must be greater than 0.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt16_AddAverage(PifNoiseFilterManager* p_manager, uint8_t size);

/**
 * @fn pifNoiseFilterInt16_AddWeightFactor
 * @brief Adds a 16-bit weighted filter with variadic weight factors.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of samples and weight elements. Must be an odd value.
 * @param weight_factor... Sequence of signed integer weights, one per sample.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt16_AddWeightFactor(PifNoiseFilterManager* p_manager, uint8_t size, ...);

/**
 * @fn pifNoiseFilterInt16_AddNoiseCancel
 * @brief Adds a 16-bit noise-cancel filter.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Internal ring-buffer length. Valid range is 3 to 32.
 * @return Pointer to the created filter instance, or NULL on failure.
 */
PifNoiseFilter* pifNoiseFilterInt16_AddNoiseCancel(PifNoiseFilterManager* p_manager, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT16_H
