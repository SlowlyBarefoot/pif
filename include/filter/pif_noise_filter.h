#ifndef PIF_NOISE_FILTER_H
#define PIF_NOISE_FILTER_H


#include "core/pif.h"


typedef enum EnPifNoiseFilterType
{
	NFT_AVERAGE,
	NFT_WEIGHT_FACTOR,
	NFT_NOISE_CANCEL,

	NFT_BIT_COUNT,
	NFT_BIT_CONTINUE
} PifNoiseFilterType;


typedef void* PifNoiseFilterValueP;

struct StPifNoiseFilter;
typedef struct StPifNoiseFilter PifNoiseFilter;

typedef void (*PifNoiseFiler_Clear)(PifNoiseFilter* p_parent);
typedef void (*PifNoiseFiler_Reset)(PifNoiseFilter* p_parent);
typedef PifNoiseFilterValueP (*PifNoiseFiler_Process)(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value);

/**
 * @class StPifNoiseFilter
 * @brief Base interface for all noise filter implementations.
 *
 * This structure stores the runtime filter type and internal function pointers
 * used by derived filter objects to clear, reset, and process samples.
 */
struct StPifNoiseFilter
{
	// Public Member Variable

	// Read-only Member Variable
	PifNoiseFilterType _type;

	// Private Member Variable

	// Private Function
	PifNoiseFiler_Clear __fn_clear;
	PifNoiseFiler_Reset __fn_reset;
	PifNoiseFiler_Process __fn_process;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilter_Reset
 * @brief Resets a filter instance to its initial state.
 * @param p_parent Pointer to the filter instance to reset.
 */
void pifNoiseFilter_Reset(PifNoiseFilter* p_parent);

/**
 * @fn pifNoiseFilter_Process
 * @brief Processes one input sample and updates the filter output.
 * @param p_parent Pointer to the filter instance.
 * @param p_value Pointer to the input value to process.
 * @return Pointer to the filtered result value, or NULL when output is not
 *         yet available for the current sample.
 */
PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_H
