#ifndef PIF_NOISE_FILTER_INT32_H
#define PIF_NOISE_FILTER_INT32_H


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
 * @class StPifNoiseFilterInt32
 * @brief
 */
typedef struct StPifNoiseFilterInt32
{
	PifNoiseFilter parent;

	// Public Member Variable

	// Read-only Member Variable
	int8_t _size;
	int32_t _result;

	// Private Member Variable
	int32_t* __buffer;
	int8_t __current;
	union {
		PifNoiseFilterAverage __avg;
		PifNoiseFilterWeightFactor __wf;
		PifNoiseFilterNoiseCancel __nc;
	};
} PifNoiseFilterInt32;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterInt32_Init
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifNoiseFilterInt32_Init(PifNoiseFilterInt32* p_owner, uint8_t size);

/**
 * @fn pifNoiseFilterInt32_Clear
 * @brief
 * @param p_owner
 */
void pifNoiseFilterInt32_Clear(PifNoiseFilterInt32* p_owner);

/**
 * @fn pifNoiseFilterInt32_SetWeightFactor
 * @brief
 * @param p_owner
 * @param weight_factor...
 * @return
 */
BOOL pifNoiseFilterInt32_SetWeightFactor(PifNoiseFilterInt32* p_owner, ...);

/**
 * @fn pifNoiseFilterInt32_SetNoiseCancel
 * @brief
 * @param p_owner
 * @param diff_max
 * @param sum_min
 * @return
 */
BOOL pifNoiseFilterInt32_SetNoiseCancel(PifNoiseFilterInt32* p_owner, uint16_t diff_max, uint16_t sum_min);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_INT32_H
