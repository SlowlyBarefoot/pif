#ifndef PIF_TASK_MANAGER_H
#define PIF_TASK_MANAGER_H


#include "core/pif_task.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTaskManager_Init
 * @brief Initializes the task manager instance and prepares all internal fields for safe use.
 * @param max_count Maximum number of elements to manage.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTaskManager_Init(int max_count);

/**
 * @fn pifTaskManager_Clear
 * @brief Clears the task manager state and releases resources currently owned by the instance.
 */
void pifTaskManager_Clear();

/**
 * @fn pifTaskManager_Add
 * @brief Adds an item to the task manager and updates internal bookkeeping for subsequent operations.
 * @param id Identifier value for the object or task.
 * @param mode Operating mode configuration value.
 * @param period Execution period value for scheduling.
 * @param evt_loop Task loop callback executed by the scheduler.
 * @param p_client User-defined context pointer owned by the caller.
 * @param start Set to TRUE to start the task immediately after registration.
 * @return Pointer to the resulting object or data, or NULL if unavailable.
 */
PifTask *pifTaskManager_Add(PifId id, PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void *p_client, BOOL start);

/**
 * @fn pifTaskManager_Remove
 * @brief Removes an item from the task manager and updates internal bookkeeping for consistency.
 * @param p_task Pointer to the task object.
 */
void pifTaskManager_Remove(PifTask *p_task);

/**
 * @fn pifTaskManager_Count
 * @brief Returns the current number of valid items managed by the task manager.
 * @return Result value returned by this API.
 */
int pifTaskManager_Count();

/**
 * @fn pifTaskManager_CurrentTask
 * @brief Returns the current number of valid items managed by the task manager.
 * @return Return value of this API.
 */
PifTask *pifTaskManager_CurrentTask();

/**
 * @fn pifTaskManager_Loop
 * @brief Runs one scheduling loop iteration and dispatches eligible tasks in the manager.
 */
void pifTaskManager_Loop();

/**
 * @fn pifTaskManager_Yield
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 */
void pifTaskManager_Yield();

/**
 * @fn pifTaskManager_YieldMs
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 * @param time Time value used by yield or delay operations.
 */
void pifTaskManager_YieldMs(uint32_t time);

/**
 * @fn pifTaskManager_YieldUs
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 * @param time Time value used by yield or delay operations.
 */
void pifTaskManager_YieldUs(uint32_t time);

/**
 * @fn pifTaskManager_YieldAbort
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 * @param p_check_abort Callback that returns TRUE when waiting should abort.
 * @param p_issuer User context pointer passed to callbacks.
 */
void pifTaskManager_YieldAbort(PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_YieldAbortMs
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 * @param time Time value used by yield or delay operations.
 * @param p_check_abort Callback that returns TRUE when waiting should abort.
 * @param p_issuer User context pointer passed to callbacks.
 */
void pifTaskManager_YieldAbortMs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_YieldAbortUs
 * @brief Yields execution from the current task context so other schedulable tasks can run.
 * @param time Time value used by yield or delay operations.
 * @param p_check_abort Callback that returns TRUE when waiting should abort.
 * @param p_issuer User context pointer passed to callbacks.
 */
void pifTaskManager_YieldAbortUs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_AllTask
 * @brief Executes the pifTaskManager_AllTask operation for the task manager module according to the API contract.
 * @param callback Callback function invoked for each task entry.
 */
void pifTaskManager_AllTask(void (*callback)(PifTask *p_task));

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

/**
 * @fn pifTaskManager_Print
 * @brief Formats and writes output related to the task manager using the provided destination.
 */
void pifTaskManager_Print();

#endif	// PIF_NO_LOG

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_MANAGER_H
