// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "core/pif_task_manager.h"


// A sequence runs on a TM_PERIOD task of its own, added by pifSequence_Init(). The task is
// paused while the sequence is idle, and while it runs it is released when the pending step is
// due. Steps measure time against pif_cumulative_timer1ms.
//
// The task asks for its next release through the return value of its loop, never through
// pifTask_SetTrigger(): a trigger is taken before the pause flag and before the check that the
// run fits in the time left before a TM_REALTIME release, and a step is ordinary work that should
// be subject to both.
//
// Each step decides what follows by calling pifSequence_Next(), pifSequence_Delay() or
// pifSequence_Wait() before it returns. A step that calls none of them ends the sequence.

// How often a wait looks for its signal. pifSequence_Signal() only raises a flag, so this is the
// latency between the signal and the step that follows it.
#ifndef PIF_SEQUENCE_WAIT_POLL_US
#define PIF_SEQUENCE_WAIT_POLL_US		1000UL
#endif

// Returned for a step that is due at once: asked for as soon as the ring comes round again.
#define PIF_SEQUENCE_NEXT_US			1UL

struct StPifSequence;
typedef struct StPifSequence PifSequence;

typedef void (*PifSequenceStep)(PifSequence *p_owner);

typedef enum EnPifSequenceState
{
	SQS_IDLE		= 0,
	SQS_RUN			= 1,
	SQS_DELAY		= 2,
	SQS_WAIT		= 3
} PifSequenceState;

struct StPifSequence
{
	// Public Member Variable
	void *p_param;

	// Read-only Member Variable
	PifId _id;
	PifTask *_p_task;

	// Private Member Variable
	PifSequenceStep __step;
	PifSequenceStep __on_timeout;
	uint32_t __start_ms;
	uint16_t __wait_ms;
	uint8_t __state;
	volatile uint8_t __event;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSequence_Init
 * @brief Initializes the sequence instance in the idle state and adds the task it runs on.
 *        The task stays registered, paused while the sequence is idle, until pifSequence_Clear().
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object and its task.
 * @param p_param Pointer to user-defined parameter block, available to the steps as p_owner->p_param.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSequence_Init(PifSequence *p_owner, PifId id, void *p_param);

/**
 * @fn pifSequence_Clear
 * @brief Stops the sequence and removes its task. Do not call it from a step.
 * @param p_owner Pointer to the target object instance.
 */
void pifSequence_Clear(PifSequence *p_owner);

/**
 * @fn pifSequence_Start
 * @brief Starts the sequence at the given step, which runs on the next poll.
 * @param p_owner Pointer to the target object instance.
 * @param step First step of the sequence.
 * @return TRUE on success, FALSE when the sequence is already running, step is NULL or the
 *         sequence has no task.
 */
BOOL pifSequence_Start(PifSequence *p_owner, PifSequenceStep step);

/**
 * @fn pifSequence_Stop
 * @brief Stops the sequence. No further step runs until it is started again.
 * @param p_owner Pointer to the target object instance.
 */
void pifSequence_Stop(PifSequence *p_owner);

/**
 * @fn pifSequence_IsRunning
 * @brief Checks whether the sequence has a step pending.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE while running, otherwise FALSE.
 */
BOOL pifSequence_IsRunning(PifSequence *p_owner);

/**
 * @fn pifSequence_Next
 * @brief Called from a step: runs the next step on the following poll.
 * @param p_owner Pointer to the target object instance.
 * @param next Step to run.
 */
void pifSequence_Next(PifSequence *p_owner, PifSequenceStep next);

/**
 * @fn pifSequence_Delay
 * @brief Called from a step: runs the next step once the delay has passed.
 * @param p_owner Pointer to the target object instance.
 * @param next Step to run.
 * @param delay1ms Delay value in milliseconds before next step.
 */
void pifSequence_Delay(PifSequence *p_owner, PifSequenceStep next, uint16_t delay1ms);

/**
 * @fn pifSequence_Wait
 * @brief Called from a step: runs the next step once pifSequence_Signal() is called.
 * @details Signals raised before this call are discarded, so call it before starting whatever
 *          produces the signal, e.g. before sending a command whose answer raises it.
 * @param p_owner Pointer to the target object instance.
 * @param next Step to run when the signal arrives.
 * @param timeout1ms Timeout value in milliseconds for event wait. 0 waits forever.
 * @param on_timeout Step to run on timeout. With NULL the sequence ends and pif_error is E_TIMEOUT.
 */
void pifSequence_Wait(PifSequence *p_owner, PifSequenceStep next, uint16_t timeout1ms, PifSequenceStep on_timeout);

/**
 * @fn pifSequence_Signal
 * @brief Raises the event a pifSequence_Wait() step waits for. It only sets a flag, so it may be
 *        called from an interrupt.
 * @param p_owner Pointer to the target object instance.
 */
void pifSequence_Signal(PifSequence *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
