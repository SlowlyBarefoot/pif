#ifndef PIF_NOISE_FILTER_MANAGER_H
#define PIF_NOISE_FILTER_MANAGER_H


#include "core/pif_ptr_array.h"
#include "filter/pif_noise_filter.h"


/**
 * @class StPifNoiseFilterManager
 * @brief Container and lifecycle manager for multiple noise filter instances.
 *
 * The manager owns filter objects through an internal pointer array and
 * releases them using the registered clear callback.
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
 * @brief Initializes a noise filter manager with fixed capacity.
 * @param p_owner Pointer to the manager instance to initialize.
 * @param count Maximum number of filters that can be registered.
 * @return TRUE if initialization succeeds, FALSE otherwise.
 */
BOOL pifNoiseFilterManager_Init(PifNoiseFilterManager* p_owner, uint8_t count);

/**
 * @fn pifNoiseFilterManager_Clear
 * @brief Clears and deallocates all filters owned by the manager.
 * @param p_owner Pointer to an initialized manager instance.
 */
void pifNoiseFilterManager_Clear(PifNoiseFilterManager* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOISE_FILTER_MANAGER_H
