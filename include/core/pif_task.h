#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "core/pif.h"


// Measures the longest run of each task, timer callback and idle callback without yielding.
// The full statistics include it.
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
	TM_REALTIME			= 0x40
} PifTaskMode;


struct StPifTask;
typedef struct StPifTask PifTask;

typedef uint32_t (*PifEvtTaskLoop)(PifTask* p_task);
typedef void (*PifActTaskSignal)(BOOL state);

typedef PifTask* (*PifTaskProcessing)(PifTask* p_owner);

typedef BOOL (*PifTaskCheckAbort)(PifIssuerP p_issuer);


/**
 * @struct StPifTask
 * @brief Represents the task data structure used by this module.
 */
struct StPifTask
{
	// Public Member Variable
	const char* name;
	BOOL pause;
	uint8_t disallow_yield_id;		// 0: Allow all, 1->255: Do not allow the corresponding id.

	// Read-only Member Variable
	PifId _id;
	PifTaskMode _mode;
	BOOL _running;
	uint32_t _default_period;
	uint32_t _delta_time;
	void *_p_client;
	uint32_t _last_execute_time;
#ifdef PIF_USE_TASK_STATISTICS
    uint32_t _total_execution_time;		// total time consumed by task since boot
    uint32_t _max_execution_time;
	uint32_t _max_trigger_delay;
#endif
#ifdef PIF_USE_BLOCK_TIME
	uint32_t _max_block_time;			// longest run without yielding, the delay this task can cause
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
 * @brief Sets configuration or runtime state for the task based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param delay Delay duration value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetTrigger(PifTask* p_owner, uint32_t delay);

/**
 * @fn pifTask_SetCutinTrigger
 * @brief Sets configuration or runtime state for the task based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetCutinTrigger(PifTask *p_owner);

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
 * @brief Retrieves the average execution time of the task. It includes the time a yield waited,
 *        so it is a wall clock value. Use _max_block_time for the time the CPU is actually held.
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
 * @fn pifTask_ResetMaxBlockTime
 * @brief Clears the longest run measured for the task. A single outlier stays in the value
 *        forever, so reset it after a one off long run such as an initialization path.
 * @param p_owner Pointer to the target object instance.
 */
void pifTask_ResetMaxBlockTime(PifTask *p_owner);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
