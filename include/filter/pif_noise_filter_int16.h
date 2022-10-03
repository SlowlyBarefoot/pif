#ifndef PIF_NOISE_FILTER_INT16_H
#define PIF_NOISE_FILTER_INT16_H


#include "filter/pif_noise_filter.h"


/**
 * @class StPifNoiseFilterAverage
 * @brief
 */
typedef struct StPifNoiseFilterAverage
{
	int8_t before;
	int32_t sum;
} PifNoiseFilterAverage;

/**
 * @class StPifNoiseFilterWeightFactor
 * @brief
 */
typedef struct StPifNoiseFilterWeightFactor
{
	int8_t* value;
	int16_t total;
} PifNoiseFilterWeightFactor;

/**
 * @class StPifNoiseFilterNoiseCancel
 * @brief
 */
typedef struct StPifNoiseFilterNoiseCancel
{
	int8_t before;
	int16_t* diff;
	uint32_t invalid;
	int16_t diff_max;
	int16_t sum_min;
} PifNoiseFilterNoiseCancel;

/**
 * @class StPifNoiseFilterInt16
 * @brief
 */
typedef struct StPifNoiseFilterInt16
{
	PifNoiseFilter parent;

	// Public Member Variable

	// Read-only Member Variable
	int8_t _size;
	int16_t _result;

	// Private Member Variable
	int16_t* __buffer;
	int8_t __current;
	union {
		PifNoiseFilterAverage __avg;
		PifNoiseFilterWeightFactor __wf;
		PifNoiseFilterNoiseCancel __nc;
	};
} PifNoiseFilterInt16;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterInt16_Init
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifNoiseFilterInt16_Init(PifNoiseFilterInt16* p_owner, uint8_t size);

/**
 * @fn pifNoiseFilterInt16_Clear
 * @brief
 * @param p_owner
 */
void pifNoiseFilterInt16_Clear(PifNoiseFilterInt16* p_owner);

/**
 * @fn pifNoiseFilterInt16_SetWeightFactor
 * @brief
 * @param p_owner
 * @param weight_factor...
 * @return
 */
BOOL pifNoiseFilterInt16_SetWeightFactor(PifNoiseFilterInt16* p_owner, ...);

/**
 * @fn pifNoiseFilterInt16_SetNoiseCancel
 * @brief
 * @param p_owner
 * @param diff_max
 * @param sum_min
 * @return
 */
BOOL pifNoiseFilterInt16_SetNoiseCancel(PifNoiseFilterInt16* p_owner, int16_t diff_max, int16_t sum_min);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT16_H
