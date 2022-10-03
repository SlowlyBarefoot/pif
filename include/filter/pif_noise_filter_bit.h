#ifndef PIF_NOISE_FILTER_BIT_H
#define PIF_NOISE_FILTER_BIT_H


#include "filter/pif_noise_filter.h"


/**
 * @class StPifNoiseFilterBit
 * @brief
 */
typedef struct StPifNoiseFilterBit
{
	PifNoiseFilter parent;

	// Public Member Variable

	// Read-only Member Variable
	int8_t _size;
	SWITCH _result;

	// Private Member Variable
    uint8_t __half;
    uint32_t __msb;
    uint8_t __count;
    uint32_t __list;
} PifNoiseFilterBit;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterBit_Init
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifNoiseFilterBit_Init(PifNoiseFilterBit* p_owner, uint8_t size);

/**
 * @fn pifNoiseFilterBit_SetContinue
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifNoiseFilterBit_SetContinue(PifNoiseFilterBit* p_owner, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_BIT_H
