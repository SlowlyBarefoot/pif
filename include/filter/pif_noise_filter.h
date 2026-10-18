// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_noise_filter.h
 * @brief Base of the noise filters that sensors and inputs pass their samples through.
 *
 * A filter is a structure of the caller's that starts with a PifNoiseFilter, initialized by the Init function of
 * its kind. The sensor modules take a PifNoiseFilter pointer, so any kind of filter fits them as long as its
 * value type matches.
 */
#ifndef PIF_NOISE_FILTER_H
#define PIF_NOISE_FILTER_H


#include "core/pif.h"


/**
 * @brief Kind of a noise filter.
 */
typedef enum EnPifNoiseFilterType
{
	NFT_AVERAGE,	///< Trimmed average of a window, pif_noise_filter_int16.h or pif_noise_filter_int32.h
	NFT_MEDIAN,	///< Moving median of a window
	NFT_EMA,	///< Exponential moving average
	NFT_DEADBAND,	///< Passes only changes of a given size or more

	NFT_BIT_COUNT,	///< Majority of a bit window, pif_noise_filter_bit.h
	NFT_BIT_CONTINUE,	///< Glitches of a bit window removed
	NFT_BIT_INTEGRATOR	///< Integrating debounce of a bit
} PifNoiseFilterType;

/**
 * @brief Type of the value a filter takes and gives. Users of a filter check it when the filter is attached.
 */
typedef enum EnPifNoiseFilterValue
{
	NFV_SWITCH,	///< SWITCH, from pif_noise_filter_bit.h
	NFV_INT16,	///< int16_t, from pif_noise_filter_int16.h
	NFV_INT32	///< int32_t, from pif_noise_filter_int32.h
} PifNoiseFilterValue;


/// Number of samples in the buffer of a median filter with a window of SIZE samples.
#define PIF_NF_MEDIAN_BUFFER_SIZE(SIZE)	((SIZE) * 2)


/**
 * @brief Pointer to a value of the type the filter takes and gives.
 */
typedef void* PifNoiseFilterValueP;

struct StPifNoiseFilter;
typedef struct StPifNoiseFilter PifNoiseFilter;

/**
 * @brief Clears the history of a filter. Implemented by each kind.
 */
typedef void (*PifNoiseFilter_Reset)(PifNoiseFilter* p_parent);
/**
 * @brief Takes one sample and returns a pointer to the result. Implemented by each kind.
 */
typedef PifNoiseFilterValueP (*PifNoiseFilter_Process)(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value);

/**
 * @class StPifNoiseFilter
 * @brief Base interface for all noise filter implementations.
 *
 * Each filter is a structure of the caller's that starts with this one, initialized by its own Init function. It
 * allocates nothing, so it can be static, global or on the stack.
 */
struct StPifNoiseFilter
{
	// Public Member Variable

	// Read-only Member Variable
	PifNoiseFilterType _type;		///< Kind of the filter
	PifNoiseFilterValue _value;		///< Type of the value taken and given
	BOOL _ready;					///< The window is full. Results before it come from the samples so far.

	// Private Member Variable

	// Private Function
	PifNoiseFilter_Reset __fn_reset;
	PifNoiseFilter_Process __fn_process;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Empties the window of a filter.
 * @param p_parent Pointer to the filter instance to reset.
 */
PIF_STATIC_INLINE void pifNoiseFilter_Reset(PifNoiseFilter* p_parent)
{
	(*p_parent->__fn_reset)(p_parent);
}

/**
 * @brief Processes one input sample and updates the filter output.
 * @param p_parent Pointer to the filter instance.
 * @param p_value Pointer to the input value, of the type in _value.
 * @return Pointer to the filtered result, of the type in _value. It is never NULL: until the window is full
 *         (_ready), it comes from the samples given so far.
 */
PIF_STATIC_INLINE PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	return (*p_parent->__fn_process)(p_parent, p_value);
}

/**
 * @brief Checks that a filter takes and gives the type its user expects.
 * @param p_parent Pointer to the filter instance, or NULL for none.
 * @param value Type the user gives and takes.
 * @return TRUE when p_parent is NULL or of that type, otherwise FALSE with E_INVALID_PARAM.
 */
PIF_STATIC_INLINE BOOL pifNoiseFilter_Check(PifNoiseFilter* p_parent, PifNoiseFilterValue value)
{
	if (p_parent && p_parent->_value != value) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_H
