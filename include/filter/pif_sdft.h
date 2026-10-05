// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SDFT_H
#define PIF_SDFT_H


#include "core/pif.h"


/*
 * Sliding DFT: the DFT of the last window_size samples, kept up to date one sample at a time for a
 * range of bins, at a cost of one complex multiply per bin and sample.
 *
 * Each new sample x(n) updates bin k as
 *     X_k(n) = W_k * (r * X_k(n-1) + x(n) - r^N * x(n-N)),   W_k = e^(j 2 pi k / N)
 * from E. Jacobsen and R. Lyons, "The Sliding DFT", IEEE Signal Processing Magazine, March 2003.
 * The damping r slightly below 1 keeps float rounding from building up; r = 1 is the exact DFT.
 *
 * The update of one sample can be split: pifSdft_PushSample() takes the sample, and
 * pifSdft_UpdateBins() then updates the bins in as many parts as suit the caller, as long as every
 * bin is updated before the next sample.
 */


/**
 * @class StPifSdftBin
 * @brief State of one bin: its value and its twiddle factor.
 */
typedef struct StPifSdftBin
{
	float re, im;				// X_k
	float tw_re, tw_im;			// W_k
} PifSdftBin;

/**
 * @class StPifSdft
 * @brief Sliding DFT over caller-provided sample and bin buffers.
 */
typedef struct StPifSdft
{
	// Read-only Member Variable
	uint16_t _window_size;		// N
	uint16_t _start_bin;		// First bin computed
	uint16_t _bin_count;		// Bins computed, from _start_bin
	uint32_t _sample_count;		// Samples pushed, up to 0xFFFFFFFF

	// Private Member Variable
	float* __p_samples;
	PifSdftBin* __p_bins;
	float __damping;			// r
	float __damping_n;			// r^N
	float __delta;				// x(n) - r^N * x(n-N) of the last sample
	uint16_t __index;			// Oldest sample in __p_samples
} PifSdft;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSdft_Init
 * @brief Initializes a sliding DFT with an empty (all zero) window.
 * @param p_owner Pointer to the instance.
 * @param window_size N, 2 or more. Bin k is at k * sample rate / N.
 * @param p_samples Buffer of window_size floats.
 * @param start_bin First bin to compute.
 * @param bin_count Number of bins to compute; start_bin + bin_count must not exceed window_size / 2 + 1.
 * @param p_bins Buffer of bin_count bins.
 * @param damping r, above 0 and at most 1. 0.9999 suits float.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSdft_Init(PifSdft* p_owner, uint16_t window_size, float* p_samples, uint16_t start_bin, uint16_t bin_count,
		PifSdftBin* p_bins, float damping);

/**
 * @fn pifSdft_Reset
 * @brief Empties the window.
 * @param p_owner Pointer to the instance.
 */
void pifSdft_Reset(PifSdft* p_owner);

/**
 * @fn pifSdft_PushSample
 * @brief Adds a sample to the window. The bins take it in with pifSdft_UpdateBins().
 * @param p_owner Pointer to the instance.
 * @param sample New sample.
 */
void pifSdft_PushSample(PifSdft* p_owner, float sample);

/**
 * @fn pifSdft_UpdateBins
 * @brief Takes the last pushed sample into some of the bins.
 * @param p_owner Pointer to the instance.
 * @param first Index of the first bin to update, 0 for _start_bin.
 * @param count Number of bins, clipped to the bins computed.
 */
void pifSdft_UpdateBins(PifSdft* p_owner, uint16_t first, uint16_t count);

/**
 * @fn pifSdft_Push
 * @brief Adds a sample and updates every bin.
 * @param p_owner Pointer to the instance.
 * @param sample New sample.
 */
void pifSdft_Push(PifSdft* p_owner, float sample);

/**
 * @fn pifSdft_GetPower
 * @brief Returns the squared magnitude of a bin.
 * @param p_owner Pointer to the instance.
 * @param index Index of the bin, 0 for _start_bin.
 * @return |X_k|^2.
 */
float pifSdft_GetPower(const PifSdft* p_owner, uint16_t index);

/**
 * @fn pifSdft_GetHannPower
 * @brief Returns the squared magnitude of a bin as if the window had been Hann-windowed, which
 *        keeps a strong peak from leaking far into the bins around it. It is formed from the bin
 *        and its two neighbours, 0.5 X_k - 0.25 (X_k-1 + X_k+1), so it exists for every bin
 *        computed except the first and the last.
 * @param p_owner Pointer to the instance.
 * @param index Index of the bin, 1 to _bin_count - 2.
 * @return Windowed |X_k|^2, or 0 for an index without both neighbours.
 */
float pifSdft_GetHannPower(const PifSdft* p_owner, uint16_t index);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SDFT_H
