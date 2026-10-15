// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "core/pif.h"


// Measures the longest run of each task, timer callback and idle callback. The full statistics
// include it.
#if defined(PIF_USE_TASK_STATISTICS) && !defined(PIF_USE_BLOCK_TIME)
#define PIF_USE_BLOCK_TIME
#endif

// Samples the moving average needs before pifTask_GetAverage*() reports a value.
#define PIF_TASK_AVERAGE_MIN_COUNT		20

// Returned by pifTask_GetAverage*() while there are not enough samples yet. It is distinct from
// an average of 0, which a task short enough to measure as 0 microseconds can produce.
#define PIF_TASK_AVERAGE_NONE			0xFFFFFFFFUL


typedef enum EnPifTaskMode
{
	TM_NONE				= 0,

	TM_EXTERNAL			= 0x10,
	TM_PERIOD			= 0x20,
	// Takes priority over every other task at its release. The release comes from the period,
	// from pifTask_SetTrigger(), or from both, so a period of zero means released by trigger
	// alone. A non zero period is also the window kept free before the release, which is why a
	// task released by trigger alone is protected only while a delayed trigger is pending.
	TM_REALTIME			= 0x40
} PifTaskMode;


#ifdef PIF_USE_BLOCK_TIME

/**
 * @struct StPifBlockTime
 * @brief Longest single run, measured over a moving window rather than since boot.
 *        Two buckets take turns holding the maximum, and the one being replaced is cleared, so
 *        a single outlier is forgotten after 100 to 200 runs. Without that a one off long run,
 *        an initialization path or a flash erase, would hold its owner back for good.
 */
typedef struct StPifBlockTime
{
	// Read-only Member Variable
	uint32_t _max;				// Longest run of the window, the delay this owner can cause

	// Private Member Variable
	uint32_t __bucket[2];
	uint16_t __count;
	uint8_t __index;
} PifBlockTime;

#endif


struct StPifTask;
typedef struct StPifTask PifTask;

typedef uint32_t (*PifEvtTaskLoop)(PifTask* p_task);
typedef void (*PifActTaskSignal)(BOOL state);

typedef PifTask* (*PifTaskProcessing)(PifTask* p_owner);


/**
 * @struct StPifTask
 * @brief Represents the task data structure used by this module.
 */
struct StPifTask
{
	// Public Member Variable
	const char* name;
	// How many times in a row a release of this task may be held back because its measured run
	// does not fit before the next realtime release. On the next visit after that it is let
	// through and counted as a guard lapse, which turns an unbounded wait into a stated one: at
	// most this many passes of the ring while the task is already due.
	// 0 takes PIF_TASK_MAX_SKIP, which is what a task registered without an opinion gets.
	uint16_t max_skip;

	// Read-only Member Variable
	BOOL _pause;					// Set by pifTask_Pause(), cleared by pifTask_Resume() and pifTask_Restart()
	PifId _id;
	PifTaskMode _mode;
	uint32_t _default_period;
	uint32_t _delta_time;
	void *_p_client;
	uint32_t _last_execute_time;
#ifdef PIF_USE_TASK_STATISTICS
    uint32_t _total_execution_time;		// Sum of the run times of this task (us)
    uint32_t _max_execution_time;
	uint32_t _max_trigger_delay;
	uint32_t _max_delay;				// Longest start past the period of this task. A release by
										// a trigger is measured by _max_trigger_delay instead.
#endif
#ifdef PIF_USE_BLOCK_TIME
	PifBlockTime _block_time;			// longest single run, the delay this task can cause
#endif

	// Private Member Variable
	PifTaskProcessing __processing;
	uint32_t __period;
	BOOL __trigger;
	uint32_t __delay_us;
	uint32_t __current_time;
	uint32_t __pretime;
	uint32_t __trigger_time;
	uint32_t __trigger_delay;
	uint16_t __skip_count;			// Releases held back in a row, against max_skip
#ifdef PIF_USE_BLOCK_TIME
	BOOL __ignore_block;
	uint32_t __next_block_time;		// Length the task declared for its next run, 0 to use _block_time._max
#endif
#ifdef PIF_USE_TASK_STATISTICS
	// Moving average buckets. The pair holds up to 199 samples, so their sum overflows once the
	// average sample passes about 21 seconds. A task with a period that long reports a wrong
	// average delta time.
	uint32_t __total_delta_time[2];
    uint32_t __sum_execution_time[2];
	uint32_t __total_trigger_delay[2];
	uint16_t __execution_count;
	uint16_t __trigger_count;
	uint8_t __execute_index;
	uint8_t __trigger_index;
#endif

	// Private Event Function
	PifEvtTaskLoop __evt_loop;
};

/**
 * @struct StPifTaskTimeout
 * @brief One shot deadline in microseconds, for the timeouts a module used to keep a TT_ONCE timer
 *        for. Any number of them can share one task: each start, and each check that finds the
 *        deadline still ahead, brings the trigger of the task forward to it when it is nearer
 *        than the one pending. The task therefore runs at the nearest deadline and, by checking
 *        every timeout it owns with pifTaskTimeout_IsExpired(), handles what is due and arms the
 *        trigger for the next one.
 *        A timeout without a task is only polled: pifTaskTimeout_IsExpired() is then the whole
 *        mechanism, as it is for a module that has no task of its own.
 *        A stopped timeout leaves the trigger it may have set, because other timeouts can share
 *        it. The task then runs once with nothing due, which is harmless.
 *        A timeout filled with zeros is stopped and has no task.
 */
typedef struct StPifTaskTimeout
{
	// Private Member Variable
	PifTask* __p_task;
	uint32_t __start;
	uint32_t __time;				// 0 while stopped
} PifTaskTimeout;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTask_Init
 * @brief Initializes the task instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 */
void pifTask_Init(PifTask* p_owner, PifId id);

/**
 * @fn pifTask_CheckParam
 * @brief Executes the pifTask_CheckParam operation for the task module according to the API contract.
 * @param p_mode Pointer to task mode output or mode selector.
 * @param period Execution period value for scheduling.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_CheckParam(PifTaskMode* p_mode, uint32_t period);

/**
 * @fn pifTask_SetParam
 * @brief Sets configuration or runtime state for the task based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param mode Operating mode configuration value.
 * @param period Execution period value for scheduling.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetParam(PifTask* p_owner, PifTaskMode mode, uint32_t period);

/**
 * @fn pifTask_ChangeMode
 * @brief Changes runtime configuration of the task while preserving object ownership semantics.
 * @param p_owner Pointer to the target object instance.
 * @param mode Operating mode configuration value.
 * @param period Execution period value for scheduling.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint32_t period);

/**
 * @fn pifTask_ChangePeriod
 * @brief Changes runtime configuration of the task while preserving object ownership semantics.
 * @param p_owner Pointer to the target object instance.
 * @param period Execution period value for scheduling.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_ChangePeriod(PifTask* p_owner, uint32_t period);

/**
 * @fn pifTask_SetTrigger
 * @brief Releases the task once after the delay. When the delay is over the release is due the
 *        way a period is: with PIF_USE_BLOCK_TIME and a TM_REALTIME task it is held back while the
 *        run of the task does not fit before the next realtime release, by the same rule and bound
 *        (max_skip) as a period release. A task that must never wait sets max_skip to 1, and work
 *        that cannot wait at all takes pifTask_SetCutinTrigger().
 *        A trigger still pending is replaced, delay included. Safe to call from an interrupt.
 * @param p_owner Pointer to the target object instance.
 * @param delay Delay in microseconds. 0 releases the task on its next visit.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetTrigger(PifTask* p_owner, uint32_t delay);

/**
 * @fn pifTask_SetCutinTrigger
 * @brief Releases the task at the next loop, ahead of the ring and of the realtime task, and
 *        without asking whether its run fits before the next realtime release. Only one cut in is
 *        held at a time: while one is pending the task gets pifTask_SetTrigger() with no delay
 *        instead, which waits for its turn in the ring and for the realtime slack.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetCutinTrigger(PifTask *p_owner);

/**
 * @fn pifTask_CancelTrigger
 * @brief Withdraws a release that pifTask_SetTrigger(), pifTask_SetCutinTrigger() or the return
 *        value of a TM_EXTERNAL task asked for and that has not happened yet. A pause does not do
 *        this: a trigger releases a paused task.
 *        Not for interrupt context, where a trigger may be set at the same time.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_CancelTrigger(PifTask *p_owner);

/**
 * @fn pifTask_Pause
 * @brief Stops the periodic release of the task. A trigger still releases it: pifTask_CancelTrigger()
 *        withdraws one that is pending. A TM_EXTERNAL task has no periodic release, so this has no
 *        effect on it.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_Pause(PifTask *p_owner);

/**
 * @fn pifTask_Resume
 * @brief Lets the periodic release run again on the grid it had. A task paused for longer than its
 *        period is therefore released on its next visit; pifTask_Restart() counts the period from
 *        now instead. No effect on a TM_EXTERNAL task.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_Resume(PifTask *p_owner);

/**
 * @fn pifTask_Restart
 * @brief Resumes the task and counts the period from now, so that the first release after it is
 *        one full period later. The period a run returned is dropped for the default one. A pending
 *        trigger is kept.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_Restart(PifTask *p_owner);

/**
 * @fn pifTask_IsSubPeriodElapsed
 * @brief Divides the runs of a periodic task into a slower step, such as the blink of a display
 *        refreshed by that task. Returns TRUE once every period. After a gap longer than a period,
 *        a paused task for instance, it starts over from now instead of catching up with a burst.
 * @param p_time_us Time the current step began. Set it to (*pif_act_timer1us)() to start.
 * @param period_us Length of a step in microseconds.
 * @return TRUE if a step has elapsed since *p_time_us.
 */
BOOL pifTask_IsSubPeriodElapsed(uint32_t *p_time_us, uint32_t period_us);

#ifdef PIF_USE_TASK_STATISTICS

/**
 * @fn pifTask_ResetStatistics
 * @brief Resets all accumulated statistics for the task, including total execution time,
 *        maximum execution time, maximum trigger delay, maximum block time, delta time
 *        counters, and execution count. Use this to clear historical data when measuring
 *        performance from a new baseline.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_ResetStatistics(PifTask* p_owner);

/**
 * @fn pifTask_ResetMaxExecutionTime
 * @brief Resets only the maximum execution time statistic for the task to zero,
 *        leaving all other accumulated statistics unchanged.
 *        Use this to start tracking a new peak execution time without discarding
 *        total execution time or delta time history.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_ResetMaxExecutionTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageDeltaTime
 * @brief Retrieves the average time between the executions of the task.
 * @param p_owner Pointer to the target object instance.
 * @return Average in microseconds, or PIF_TASK_AVERAGE_NONE until PIF_TASK_AVERAGE_MIN_COUNT
 *         samples are collected.
 */
uint32_t pifTask_GetAverageDeltaTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageExecuteTime
 * @brief Retrieves the average execution time of the task.
 * @param p_owner Pointer to the target object instance.
 * @return Average in microseconds, or PIF_TASK_AVERAGE_NONE until PIF_TASK_AVERAGE_MIN_COUNT
 *         samples are collected.
 */
uint32_t pifTask_GetAverageExecuteTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageTriggerTime
 * @brief Retrieves the average delay between a trigger and the execution it caused.
 * @param p_owner Pointer to the target object instance.
 * @return Average in microseconds, or PIF_TASK_AVERAGE_NONE until PIF_TASK_AVERAGE_MIN_COUNT
 *         samples are collected.
 */
uint32_t pifTask_GetAverageTriggerTime(PifTask* p_owner);

#endif

#ifdef PIF_USE_BLOCK_TIME

/**
 * @fn pifTask_ResetBlockTime
 * @brief Clears a block time measurement, both buckets of its moving window.
 * @param p_owner Pointer to the target measurement.
 */
void pifTask_ResetBlockTime(PifBlockTime *p_owner);

/**
 * @fn pifTask_UpdateBlockTime
 * @brief Adds one run to a block time measurement. The scheduler calls this where the run ends,
 *        which is the return of the task or of the callback.
 * @param p_owner Pointer to the target measurement.
 * @param block_time Length of the run in microseconds.
 */
void pifTask_UpdateBlockTime(PifBlockTime *p_owner, uint32_t block_time);

/**
 * @fn pifTask_ResetMaxBlockTime
 * @brief Clears the longest run measured for the task, both buckets of the moving window.
 *        The window forgets an outlier on its own, so this is for starting a measurement from
 *        a known baseline rather than for undoing one long run: pifTask_IgnoreBlockTime()
 *        keeps that run out of the value in the first place.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_ResetMaxBlockTime(PifTask *p_owner);

/**
 * @fn pifTask_SetNextBlockTime
 * @brief Declares how long the next run of the task will be, for the scheduler to judge against
 *        the time left before the next realtime release. It replaces _block_time._max for that
 *        one run only, and the declaration is consumed when the task is dispatched: a task that
 *        says nothing is judged by its measurement again.
 *        This is for a task whose runs are not interchangeable - a state machine whose states
 *        differ by an order of magnitude, where the longest run seen would keep the short states
 *        out for no reason. Call it at the end of a run, for the run that follows.
 *        0 restores the measured value.
 * @param p_owner Pointer to the target object instance.
 * @param block_time Length of the next run in microseconds.
 */
void pifTask_SetNextBlockTime(PifTask *p_owner, uint32_t block_time);

/**
 * @fn pifTask_IgnoreBlockTime
 * @brief Leaves the current execution out of the block time of the task. Call it from inside the
 *        task when the run is not representative, such as an initialization path or a flash
 *        erase.
 *        The time still counts towards the CPU load, because the CPU was held either way, and
 *        the execution time statistics are unaffected. Only the value the scheduler uses to
 *        decide whether this task fits before a realtime release is left untouched.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_IgnoreBlockTime(PifTask *p_owner);

#endif


/**
 * @fn pifTaskTimeout_Init
 * @brief Stops the timeout and binds it to the task released at its expiry.
 * @param p_owner Pointer to the timeout.
 * @param p_task Task to trigger at the deadline, or NULL for a timeout that is only polled.
 */
void pifTaskTimeout_Init(PifTaskTimeout *p_owner, PifTask *p_task);

/**
 * @fn pifTaskTimeout_Start
 * @brief Starts or restarts the timeout from now, and triggers its task for the deadline unless a
 *        nearer trigger is already pending. Not for interrupt context.
 * @param p_owner Pointer to the timeout.
 * @param time_us Time to the deadline in microseconds. 0 is taken as 1, expired at the next check.
 */
void pifTaskTimeout_Start(PifTaskTimeout *p_owner, uint32_t time_us);

/**
 * @fn pifTaskTimeout_Stop
 * @brief Stops the timeout. See PifTaskTimeout for the trigger it leaves.
 * @param p_owner Pointer to the timeout.
 */
void pifTaskTimeout_Stop(PifTaskTimeout *p_owner);

/**
 * @fn pifTaskTimeout_IsRunning
 * @brief Tells whether the timeout is started and not yet reported as expired.
 * @param p_owner Pointer to the timeout.
 * @return TRUE while it runs.
 */
BOOL pifTaskTimeout_IsRunning(PifTaskTimeout *p_owner);

/**
 * @fn pifTaskTimeout_IsExpired
 * @brief Checks the timeout and stops it when it has expired, so that each expiry is reported once.
 *        While the deadline is still ahead it triggers the task for it, as pifTaskTimeout_Start()
 *        does, which is what keeps the remaining timeouts of a task armed after the task has run
 *        for the nearest one.
 * @param p_owner Pointer to the timeout.
 * @return TRUE if it was running and has expired.
 */
BOOL pifTaskTimeout_IsExpired(PifTaskTimeout *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
