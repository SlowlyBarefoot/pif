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

typedef PifNoiseFilterValueP (*PifNoiseFiler_Process)(PifNoiseFilter* p_owner, PifNoiseFilterValueP p_value);


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
	PifNoiseFiler_Process __fn_process;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilter_Init
 * @brief
 * @param p_owner
 * @param type
 * @return
 */
BOOL pifNoiseFilter_Init(PifNoiseFilter* p_owner, PifNoiseFilterType type);

/**
 * @fn pifNoiseFilter_Process
 * @brief
 * @param p_owner
 * @param p_value
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, PifNoiseFilterValueP p_value);
#else
	inline PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, PifNoiseFilterValueP p_value) {
		return (*p_owner->__fn_process)(p_owner, p_value);
	}
#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_H
