// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_PWM_GEN_H
#define PIF_PWM_GEN_H


#include "core/pif_timer_manager.h"


// Generates a software PWM output on a timer of a PifTimerManager. Not to be confused with
// rc/pif_rc_pwm, which reads a PWM input.

#ifndef PIF_PWM_GEN_MAX_DUTY
#define PIF_PWM_GEN_MAX_DUTY			1000
#endif


typedef void (*PifActPwmGen)(SWITCH value);


/**
 * @struct StPifPwmGen
 * @brief Software PWM output driven by a one-shot timer whose finish callback runs in the tick
 *        interrupt. Each callback switches the output and starts the timer for the next phase.
 */
typedef struct StPifPwmGen
{
	// Read-only Member Variable
	PifId _id;
	uint32_t _period;					// Period in ticks
	uint16_t _duty;						// On-time per PIF_PWM_GEN_MAX_DUTY
	BOOL _running;

	// Private Member Variable
	PifTimer* __p_timer;
	// The phases the tick handler counts. The main context writes them only while the timer is
	// stopped, since on an 8-bit target a 32-bit store takes several instructions.
	volatile uint32_t __on_time;
	volatile uint32_t __off_time;
	// The phases waiting for the next period. __update is cleared before they are written and set
	// after, so the tick handler takes them only when they are whole.
	volatile uint32_t __next_on_time;
	volatile uint32_t __next_off_time;
	volatile BOOL __update;
	BOOL __phase_on;

	// Private Action Function
	PifActPwmGen __act_pwm;
} PifPwmGen;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPwmGen_Init
 * @brief Initializes the PWM output and takes a timer from the timer manager for it.
 * @param p_owner Pointer to the target object instance.
 * @param id Identifier value for the object or task.
 * @param p_timer_manager Timer manager whose tick is the time base of the output.
 * @param act_pwm Callback that drives the output pin.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPwmGen_Init(PifPwmGen* p_owner, PifId id, PifTimerManager* p_timer_manager,
		PifActPwmGen act_pwm);

/**
 * @fn pifPwmGen_Clear
 * @brief Stops the output, leaving it off, and gives the timer back to the timer manager.
 * @param p_owner Pointer to the target object instance.
 */
void pifPwmGen_Clear(PifPwmGen* p_owner);

/**
 * @fn pifPwmGen_Start
 * @brief Starts the output with a new period. Each period begins with the on-time. The duty
 *        last set with pifPwmGen_SetDuty() is kept, and is 0 after pifPwmGen_Init().
 * @param p_owner Pointer to the target object instance.
 * @param period Period in ticks of the timer manager, greater than 0.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPwmGen_Start(PifPwmGen* p_owner, uint32_t period);

/**
 * @fn pifPwmGen_Stop
 * @brief Stops the output and leaves it off.
 * @param p_owner Pointer to the target object instance.
 */
void pifPwmGen_Stop(PifPwmGen* p_owner);

/**
 * @fn pifPwmGen_SetPeriod
 * @brief Changes the period and keeps the duty ratio. A running output finishes the period under
 *        way and uses the new one from the next.
 * @param p_owner Pointer to the target object instance.
 * @param period Period in ticks of the timer manager, greater than 0.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPwmGen_SetPeriod(PifPwmGen* p_owner, uint32_t period);

/**
 * @fn pifPwmGen_SetDuty
 * @brief Sets the on-time as a ratio of the period. A running output finishes the period under
 *        way and uses the new ratio from the next. At 0 or PIF_PWM_GEN_MAX_DUTY the output is held
 *        off or on and the timer does not run.
 * @param p_owner Pointer to the target object instance.
 * @param duty Duty ratio, 0 to PIF_PWM_GEN_MAX_DUTY. Larger values are taken as the maximum.
 */
void pifPwmGen_SetDuty(PifPwmGen* p_owner, uint16_t duty);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PWM_GEN_H
