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
 * @brief
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
 * @brief
 * @param p_parent
 */
void pifNoiseFilter_Reset(PifNoiseFilter* p_parent);

/**
 * @fn pifNoiseFilter_Process
 * @brief
 * @param p_parent
 * @param p_value
 * @return
 */
PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_H
