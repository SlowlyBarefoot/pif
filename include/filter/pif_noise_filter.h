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

struct StPifNoiseFilterMethod;
typedef struct StPifNoiseFilterMethod PifNoiseFilterMethod;

typedef void (*PifNoiseFiler_Clear)(PifNoiseFilterMethod* p_method);
typedef void (*PifNoiseFiler_Reset)(PifNoiseFilterMethod* p_method);
typedef PifNoiseFilterValueP (*PifNoiseFiler_Process)(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value);

/**
 * @class StPifNoiseFilterMethod
 * @brief
 */
struct StPifNoiseFilterMethod
{
	PifNoiseFilterType type;

	PifNoiseFiler_Clear fn_clear;
	PifNoiseFiler_Reset fn_reset;
	PifNoiseFiler_Process fn_process;
};


/**
 * @class StPifNoiseFilter
 * @brief
 */
struct StPifNoiseFilter
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t _count;
	uint8_t _last;

	// Private Member Variable

	// Private Function
	PifNoiseFilterMethod** __p_method;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilter_Init
 * @brief
 * @param p_owner
 * @param uint8_t count
 * @return
 */
BOOL pifNoiseFilter_Init(PifNoiseFilter* p_owner, uint8_t count);

/**
 * @fn pifNoiseFilter_Clear
 * @brief
 * @param p_owner
 */
void pifNoiseFilter_Clear(PifNoiseFilter* p_owner);

/**
 * @fn pifNoiseFilter_Reset
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	void pifNoiseFilter_Reset(PifNoiseFilter* p_owner, uint8_t index);
#else
	inline void pifNoiseFilter_Reset(PifNoiseFilter* p_owner, uint8_t index) {
		(*p_owner->__p_method[index]->fn_reset)(p_owner->__p_method[index]);
	}
#endif

/**
 * @fn pifNoiseFilter_Process
 * @brief
 * @param p_owner
 * @param index
 * @param p_value
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, uint8_t index, PifNoiseFilterValueP p_value);
#else
	inline PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, uint8_t index, PifNoiseFilterValueP p_value) {
		return (*p_owner->__p_method[index]->fn_process)(p_owner->__p_method[index], p_value);
	}
#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_H
