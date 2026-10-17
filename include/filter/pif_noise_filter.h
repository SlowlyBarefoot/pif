// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NOISE_FILTER_H
#define PIF_NOISE_FILTER_H


#include "core/pif.h"


typedef enum EnPifNoiseFilterType
{
	NFT_AVERAGE,
	NFT_MEDIAN,
	NFT_EMA,
	NFT_DEADBAND,

	NFT_BIT_COUNT,
	NFT_BIT_CONTINUE,
	NFT_BIT_INTEGRATOR
} PifNoiseFilterType;

/**
 * @brief Type of the value a filter takes and gives. Users of a filter check it when the filter is attached.
 */
typedef enum EnPifNoiseFilterValue
{
	NFV_SWITCH,
	NFV_INT16,
	NFV_INT32
} PifNoiseFilterValue;


// Number of samples in the buffer of a median filter with a window of SIZE samples.
#define PIF_NF_MEDIAN_BUFFER_SIZE(SIZE)	((SIZE) * 2)


typedef void* PifNoiseFilterValueP;

struct StPifNoiseFilter;
typedef struct StPifNoiseFilter PifNoiseFilter;

typedef void (*PifNoiseFilter_Reset)(PifNoiseFilter* p_parent);
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
	PifNoiseFilterType _type;
	PifNoiseFilterValue _value;		// Type of the value taken and given
	BOOL _ready;					// The window is full. Results before it come from the samples so far.

	// Private Member Variable

	// Private Function
	PifNoiseFilter_Reset __fn_reset;
	PifNoiseFilter_Process __fn_process;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilter_Reset
 * @brief Empties the window of a filter.
 * @param p_parent Pointer to the filter instance to reset.
 */
PIF_STATIC_INLINE void pifNoiseFilter_Reset(PifNoiseFilter* p_parent)
{
	(*p_parent->__fn_reset)(p_parent);
}

/**
 * @fn pifNoiseFilter_Process
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
 * @fn pifNoiseFilter_Check
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
