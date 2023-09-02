#ifndef PIF_NOISE_FILTER_MANAGER_H
#define PIF_NOISE_FILTER_MANAGER_H


#include "core/pif_ptr_array.h"
#include "filter/pif_noise_filter.h"


/**
 * @class StPifNoiseFilterManager
 * @brief
 */
typedef struct StPifNoiseFilterManager
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t _count;

	// Private Member Variable

	// Private Function
	PifPtrArray __filters;
} PifNoiseFilterManager;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNoiseFilterManager_Init
 * @brief
 * @param p_owner
 * @param count
 * @return
 */
BOOL pifNoiseFilterManager_Init(PifNoiseFilterManager* p_owner, uint8_t count);

/**
 * @fn pifNoiseFilterManager_Clear
 * @brief
 * @param p_owner
 */
void pifNoiseFilterManager_Clear(PifNoiseFilterManager* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_MANAGER_H
