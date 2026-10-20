// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_task_manager.h
 * @brief Cooperative scheduler that runs the tasks, the timer callbacks and the idle callback.
 *
 * Call pifTaskManager_Init() once, add the tasks with pifTaskManager_Add(), and call pifTaskManager_Loop() from
 * the main loop. With PIF_USE_TASK_REALTIME a TM_REALTIME task takes priority, and the other tasks run only when
 * their measured run fits before its next release. Without it the realtime release, the time kept free before it
 * and max_skip are not built, and the tasks, the timer callbacks and the idle callback run as soon as they are due.
 */
#ifndef PIF_TASK_MANAGER_H
#define PIF_TASK_MANAGER_H


#include "core/pif_task.h"


/**
 * @brief Callback run at the start of every pass of the main loop.
 */
typedef void (*PifEvtTaskTimer)(void *p_client);

/**
 * @struct StPifTaskTimer
 * @brief Callback run at the start of every pass of the main loop, added with pifTaskManager_AddTimer().
 */
typedef struct StPifTaskTimer
{
	// Public Member Variable
#ifdef PIF_USE_TASK_REALTIME
	/// As PifTask::max_skip, for the loops this callback may be held back in a row. A timer
	/// callback is meant to run at the start of every loop, so being held back without a bound
	/// would break that contract rather than merely delay it.
	uint16_t max_skip;
#endif

	// Read-only Member Variable
    void *_p_client;			///< Client given to pifTaskManager_AddTimer()
#ifdef PIF_USE_BLOCK_TIME
	PifBlockTime _block_time;	///< longest run of the callback, the delay it can cause
#endif

	// Read-only Event Function
    PifEvtTaskTimer _evt_timer;	///< The callback

#ifdef PIF_USE_TASK_REALTIME
	// Private Member Variable
	uint16_t __skip_count;		///< Runs held back in a row, against max_skip
#endif
} PifTaskTimer;

/**
 * @brief Callback run when no task is due, set with pifTaskManager_SetIdle().
 */
typedef void (*PifEvtTaskIdle)(void);

#ifdef PIF_USE_TASK_STATISTICS

/**
 * @struct StPifTaskIdleStatistics
 * @brief The statistics of the idle callback. It runs outside the task ring, so it is not a
 *        PifTask, but it keeps the same measurements over the same moving window so that a report
 *        can put it next to the tasks.
 */
typedef struct StPifTaskIdleStatistics
{
	uint32_t total_execution_time;		///< Sum of the run times since the callback was set (us)
	uint32_t max_execution_time;		///< Longest run since the last reset (us)
	uint32_t average_execution_time;	///< Average run time (us), or PIF_TASK_AVERAGE_NONE until
										///< PIF_TASK_AVERAGE_MIN_COUNT runs are collected
	uint32_t average_delta_time;		///< Average time between the starts of two runs (us), likewise
} PifTaskIdleStatistics;

#endif

#ifdef PIF_DEBUG

extern PifActTaskSignal pif_act_task_signal;

#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates the pools of tasks and timer callbacks. Call it once after pif_Init().
 * @param max_count Most tasks.
 * @param timer_count Most timer callbacks, 0 for none.
 * @return TRUE on success, FALSE with pif_error set otherwise. E_INVALID_STATE before pif_Init().
 */
BOOL pifTaskManager_Init(int max_count, int timer_count);

/**
 * @brief Removes every task and timer callback and frees the pools. Safe before pif_Init() as well.
 */
void pifTaskManager_Clear();

/**
 * @brief Clears the monitoring result accumulated in pif_task_realtime.
 */
void pifTaskManager_ResetRealtime();

#ifdef PIF_USE_BLOCK_TIME

/**
 * @brief Clears the longest run measured for every task and for the timer and idle callbacks.
 *        A single outlier stays in those values forever and keeps delaying the run it belongs
 *        to, so reset it after a one off long run such as an initialization path.
 */
void pifTaskManager_ResetBlockTime();

#endif

/**
 * @brief Adds a task with its loop function.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param mode How the task is released. Only one task can be TM_REALTIME.
 * @param period Period in microseconds for TM_PERIOD and TM_REALTIME. Ignored for TM_EXTERNAL.
 * @param evt_loop Loop of the task.
 * @param p_client Client of the task, kept in _p_client.
 * @param start TRUE to start it now, FALSE to leave it paused until pifTask_Resume().
 * @return The task, or NULL with pif_error set.
 */
PifTask *pifTaskManager_Add(PifId id, PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void *p_client, BOOL start);

/**
 * @brief Removes a task.
 * @param p_task Task to remove.
 */
void pifTaskManager_Remove(PifTask *p_task);

/**
 * @brief Gets the number of tasks.
 * @return Number of tasks.
 */
int pifTaskManager_Count();

/**
 * @brief Gets the task whose loop is running.
 * @return The task, or NULL outside a loop.
 */
PifTask *pifTaskManager_CurrentTask();

/**
 * @brief Adds a timer process to the task manager. It runs at the start of every loop, before
 *        any task, and its execution time counts towards pif_performance._task_load.
 *        With PIF_USE_BLOCK_TIME a TM_REALTIME task delays a timer whose measured run does not
 *        fit in the time left before its next release.
 * @param evt_timer The timer process callback function.
 * @param p_client User-defined context pointer owned by the caller.
 * @return Pointer to the resulting timer object or data, or NULL if unavailable.
 */
PifTaskTimer *pifTaskManager_AddTimer(PifEvtTaskTimer evt_timer, void *p_client);

/**
 * @brief Removes a timer process from the task manager.
 * @param p_timer Pointer to the timer object.
 */
void pifTaskManager_RemoveTimer(PifTaskTimer *p_timer);

/**
 * @brief Sets the idle process callback for the task manager. It runs only in a loop where no
 *        task was dispatched, which makes it the place for work to be done with the time left
 *        over. That work is work all the same, so its execution time counts towards
 *        pif_performance._task_load.
 *        With PIF_USE_BLOCK_TIME a TM_REALTIME task delays it when its measured run does not fit
 *        in the time left before the release.
 * @param evt_idle The idle process callback function.
 * @param period_ms The idle period in milliseconds. Zero runs the callback in every idle loop.
 */
void pifTaskManager_SetIdle(PifEvtTaskIdle evt_idle, uint32_t period_ms);

/**
 * @brief Runs one scheduling loop iteration and dispatches eligible tasks in the manager.
 *        A loop callback runs to its end before anything else does: there is no way to give the
 *        CPU up in the middle of one and get it back later. A task that has to wait for something
 *        returns instead, and is released again when the wait is over, either after the delay it
 *        returned or by pifTask_SetTrigger().
 *        With PIF_USE_BLOCK_TIME a TM_REALTIME task delays a task whose measured run does not fit
 *        in the time left before its release, whether the task is due by its period, by the delay
 *        it returned or by pifTask_SetTrigger(). Only pifTask_SetCutinTrigger() is dispatched
 *        regardless.
 *        Returns at once while pif_ready is FALSE.
 */
void pifTaskManager_Loop();

/**
 * @brief Calls the callback once for every task in the manager, in ring order.
 * @param callback Callback function invoked for each task entry.
 * @param p_arg Passed to every call of the callback unchanged, so the caller can carry state
 *              such as a running total across the tasks without a static variable.
 */
void pifTaskManager_AllTask(void (*callback)(PifTask *p_task, void *p_arg), void *p_arg);

#ifdef PIF_USE_TASK_STATISTICS

/**
 * @brief Retrieves the statistics of the idle callback set by pifTaskManager_SetIdle().
 * @param p_stat Filled in with the statistics. Left untouched when there is no idle callback.
 * @return FALSE when no idle callback is set, so there is nothing to report.
 */
BOOL pifTaskManager_GetIdleStatistics(PifTaskIdleStatistics *p_stat);

/**
 * @brief Clears the longest run of the idle callback, as pifTask_ResetMaxExecutionTime() does
 *        for a task.
 */
void pifTaskManager_ResetIdleMaxExecutionTime();

#endif

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

/**
 * @brief Logs every task with its mode, period and, with PIF_USE_BLOCK_TIME or PIF_USE_TASK_STATISTICS, its
 *        measurements, then the realtime figures and the load.
 */
void pifTaskManager_Print();

#endif	// PIF_NO_LOG

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_MANAGER_H
