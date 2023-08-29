#ifndef PIF_NOISE_FILTER_INT16_H
#define PIF_NOISE_FILTER_INT16_H


#include "filter/pif_noise_filter_manager.h"


/**
 * @class StPifNfInt16Common
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
 * @param p_manager
 * @param size
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt16_AddAverage(PifNoiseFilterManager* p_manager, uint8_t size);

/**
 * @fn pifNoiseFilterInt16_AddWeightFactor
 * @brief
 * @param p_manager
 * @param size
 * @param weight_factor...
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt16_AddWeightFactor(PifNoiseFilterManager* p_manager, uint8_t size, ...);

/**
 * @fn pifNoiseFilterInt16_AddNoiseCancel
 * @brief
 * @param p_manager
 * @param size
 * @return
 */
PifNoiseFilter* pifNoiseFilterInt16_AddNoiseCancel(PifNoiseFilterManager* p_manager, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT16_H
