#ifndef PIF_TASK_MANAGER_H
#define PIF_TASK_MANAGER_H


#include "core/pif_task.h"


typedef void (*PifEvtTaskTimer)(void *p_client);

/**
 * @struct StPifTaskTimer
 * @brief Represents the task timer data structure used by this module.
 */
typedef struct StPifTaskTimer
{
	// Read-only Member Variable
    void *_p_client;
#ifdef PIF_USE_BLOCK_TIME
	PifBlockTime _block_time;	// longest run of the callback, the delay it can cause
#endif

	// Read-only Event Function
    PifEvtTaskTimer _evt_timer;
} PifTaskTimer;

typedef void (*PifEvtTaskIdle)(void);

#ifdef PIF_DEBUG

extern PifActTaskSignal pif_act_task_signal;

#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTaskManager_Init
 * @brief Initializes the task manager instance and prepares all internal fields for safe use.
 * @param max_count Maximum number of elements to manage.
 * @param timer_count Maximum number of timer elements to manage.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTaskManager_Init(int max_count, int timer_count);

/**
 * @fn pifTaskManager_Clear
 * @brief Clears the task manager state and releases resources currently owned by the instance.
 */
void pifTaskManager_Clear();

/**
 * @fn pifTaskManager_ResetRealtime
 * @brief Clears the monitoring result accumulated in pif_task_realtime.
 */
void pifTaskManager_ResetRealtime();

#ifdef PIF_USE_BLOCK_TIME

/**
 * @fn pifTaskManager_ResetBlockTime
 * @brief Clears the longest run measured for every task and for the timer and idle callbacks.
 *        A single outlier stays in those values forever and keeps delaying the run it belongs
 *        to, so reset it after a one off long run such as an initialization path.
 */
void pifTaskManager_ResetBlockTime();

#endif

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
 * @fn pifTaskManager_AddTimer
 * @brief Adds a timer process to the task manager. It runs at the start of every loop, before
 *        any task, and its execution time counts towards pif_performance._task_load.
 *        The callback must not yield: it is not a task, so a yield inside it would be accounted
 *        as if the waiting were CPU time. With PIF_USE_BLOCK_TIME a TM_REALTIME task delays a
 *        timer whose measured run does not fit in the time left before its next release.
 * @param evt_timer The timer process callback function.
 * @param p_client User-defined context pointer owned by the caller.
 * @return Pointer to the resulting timer object or data, or NULL if unavailable.
 */
PifTaskTimer *pifTaskManager_AddTimer(PifEvtTaskTimer evt_timer, void *p_client);

/**
 * @fn pifTaskManager_RemoveTimer
 * @brief Removes a timer process from the task manager.
 * @param p_timer Pointer to the timer object.
 */
void pifTaskManager_RemoveTimer(PifTaskTimer *p_timer);

/**
 * @fn pifTaskManager_SetIdle
 * @brief Sets the idle process callback for the task manager. It runs only in a loop where no
 *        task was dispatched, which makes it the place for work to be done with the time left
 *        over. That work is work all the same, so its execution time counts towards
 *        pif_performance._task_load.
 *        The callback must not yield, for the same reason as a timer callback, and with
 *        PIF_USE_BLOCK_TIME a TM_REALTIME task delays it when its measured run does not fit in
 *        the time left before the release.
 * @param evt_idle The idle process callback function.
 * @param period_ms The idle period in milliseconds. Zero runs the callback in every idle loop.
 */
void pifTaskManager_SetIdle(PifEvtTaskIdle evt_idle, uint32_t period_ms);

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
