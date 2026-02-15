#ifndef PIF_NOISE_FILTER_BIT_H
#define PIF_NOISE_FILTER_BIT_H


#include "filter/pif_noise_filter_manager.h"


/**
 * @class StPifNfBit
 * @brief Bit-oriented noise filter state for digital inputs.
 *
 * The filter keeps a bit history window and computes a debounced output
 * using either majority count mode or continuous-state correction mode.
 */
typedef struct StPifNfBit
{
	// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

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
 * @brief Adds a bit filter that outputs the majority value in the history window.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of bits in the history window. Valid range is 3 to 31.
 * @return Pointer to the created filter instance, or NULL if allocation fails
 *         or parameters are invalid.
 */
PifNoiseFilter* pifNoiseFilterBit_AddCount(PifNoiseFilterManager* p_manager, uint8_t size);

/**
 * @fn pifNoiseFilterBit_AddContinue
 * @brief Adds a bit filter that suppresses short glitches in state transitions.
 * @param p_manager Pointer to an initialized noise filter manager.
 * @param size Number of bits in the history window. Valid range is 3 to 31.
 * @return Pointer to the created filter instance, or NULL if allocation fails
 *         or parameters are invalid.
 */
PifNoiseFilter* pifNoiseFilterBit_AddContinue(PifNoiseFilterManager* p_manager, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_BIT_H
