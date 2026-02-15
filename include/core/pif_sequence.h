#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


struct StPifSequence;
typedef struct StPifSequence PifSequence;

typedef void (*PifSequenceProcess)(PifSequence* p_owner);
typedef void (*PifEvtSequenceError)(PifSequence* p_owner);


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifSequenceCsFlag
{
    SQ_CSF_OFF			= 0,

    SQ_CSF_PHASE_IDX	= 0,

	SQ_CSF_PHASE_BIT	= 1,
	SQ_CSF_ALL_BIT		= 1,

    SQ_CSF_COUNT		= 1
} PifSequenceCsFlag;

typedef struct StPifSequenceColSig
{
	PifSequence* p_owner;
	uint8_t flag;
    void* p_device[SQ_CSF_COUNT];
} PifSequenceColSig;

#endif	// PIF_COLLECT_SIGNAL

struct StPifSequence
{
	// Public Member Variable
	void* p_param;

    // Public Event Function
	PifEvtSequenceError evt_error;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifTask* __p_task;
	PifTimerManager* __p_timer_manager;
	PifTimer *__p_timer_timeout;
	PifSequenceProcess __process;
	PifSequenceProcess __next_process;
#ifdef PIF_COLLECT_SIGNAL
	PifSequenceColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSequence_Init
 * @brief Initializes the sequence instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param p_timer_manager Pointer to the timer manager instance.
 * @param p_param Pointer to user-defined parameter block.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifTimerManager* p_timer_manager, void* p_param);

/**
 * @fn pifSequence_Clear
 * @brief Clears the sequence state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifSequence_Clear(PifSequence* p_owner);

/**
 * @fn pifSequence_IsRunning
 * @brief Checks whether the sequence currently satisfies the requested condition.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_IsRunning(PifSequence *p_owner);

/**
 * @fn pifSequence_Start
 * @brief Starts the sequence operation using the current timing, trigger, or mode configuration.
 * @param p_owner Pointer to the target object instance.
 * @param process Sequence processing callback function.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_Start(PifSequence *p_owner, PifSequenceProcess process);

/**
 * @fn pifSequence_NextDelay
 * @brief Executes the pifSequence_NextDelay operation for the sequence module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param process Sequence processing callback function.
 * @param delay1ms Delay value in milliseconds before next step.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_NextDelay(PifSequence *p_owner, PifSequenceProcess process, uint16_t delay1ms);

/**
 * @fn pifSequence_NextEvent
 * @brief Executes the pifSequence_NextEvent operation for the sequence module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param process Sequence processing callback function.
 * @param timeout1ms Timeout value in milliseconds for event wait.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_NextEvent(PifSequence *p_owner, PifSequenceProcess process, uint16_t timeout1ms);

/**
 * @fn pifSequence_TriggerEvent
 * @brief Triggers a pending event or transition in the sequence according to current state.
 * @param p_owner Pointer to the target object instance.
 */
void pifSequence_TriggerEvent(PifSequence *p_owner);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSequence_SetCsFlag
 * @brief Sets configuration or runtime state for the sequence based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifSequence_SetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag);

/**
 * @fn pifSequence_ResetCsFlag
 * @brief Resets runtime state in the sequence to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifSequence_ResetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag);

/**
 * @fn pifSequenceColSig_SetFlag
 * @brief Sets configuration or runtime state for the sequence col sig based on the provided parameters.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifSequenceColSig_SetFlag(PifSequenceCsFlag flag);

/**
 * @fn pifSequenceColSig_ResetFlag
 * @brief Resets runtime state in the sequence col sig to an initial or configured baseline.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifSequenceColSig_ResetFlag(PifSequenceCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
