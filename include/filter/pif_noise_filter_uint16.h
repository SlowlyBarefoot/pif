#ifndef PIF_NOISE_FILTER_UINT16_H
#define PIF_NOISE_FILTER_UINT16_H


#include "filter/pif_noise_filter.h"


/**
 * @class StPifNoiseFilterAverage
 * @brief
 */
typedef struct StPifNoiseFilterAverage
{
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
 * @class StPifNoiseFilterUint16
 * @brief
 */
typedef struct StPifNoiseFilterUint16
{
	PifNoiseFilter parent;

	// Public Member Variable

	// Read-only Member Variable
	int8_t _size;
	uint16_t _result;

	// Private Member Variable
	uint16_t* __buffer;
	int8_t __current;
	union {
		PifNoiseFilterAverage __avg;
		PifNoiseFilterWeightFactor __wf;
		PifNoiseFilterNoiseCancel __nc;
	};
} PifNoiseFilterUint16;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterUint16_Init
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifNoiseFilterUint16_Init(PifNoiseFilterUint16* p_owner, uint8_t size);

/**
 * @fn pifNoiseFilterUint16_Clear
 * @brief
 * @param p_owner
 */
void pifNoiseFilterUint16_Clear(PifNoiseFilterUint16* p_owner);

/**
 * @fn pifNoiseFilterUint16_SetWeightFactor
 * @brief
 * @param p_owner
 * @param weight_factor...
 * @return
 */
BOOL pifNoiseFilterUint16_SetWeightFactor(PifNoiseFilterUint16* p_owner, ...);

/**
 * @fn pifNoiseFilterUint16_SetNoiseCancel
 * @brief
 * @param p_owner
 * @param diff_max
 * @param sum_min
 * @return
 */
BOOL pifNoiseFilterUint16_SetNoiseCancel(PifNoiseFilterUint16* p_owner, uint16_t diff_max, uint16_t sum_min);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_UINT16_H
