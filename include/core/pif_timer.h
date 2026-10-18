// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_timer.h
 * @brief Software timer counted in ticks of a PifTimerManager.
 *
 * A timer runs once or repeatedly and calls its finish callback in the tick interrupt or, on request, in a task.
 */
#ifndef PIF_TIMER_H
#define PIF_TIMER_H


#include "core/pif_task.h"


/**
 * @brief Called when a timer reaches its target. p_issuer is the pointer given with the callback.
 */
typedef void (*PifEvtTimerFinish)(PifIssuerP p_issuer);


/**
 * @brief Whether a timer stops or starts over when it reaches its target.
 */
typedef enum EnPifTimerType
{
    TT_ONCE			= 0,	///< Stops at the target
    TT_REPEAT		= 1	///< Starts over at the target
} PifTimerType;

/**
 * @brief Run state of a timer.
 */
typedef enum EnPifTimerStep
{
    TS_STOP			= 0,	///< Stopped
    TS_RUNNING		= 1,	///< Counting
    TS_REMOVE		= 2	///< To be removed by the timer manager
} PifTimerStep;


/**
 * @struct StPifTimer
 * @brief One software timer, added with pifTimerManager_Add().
 */
typedef struct StPifTimer
{
	// Read-only Member Variable
	/// Set with pifTimer_Start() or pifTimer_SetTarget(). pifTimerManager_sigTick() reads it at every
	/// reload, and on an 8-bit target it could read a value written directly half way.
    volatile uint32_t _target;
    PifTimerType _type;	///< Runs once or repeatedly
    /// pifTimerManager_sigTick() counts only a timer in TS_RUNNING, so the main context takes a
    /// timer out of it while it rewrites what the handler reads.
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
 * @brief Starts the timer operation using the current timing, trigger, or mode configuration.
 * @param p_owner Pointer to the target object instance.
 * @param target Target tick count to run before completion.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimer_Start(PifTimer* p_owner, uint32_t target);

/**
 * @brief Stops the timer operation and transitions it into an inactive state.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Stop(PifTimer* p_owner);

/**
 * @brief Resets runtime state in the timer to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Reset(PifTimer* p_owner);

/**
 * @brief Changes the tick count of the timer. A running timer finishes its current period and
 *        uses the new count from the next reload, as pifTimer_Reset() and repeat timers do.
 * @param p_owner Pointer to the target object instance.
 * @param target Target tick count, greater than 0.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimer_SetTarget(PifTimer* p_owner, uint32_t target);

/**
 * @brief Gets the ticks left before a running timer reaches its target.
 * @param p_owner Pointer to the target object instance.
 * @return Ticks left, or 0 when the timer is not running.
 */
uint32_t pifTimer_Remain(PifTimer* p_owner);

/**
 * @brief Gets the ticks a running timer has counted since its start.
 * @param p_owner Pointer to the target object instance.
 * @return Ticks counted, or 0 when the timer is not running.
 */
uint32_t pifTimer_Elapsed(PifTimer* p_owner);

/**
 * @brief Sets the callback called when the timer reaches its target.
 * @param p_owner Pointer to the target object instance.
 * @param evt_finish Callback, or NULL.
 * @param p_issuer Pointer handed to the callback.
 * @param event_into_int TRUE runs the callback inside pifTimerManager_sigTick(), in the tick
 *        interrupt, so it must be short and interrupt safe. FALSE defers it to the timer task.
 */
void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer, BOOL event_into_int);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_H
