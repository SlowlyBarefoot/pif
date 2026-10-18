// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_timer_manager.h
 * @brief Group of software timers counted by one tick.
 *
 * The platform calls pifTimerManager_sigTick() every _period1us microseconds, usually from a hardware timer
 * interrupt, and every running timer of the group counts one tick.
 */
#ifndef PIF_TIMER_MANAGER_H
#define PIF_TIMER_MANAGER_H


#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"
#include "core/pif_timer.h"


/**
 * @struct StPifTimerManager
 * @brief Timers counted by one tick of _period1us microseconds.
 */
typedef struct StPifTimerManager
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	uint32_t _period1us;		///< Time of one tick in microseconds

	// Private Member Variable
    PifObjArray __timers;
	PifTaskTimer *__p_task;
	volatile BOOL __pending;
	volatile BOOL __ready;		///< The tick handler walks the timers only between Init and Clear.
} PifTimerManager;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates the timers of a manager whose tick is period1us, and adds the timer callback of the task
 *        manager that runs the deferred finish callbacks.
 * @param p_manager Pointer to the timer manager instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param period1us Time of one tick in microseconds, the period at which pifTimerManager_sigTick() is called.
 * @param max_count Number of timers the manager can hold.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifTimerManager_Init(PifTimerManager *p_manager, PifId id, uint32_t period1us, int max_count);

/**
 * @brief Stops every timer and frees them.
 * @param p_manager Pointer to the timer manager instance.
 */
void pifTimerManager_Clear(PifTimerManager *p_manager);

/**
 * @brief Adds a timer, stopped. Call it from the main context: the manager task walks the same list there
 *        unguarded.
 * @param p_manager Pointer to the timer manager instance.
 * @param type TT_ONCE or TT_REPEAT.
 * @return The timer, or NULL with pif_error set when the manager is full.
 */
PifTimer *pifTimerManager_Add(PifTimerManager *p_manager, PifTimerType type);

/**
 * @brief Marks a timer for removal. It stops counting and no finish callback runs for it from
 *        then on. The manager task frees its slot after the next tick, so the count drops only
 *        then. Safe to call from a finish callback, including one attached to run in the interrupt.
 * @param p_timer Pointer to the timer instance.
 */
void pifTimerManager_Remove(PifTimer *p_timer);

/**
 * @brief Gets the number of timers added.
 * @param p_manager Pointer to the timer manager instance.
 * @return Number of timers.
 */
int pifTimerManager_Count(PifTimerManager *p_manager);

/**
 * @brief Counts one tick on every running timer and finishes the ones that reach their target. Call it every
 *        period1us, usually from a timer interrupt.
 * @param p_manager Pointer to the timer manager instance.
 */
void pifTimerManager_sigTick(PifTimerManager *p_manager);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_MANAGER_H
