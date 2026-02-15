#ifndef PIF_TIMER_MANAGER_H
#define PIF_TIMER_MANAGER_H


#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"
#include "core/pif_timer.h"


/**
 * @struct StPifTimerManager
 * @brief Represents the timer manager data structure used by this module.
 */
typedef struct StPifTimerManager
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	uint32_t _period1us;

	// Private Member Variable
    PifObjArray __timers;
	PifTask *__p_task;
} PifTimerManager;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTimerManager_Init
 * @brief Initializes the timer manager instance and prepares all internal fields for safe use.
 * @param p_manager Pointer to the timer manager instance.
 * @param id Identifier value for the object or task.
 * @param period1us Tick period in microseconds for timer updates.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimerManager_Init(PifTimerManager *p_manager, PifId id, uint32_t period1us, int max_count);

/**
 * @fn pifTimerManager_Clear
 * @brief Clears the timer manager state and releases resources currently owned by the instance.
 * @param p_manager Pointer to the timer manager instance.
 */
void pifTimerManager_Clear(PifTimerManager *p_manager);

/**
 * @fn pifTimerManager_Add
 * @brief Adds an item to the timer manager and updates internal bookkeeping for subsequent operations.
 * @param p_manager Pointer to the timer manager instance.
 * @param type Timer type configuration value.
 * @return Return value of this API.
 */
PifTimer *pifTimerManager_Add(PifTimerManager *p_manager, PifTimerType type);

/**
 * @fn pifTimerManager_Remove
 * @brief Removes an item from the timer manager and updates internal bookkeeping for consistency.
 * @param p_timer Pointer to the timer instance.
 */
void pifTimerManager_Remove(PifTimer *p_timer);

/**
 * @fn pifTimerManager_Count
 * @brief Returns the current number of valid items managed by the timer manager.
 * @param p_manager Pointer to the timer manager instance.
 * @return Return value of this API.
 */
int pifTimerManager_Count(PifTimerManager *p_manager);

/**
 * @fn pifTimerManager_sigTick
 * @brief Processes an external signal or tick for the timer manager and updates runtime timing state.
 * @param p_manager Pointer to the timer manager instance.
 */
void pifTimerManager_sigTick(PifTimerManager *p_manager);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_MANAGER_H
