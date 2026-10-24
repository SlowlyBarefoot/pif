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

#ifdef PIF_USE_TASK_REALTIME

/**
 * @struct StPifTaskRealtimeStatistics
 * @brief How well the realtime task was served, filled in by pifTaskManager_GetRealtimeStatistics().
 */
typedef struct StPifTaskRealtimeStatistics
{
	uint32_t max_delay;		///< Longest delay from the release to the dispatch (us)
	uint32_t miss_count;	///< Releases delayed by a period or more
	uint32_t guard;			///< Margin kept free before the release (us), 0 without PIF_USE_BLOCK_TIME
	uint32_t lapse_count;	///< Runs let through without that margin, 0 without PIF_USE_BLOCK_TIME
} PifTaskRealtimeStatistics;

#endif

#ifdef PIF_USE_TASK_STATISTICS_VCD

/// Room a line of pifTaskManager_GetVcdLine() needs, the terminating NUL included.
#define PIF_TASK_VCD_LINE_SIZE		64

/**
 * @struct StPifTaskStatisticsRecord
 * @brief One set of statistics taken together, as the application keeps it. The record only
 *        points at the copies, so the application lays its storage out as it likes and fills this
 *        in when pifTaskManager_GetVcdLine() asks for a record. A pointer left NULL is a value
 *        that was not taken, which the VCD shows as x.
 */
typedef struct StPifTaskStatisticsRecord
{
	uint32_t time;									///< When it was taken, in the unit of the timescale
	uint8_t load;									///< pif_performance._task_load
	uint8_t task_count;								///< Entries in p_task
	const PifTaskStatistics *p_task;				///< From pifTask_GetStatistics(), the same tasks in the same order in every record
#ifdef PIF_USE_TASK_REALTIME
	const PifTaskRealtimeStatistics *p_realtime;	///< From pifTaskManager_GetRealtimeStatistics()
#endif
	const PifTaskIdleStatistics *p_idle;			///< From pifTaskManager_GetIdleStatistics()
} PifTaskStatisticsRecord;

/**
 * @brief Fills in a record of the VCD file.
 * @param index Record, from 0 for the oldest to the count given to pifTaskManager_StartVcd().
 * @param p_record Filled in with the record.
 * @param p_client Given to pifTaskManager_StartVcd().
 * @return FALSE when the record is gone, which ends the file there.
 */
typedef BOOL (*PifTaskVcdGetRecord)(uint16_t index, PifTaskStatisticsRecord *p_record, void *p_client);

/**
 * @struct StPifTaskVcd
 * @brief Where the making of a VCD file stands. The caller owns it, and nothing else is kept, so
 *        a file can be made while another one is.
 */
typedef struct StPifTaskVcd
{
	// Private Member Variable
	PifTaskVcdGetRecord __get_record;
	void *__p_client;
	const char *__p_version;
	const char *__p_timescale;
	uint16_t __count;
	uint16_t __record;
	uint16_t __signal;
	int16_t __scope;
	uint8_t __stage;
	PifTaskStatisticsRecord __first;			///< Gives the tasks the header declares
	PifTaskStatisticsRecord __current;
	PifTaskStatisticsRecord __previous;			///< Only the values that changed from it are written
} PifTaskVcd;

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
 * @brief Clears what was measured of the realtime task, pif_performance._max_delay and _miss_count,
 *        and with PIF_USE_BLOCK_TIME also the margin the scheduler learned to keep free before the
 *        release, which then starts again from its lower bound. To clear only the figures, as a
 *        periodic report does, use pifTaskManager_GetRealtimeStatistics() with reset.
 */
void pifTaskManager_ResetRealtime();

#ifdef PIF_USE_TASK_REALTIME

/**
 * @brief Copies how well the realtime task was served, for a report or a periodic backup.
 * @param p_stat Filled in with the figures.
 * @param reset TRUE to clear the longest delay, the miss count and the lapse count once they are
 *              copied, so that each backup holds its own interval. The margin is left alone, as
 *              the scheduler keeps using it.
 */
void pifTaskManager_GetRealtimeStatistics(PifTaskRealtimeStatistics *p_stat, BOOL reset);

#endif

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

#ifdef PIF_USE_TASK_STATISTICS_VCD

/**
 * @brief Starts a VCD file of statistics records the application keeps, such as a periodic backup.
 *        The file has a scope for the system figures (load, realtime and idle) and one for each
 *        task of the first record, named after the task with that ID that runs now. Each record
 *        is one time step, and after the first only the values that changed are written.
 *        The lines are taken with pifTaskManager_GetVcdLine(), so the caller decides where they
 *        go and how fast. The records must stay as they are until the file is done.
 * @param p_owner The making of the file, owned by the caller.
 * @param count Records, 1 or more.
 * @param get_record Fills in a record when its turn comes.
 * @param p_client Passed to get_record unchanged.
 * @param p_version Text of the $version line.
 * @param p_timescale Text of the $timescale line, the unit of PifTaskStatisticsRecord::time, such as "1 s".
 * @return FALSE with pif_error set when there is no first record.
 */
BOOL pifTaskManager_StartVcd(PifTaskVcd *p_owner, uint16_t count, PifTaskVcdGetRecord get_record, void *p_client,
		const char *p_version, const char *p_timescale);

/**
 * @brief Makes the next line of the VCD file, its newline included. The last one is a $comment
 *        that says the file is complete, so a file cut short can be told from a whole one.
 * @param p_owner The making of the file.
 * @param p_line Filled in with the line.
 * @param size Room in p_line, PIF_TASK_VCD_LINE_SIZE or more.
 * @return Length of the line, 0 once the file is done, or with pif_error set when size is too small.
 */
uint16_t pifTaskManager_GetVcdLine(PifTaskVcd *p_owner, char *p_line, uint16_t size);

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
