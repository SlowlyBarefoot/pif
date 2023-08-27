#ifndef PIF_NOISE_FILTER_BIT_H
#define PIF_NOISE_FILTER_BIT_H


#include "filter/pif_noise_filter.h"


/**
 * @class StPifNfBit
 * @brief
 */
typedef struct StPifNfBit
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilterMethod parent;

	int8_t _size;
	SWITCH _result;
    uint8_t __half;
    uint32_t __msb;
    uint8_t __count;
    uint32_t __list;
} PifNfBit;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterBit_AddCount
 * @brief
 * @param p_parent
 * @param size
 * @return
 */
BOOL pifNoiseFilterBit_AddCount(PifNoiseFilter* p_parent, uint8_t size);

/**
 * @fn pifNoiseFilterBit_AddContinue
 * @brief
 * @param p_parent
 * @param size
 * @return
 */
BOOL pifNoiseFilterBit_AddContinue(PifNoiseFilter* p_parent, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_BIT_H
