#ifndef PIF_NOISE_FILTER_INT32_H
#define PIF_NOISE_FILTER_INT32_H


#include "filter/pif_noise_filter_manager.h"


/**
 * @class StPifNfInt32Common
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
 * @param p_manager
 * @param size
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt32_AddAverage(PifNoiseFilterManager* p_manager, uint8_t size);

/**
 * @fn pifNoiseFilterInt32_AddWeightFactor
 * @brief
 * @param p_manager
 * @param size
 * @param weight_factor...
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt32_AddWeightFactor(PifNoiseFilterManager* p_manager, uint8_t size, ...);

/**
 * @fn pifNoiseFilterInt32_AddNoiseCancel
 * @brief
 * @param p_manager
 * @param size
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt32_AddNoiseCancel(PifNoiseFilterManager* p_manager, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT32_H
