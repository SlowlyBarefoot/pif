// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_noise_filter_bit.h
 * @brief Noise filters for digital inputs that take and give SWITCH: majority, glitch removal and integrating debounce.
 */
#ifndef PIF_NOISE_FILTER_BIT_H
#define PIF_NOISE_FILTER_BIT_H


#include "filter/pif_noise_filter.h"


/**
 * @class StPifNfBit
 * @brief Bit-oriented noise filter state for digital inputs. It takes and gives SWITCH.
 *
 * The filter keeps a bit history window and computes a debounced output
 * using either majority count mode or continuous-state correction mode.
 */
typedef struct StPifNfBit
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	uint8_t _size;	///< Number of bits in the history window
	SWITCH _result;	///< Filtered output

	// Private Member Variable
	uint8_t __half;
	uint8_t __len;
	uint8_t __count;
	uint32_t __msb;
	uint32_t __list;
} PifNfBit;

/**
 * @class StPifNfBitIntegrator
 * @brief Integrating debounce filter state for digital inputs. It takes and gives SWITCH.
 *
 * A counter goes up on ON and down on OFF, and the output turns ON after _on_count more ON samples than OFF ones,
 * and OFF after _off_count more OFF samples than ON ones. A bounce only delays the change instead of starting it
 * over, and it needs no window, so a debounce time is not limited to 31 samples.
 */
typedef struct StPifNfBitIntegrator
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	uint16_t _on_count;		///< Net ON samples that turn the output ON
	uint16_t _off_count;	///< Net OFF samples that turn the output OFF
	SWITCH _result;			///< Filtered output

	// Private Member Variable
	uint16_t __count;		///< 0 to _on_count while OFF, and _off_count down to 0 while ON
} PifNfBitIntegrator;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a bit filter that outputs the majority value in the history window.
 * @param p_owner Pointer to the filter instance.
 * @param size Number of bits in the history window. Valid range is 3 to 31. An odd size gives a strict majority;
 *             with an even size a tie gives OFF.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifNoiseFilterBit_InitCount(PifNfBit* p_owner, uint8_t size);

/**
 * @brief Initializes a bit filter that suppresses short glitches in state transitions.
 *
 * A run of size / 2 samples or fewer that is broken by the opposite state is taken as a glitch and rewritten to
 * that state. The output is the sample size / 2 steps back, so it lags the input by that many samples.
 * @param p_owner Pointer to the filter instance.
 * @param size Number of bits in the history window. Valid range is 3 to 31.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifNoiseFilterBit_InitContinue(PifNfBit* p_owner, uint8_t size);

/**
 * @brief Initializes an integrating debounce filter. The output starts OFF, and the filter is _ready from the first
 *        sample.
 * @param p_owner Pointer to the filter instance.
 * @param on_count Net ON samples that turn the output ON, 1 or more.
 * @param off_count Net OFF samples that turn the output OFF, 1 or more. It may differ from on_count, for example to
 *                  take a press quickly and a release slowly.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifNoiseFilterBit_InitIntegrator(PifNfBitIntegrator* p_owner, uint16_t on_count, uint16_t off_count);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_BIT_H
