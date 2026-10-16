// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_TIMER_H
#define PIF_TIMER_H


#include "core/pif_task.h"


typedef void (*PifEvtTimerFinish)(PifIssuerP p_issuer);


typedef enum EnPifTimerType
{
    TT_ONCE			= 0,
    TT_REPEAT		= 1
} PifTimerType;

typedef enum EnPifTimerStep
{
    TS_STOP			= 0,
    TS_RUNNING		= 1,
    TS_REMOVE		= 2
} PifTimerStep;


/**
 * @struct StPifTimer
 * @brief Represents the timer data structure used by this module.
 */
typedef struct StPifTimer
{
	// Read-only Member Variable
	// Set with pifTimer_Start() or pifTimer_SetTarget(). pifTimerManager_sigTick() reads it at every
	// reload, and on an 8-bit target it could read a value written directly half way.
    volatile uint32_t _target;
    PifTimerType _type;
    // pifTimerManager_sigTick() counts only a timer in TS_RUNNING, so the main context takes a
    // timer out of it while it rewrites what the handler reads.
    volatile PifTimerStep _step;

	// Private Member Variable
    volatile uint32_t __current;
    PifIssuerP __p_finish_issuer;
    BOOL __event_into_int;
	volatile BOOL __event;

    // Private Event Function
    PifEvtTimerFinish __evt_finish;
} PifTimer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTimer_Start
 * @brief Starts the timer operation using the current timing, trigger, or mode configuration.
 * @param p_owner Pointer to the target object instance.
 * @param target Target tick count to run before completion.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimer_Start(PifTimer* p_owner, uint32_t target);

/**
 * @fn pifTimer_Stop
 * @brief Stops the timer operation and transitions it into an inactive state.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Stop(PifTimer* p_owner);

/**
 * @fn pifTimer_Reset
 * @brief Resets runtime state in the timer to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Reset(PifTimer* p_owner);

/**
 * @fn pifTimer_SetTarget
 * @brief Changes the tick count of the timer. A running timer finishes its current period and
 *        uses the new count from the next reload, as pifTimer_Reset() and repeat timers do.
 * @param p_owner Pointer to the target object instance.
 * @param target Target tick count, greater than 0.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimer_SetTarget(PifTimer* p_owner, uint32_t target);

/**
 * @fn pifTimer_Remain
 * @brief Executes the pifTimer_Remain operation for the timer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @return Return value of this API.
 */
uint32_t pifTimer_Remain(PifTimer* p_owner);

/**
 * @fn pifTimer_Elapsed
 * @brief Executes the pifTimer_Elapsed operation for the timer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @return Return value of this API.
 */
uint32_t pifTimer_Elapsed(PifTimer* p_owner);

/**
 * @fn pifTimer_AttachEvtFinish
 * @brief Attaches a callback, device, or external resource to the timer for integration.
 * @param p_owner Pointer to the target object instance.
 * @param evt_finish Callback invoked when timer operation completes.
 * @param p_issuer User context pointer passed to callback functions.
 * @param event_into_int TRUE runs the callback inside pifTimerManager_sigTick(), in the tick
 *        interrupt, so it must be short and interrupt safe. FALSE defers it to the timer task.
 */
void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer, BOOL event_into_int);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_H
