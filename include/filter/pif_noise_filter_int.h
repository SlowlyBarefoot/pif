// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_noise_filter_int.h
 * @brief Template of the integer noise filters: trimmed average, median, EMA and deadband.
 *
 * Template of the integer noise filters, shared by pif_noise_filter_int16.h and pif_noise_filter_int32.h. Each of
 * them includes it with PIF_NF_INT_BITS set to its width and PIF_NF_INT_WIDE_T to a type twice as wide, and it
 * is not included directly.
 *
 * For a width of N bits it declares, with int<N>_t samples:
 *   PifNfInt<N>Average, pifNoiseFilterInt<N>_InitAverage()
 *   PifNfInt<N>Median, pifNoiseFilterInt<N>_InitMedian()
 *   PifNfInt<N>Ema, pifNoiseFilterInt<N>_InitEma()
 *   PifNfInt<N>Deadband, pifNoiseFilterInt<N>_InitDeadband()
 */

#if !defined(PIF_NF_INT_BITS) || !defined(PIF_NF_INT_WIDE_T)
	#error "Include pif_noise_filter_int16.h or pif_noise_filter_int32.h instead."
#endif


#include "filter/pif_noise_filter.h"


#define PIF_NF_INT_CAT_(A, B, C)	A##B##C	///< Helper of PIF_NF_INT_CAT
#define PIF_NF_INT_CAT(A, B, C)		PIF_NF_INT_CAT_(A, B, C)	///< Joins three tokens after expanding them

#define PIF_NF_INT_T				PIF_NF_INT_CAT(int, PIF_NF_INT_BITS, _t)	///< Sample type, int<N>_t
#define PIF_NF_INT_AVERAGE			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Average)	///< Type of the average filter of this width
#define PIF_NF_INT_MEDIAN			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Median)	///< Type of the median filter of this width
#define PIF_NF_INT_EMA				PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Ema)	///< Type of the EMA filter of this width
#define PIF_NF_INT_DEADBAND			PIF_NF_INT_CAT(PifNfInt, PIF_NF_INT_BITS, Deadband)	///< Type of the deadband filter of this width


/**
 * @brief Trimmed-average filter state. It takes and gives int<N>_t (NFV_INT<N>).
 *
 * The algorithm stores a fixed-size window and computes an average after
 * excluding the minimum and maximum sample values.
 */
typedef struct PIF_NF_INT_CAT(StPifNfInt, PIF_NF_INT_BITS, Average)
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	uint8_t _size;	///< Number of samples in the window
	PIF_NF_INT_T _result;	///< Filtered output

	// Private Member Variable
	uint8_t __current;
	uint8_t __len;
	PIF_NF_INT_T* __p_buffer;
} PIF_NF_INT_AVERAGE;

/**
 * @brief Moving median filter state. It takes and gives int<N>_t (NFV_INT<N>).
 *
 * Spikes shorter than half the window are taken out entirely, and steps pass with a delay of half the window.
 * Each sample costs O(size).
 */
typedef struct PIF_NF_INT_CAT(StPifNfInt, PIF_NF_INT_BITS, Median)
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	uint8_t _size;					///< Number of samples in the window
	PIF_NF_INT_T _result;			///< Filtered output

	// Private Member Variable
	uint8_t __current;
	uint8_t __len;
	PIF_NF_INT_T* __p_buffer;		///< The window in arrival order, then the same samples in sorted order
} PIF_NF_INT_MEDIAN;

/**
 * @brief Exponential moving average filter state. It takes and gives int<N>_t (NFV_INT<N>).
 *
 * Each sample moves the result by 1 / 2^_shift of its distance from it, so the time constant is about 2^_shift
 * samples. It needs no buffer and only adds and shifts.
 */
typedef struct PIF_NF_INT_CAT(StPifNfInt, PIF_NF_INT_BITS, Ema)
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	uint8_t _shift;					///< Weight of a sample as 1 / 2^_shift
	PIF_NF_INT_T _result;			///< Filtered output

	// Private Member Variable
	PIF_NF_INT_WIDE_T __acc;		///< The result times 2^_shift, so that the fraction is kept
} PIF_NF_INT_EMA;

/**
 * @brief Deadband filter state. It takes and gives int<N>_t (NFV_INT<N>).
 *
 * The result follows a sample only when it is _band or more away from the result, so that a level jittering by
 * less than _band keeps one value.
 */
typedef struct PIF_NF_INT_CAT(StPifNfInt, PIF_NF_INT_BITS, Deadband)
{
	/// The parent variable must be at the beginning of this structure.
	PifNoiseFilter parent;

	// Read-only Member Variable
	PIF_NF_INT_T _band;	///< Smallest change that passes
	PIF_NF_INT_T _result;	///< Filtered output
} PIF_NF_INT_DEADBAND;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a trimmed-average filter with an empty window.
 *
 * Until the window is full, the result is the trimmed average of the samples so far, or their plain average while
 * there are fewer than 3.
 * @param p_owner Pointer to the filter instance.
 * @param p_buffer Buffer of size samples. It must stay valid while the filter is used.
 * @param size Number of samples in the moving window. Must be 3 or more.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitAverage)(PIF_NF_INT_AVERAGE* p_owner,
		PIF_NF_INT_T* p_buffer, uint8_t size);

/**
 * @brief Initializes a moving median filter with an empty window.
 *
 * Until the window is full, the result is the median of the samples so far. The median of an even count is the
 * average of the two middle samples.
 * @param p_owner Pointer to the filter instance.
 * @param p_buffer Buffer of PIF_NF_MEDIAN_BUFFER_SIZE(size) samples. It must stay valid while the filter is used.
 * @param size Number of samples in the moving window. Must be 3 or more, and odd for a median that is a sample.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitMedian)(PIF_NF_INT_MEDIAN* p_owner,
		PIF_NF_INT_T* p_buffer, uint8_t size);

/**
 * @brief Initializes an exponential moving average filter.
 *
 * The first sample after the init or a reset is the result as it is, and the filter is _ready from then on.
 * @param p_owner Pointer to the filter instance.
 * @param shift Weight of a sample as 1 / 2^shift. Valid range is 1 to N - 1.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitEma)(PIF_NF_INT_EMA* p_owner, uint8_t shift);

/**
 * @brief Initializes a deadband filter.
 *
 * The first sample after the init or a reset is the result as it is, and the filter is _ready from then on.
 * @param p_owner Pointer to the filter instance.
 * @param band Smallest change that passes. Must be 1 or more; 1 passes every change.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL PIF_NF_INT_CAT(pifNoiseFilterInt, PIF_NF_INT_BITS, _InitDeadband)(PIF_NF_INT_DEADBAND* p_owner,
		PIF_NF_INT_T band);

#ifdef __cplusplus
}
#endif


// Undefined so that the other width can be included as well.
#undef PIF_NF_INT_CAT_
#undef PIF_NF_INT_CAT
#undef PIF_NF_INT_T
#undef PIF_NF_INT_AVERAGE
#undef PIF_NF_INT_MEDIAN
#undef PIF_NF_INT_EMA
#undef PIF_NF_INT_DEADBAND
