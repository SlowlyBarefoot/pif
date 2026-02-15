#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "core/pif.h"


typedef enum EnPifTaskMode
{
	TM_NONE				= 0,

	TM_ALWAYS			= 0x04,
	TM_TIMER			= 0x08,		// Do not use it for other purposes because it is a mode used by the timer.

	TM_EXTERNAL			= 0x10,
	TM_PERIOD			= 0x20,
	TM_IDLE				= 0x80		// If at least one TM_ALWAYS task exists, the TM_IDLE_MS task is not executed.
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
    int32_t _max_execution_time;
	uint32_t _max_trigger_delay;
#endif

	// Private Member Variable
	PifTaskProcessing __processing;
	uint32_t __period;
	BOOL __trigger;
    int __timer_trigger;
	uint32_t __delay_us;
	uint32_t __current_time;
	uint32_t __pretime;
	uint32_t __trigger_time;
	uint32_t __trigger_delay;
#ifdef PIF_USE_TASK_STATISTICS
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

#ifdef PIF_DEBUG

extern PifActTaskSignal pif_act_task_signal;

#endif


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

/**
 * @fn pifTask_SetTriggerForTimer
 * @brief Sets configuration or runtime state for the task based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTask_SetTriggerForTimer(PifTask *p_owner);

/**
 * @fn pifTask_DelayMs
 * @brief Performs a delay operation in the task context for the requested time interval.
 * @param p_owner Pointer to the target object instance.
 * @param delay Delay duration value.
 */
void pifTask_DelayMs(PifTask* p_owner, uint16_t delay);

#ifdef PIF_USE_TASK_STATISTICS

/**
 * @fn pifTask_GetAverageDeltaTime
 * @brief Retrieves the requested value or pointer from the task without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifTask_GetAverageDeltaTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageExecuteTime
 * @brief Retrieves the requested value or pointer from the task without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifTask_GetAverageExecuteTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageTriggerTime
 * @brief Retrieves the requested value or pointer from the task without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifTask_GetAverageTriggerTime(PifTask* p_owner);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
